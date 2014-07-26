/* Teasmade
 *
 *
 * Copyright (c) Karl Lattimer
 * License: MIT
 *
 * This is the code for my Arduino driven Goblin Teasmade upcycle project.
 *
 * The teasmade is wired to a Arduino Mega with an Arduino WiFi shield and the following I/O configuration;
 *   - Analog clock mechanism coil side A  - D35
 *   - Analog clock mechanism coil side B  - D33
 *   - Left side LED (red)                 - D29
 *   - Left side button                    - D23
 *   - Right side LED (white)              - D31
 *   - Right side button                   - D25
 *   - Teapot microswitch (original mech)  - D18
 *   - Kettle microswitch (original mech)  - D19
 *
 * There are also 2 ghetto pixel (blinkm clones) connected to I2C and a DS1307 RTC to maintain the on-device
 * time.
 
 * ERROR Codes 
 *  Red Light Flashes, fast = 300ms delay, slow = 1000ms delay
 *
 *  - FAST 5 times = Can't make tea, either the kettle is empty or either the kettle or teapot is not present.
 *  - FAST 2 times = Failed to start the SD card reader
 *  - SLOW 10 times = Wifi shield can't be found
 *  - SLOW 3 times = Upgrade wifi shield firmware
 *
 * States
 *  - Kettle not present - Red light on
 *  - Teapot not present - White light on
 *
 * Button controls
 *  - Left side button   - Make tea
 *  - Right side button  - Turn main light on (ghetto pixels, uses a default colour)
 *
 *
 * Web interface
 *  - /METHODS/MAKETEA          - Make tea, returns JSON success
 *  - /STATUS                   - Return JSON for the current status
 *  - /METHODS/LIGHT/i/#rrggbb  - Set ghetto pixel (i 0 or 1) to a specific colour
 *  - /METHODS/TIME/???         - Set the time
 *  - /METHODS/FACETIME/???     - Set the face to the correct time by telling it the current time
 *  - /METHODS/FACENUDGE/i      - Nudge the face time by x number of seconds
 *
 *  Files stored on the WiFi shield SD card will be served up as HTML/JS/CSS to the client. 
 */

#include <SPI.h>
#include <WiFi.h>
#include <String.h>
#include <SD.h>
#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>

#include "Wire.h"
#include "BlinkM_funcs.h"

const boolean BLINKM_ARDUINO_POWERED = true;
byte blinkm_addr_a = 0x09;
byte blinkm_addr_b = 0x0A;


char rpath[128];

int status = WL_IDLE_STATUS;
int SD_CHIPSELECT  = 4;

#define PIN_CLOCK_A                    35
#define PIN_CLOCK_B                    33

#define PIN_LIGHT_BUTTON_LEFT_RED      29
#define PIN_LIGHT_BUTTON_RIGHT_WHITE   31
#define PIN_RELAY                      27

#define PIN_SWITCH_TEAPOT_ENGAGED      18
#define PIN_SWITCH_KETTLE_ENGAGED      19
#define PIN_SWITCH_KETTLE_DISENGAGED   37

#define PIN_BUTTON_LEFT                23
#define PIN_BUTTON_RIGHT               25

#define OFF false
#define ON true

WiFiServer server(80);


const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
tmElements_t tm;

typedef enum kButtons {
  BUTTON_LEFT = 1,
  BUTTON_RIGHT = 2,
  KETTLE_PRESENT = 4,
  TEAPOT_PRESENT = 8
} kButton;


char ssid[] = "WIFISSID";
char pass[] = "WIFIPASS";

void softwareReset() {
  asm volatile ("  jmp 0");  
}

void scanfunc( byte addr, byte result ) {
  Serial.print(F("addr: "));
  Serial.print(addr,DEC);
  Serial.print( (result==0) ? " found!":"       ");
  Serial.print( (addr%4) ? "\t":"\n");
}

void lookForBlinkM() {
  Serial.print(F("Looking for a BlinkM: "));
  int a = BlinkM_findFirstI2CDevice();
  if( a == -1 ) {
    Serial.println(F("No I2C devices found"));
  } else { 
    Serial.print(F("Device found at addr "));
    Serial.println( a, DEC);
    blinkm_addr_a = a;
  }
}


bool getTime(const char *str)
{
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  tm.Hour = Hour;
  tm.Minute = Min;
  tm.Second = Sec;
  return true;
}

bool getDate(const char *str)
{
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  if (sscanf(str, "%s-%d-%d", Month, &Day, &Year) != 3) return false;
  for (monthIndex = 0; monthIndex < 12; monthIndex++) {
    if (strcmp(Month, monthName[monthIndex]) == 0) break;
  }
  if (monthIndex >= 12) return false;
  tm.Day = Day;
  tm.Month = monthIndex + 1;
  tm.Year = CalendarYrToTm(Year);
  return true;
}



bool kettlePresent = false;
bool makingTea = false;
bool teasmade = false;
bool teapot = false;
bool relay = false;

// Println during an interrupt causes a crash. It would be nice to get those messages though
void kettleInterrupt() {
  int kettle = digitalRead(PIN_SWITCH_KETTLE_ENGAGED);
  // Decided against the disengage switch as it's not entirely reliable 
  //int kettleD = digitalRead(PIN_SWITCH_KETTLE_DISENGAGED);
  
  if (!kettle) kettlePresent = true;
  else kettlePresent = false;
  
  // If the kettle is not present or water is not present ensure the relay is switched off
  if (kettlePresent == false) {
    //Serial.println("Kettle removed");
    setRelay(OFF);
    if (makingTea) {
      //Serial.println("Cancelling making tea");
      teasmade = true; 
      makingTea = false;
    } else {
      //Serial.println("Kettle replaced");
    }
  }
}
void teapotInterrupt() {
  teapot = digitalRead(PIN_SWITCH_TEAPOT_ENGAGED);
  // If the teapot is not present ensure the relay is switched off
  if (teapot == true) {
    //Serial.println("Teapot removed");
    setRelay(OFF);
  } else {
    //Serial.println("Teapot replaced");
  }
  teasmade = false;
  makingTea = false;
}

void setRelay(bool relaySetting) {
  //Serial.print("Relay changing state to ");
  //Serial.println(relaySetting);
  if (relaySetting == OFF) {
    relay = false;
    digitalWrite(PIN_RELAY, HIGH);
    return;
  }
  
  int buttons = readButtons();
  if ((buttons & KETTLE_PRESENT) && (buttons & TEAPOT_PRESENT)) {
    digitalWrite(PIN_RELAY, LOW);
    relay = true;
    makingTea = true;
  } else {
    flashRed(5, 300);
  }
}

bool redOn;
bool whiteOn;

void setRed(bool light) {
  if (light == ON) {
    digitalWrite(PIN_LIGHT_BUTTON_LEFT_RED, HIGH);
    redOn = true;
  } else {
    digitalWrite(PIN_LIGHT_BUTTON_LEFT_RED, LOW);
    redOn = false;
  }
}

void flashRed(int times, int delayTime) {
  int i;
  for (i = 0 ; i < times ; i++ ) {
    setRed(ON);
    delay(delayTime);
    setRed(OFF);
  }
}

void setWhite(bool light) {
  if (light == ON) {
    digitalWrite(PIN_LIGHT_BUTTON_RIGHT_WHITE, HIGH);
    whiteOn = true;
  } else {
    digitalWrite(PIN_LIGHT_BUTTON_RIGHT_WHITE, LOW);
    whiteOn = false;
  }
}

void flashWhite(int times, int delayTime) {
  int i;
  for (i = 0 ; i < times ; i++ ) {
    setWhite(ON);
    delay(delayTime);
    setWhite(OFF);
  }
}

byte left_r, left_g, left_b;
byte right_r, right_g, right_b;

void setLeft(byte r, byte g, byte b) {
  left_r = r;
  left_g = g;
  left_b = b;
  BlinkM_fadeToRGB( blinkm_addr_a, r,g,b);
}

void setRight(byte r, byte g, byte b) {
  right_r = r;
  right_g = g;
  right_b = b;
  BlinkM_fadeToRGB( blinkm_addr_a, r,g,b);
}

bool makeTea() {
  Serial.println(F("Make Tea!"));
  setRelay(ON);
  return relay;
}

int readButtons() {
  int left = digitalRead(PIN_BUTTON_LEFT);
  int right = digitalRead(PIN_BUTTON_RIGHT);
  
  int kettle = digitalRead(PIN_SWITCH_KETTLE_ENGAGED);
  int teapot = digitalRead(PIN_SWITCH_TEAPOT_ENGAGED);
  
  // Combine button values into return value
  
  int buttons = 0;
  if (left) {
    buttons = buttons | (1 << 0);
  }
  if (right) {
    buttons = buttons | (1 << 1);
  }
  if (!kettle) {
    buttons = buttons | (1 << 2);
  }
  if (!teapot) {
    buttons = buttons | (1 << 3);
  } 
  
  return buttons;
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}

void PrintHex8(WiFiClient client, byte data) // prints 8-bit data in hex with leading zeroes
{
    if (data < 0x10) {
      Serial.print("0"); 
    }
    client.print(data, HEX);
}

void setup() {
  Serial.begin(9600); 
  Serial.println(F("Teasmade Starting up"));
  
  pinMode(PIN_RELAY, OUTPUT);
  setRelay(OFF);
  
  pinMode(PIN_LIGHT_BUTTON_LEFT_RED, OUTPUT);
  pinMode(PIN_LIGHT_BUTTON_RIGHT_WHITE, OUTPUT);
  setRed(OFF);
  setWhite(OFF);
  
  pinMode(PIN_SWITCH_TEAPOT_ENGAGED, INPUT);
  pinMode(PIN_SWITCH_KETTLE_ENGAGED, INPUT);
  pinMode(PIN_SWITCH_KETTLE_DISENGAGED, INPUT);
  
  attachInterrupt(4, kettleInterrupt, CHANGE);
  attachInterrupt(5, teapotInterrupt, CHANGE);
  
  pinMode(PIN_BUTTON_LEFT, INPUT);
  pinMode(PIN_BUTTON_RIGHT, INPUT);
  
  pinMode(PIN_CLOCK_A, OUTPUT);
  pinMode(PIN_CLOCK_B, OUTPUT);
  
  setWhite(ON);
  if( BLINKM_ARDUINO_POWERED )
    BlinkM_beginWithPower();
  else
    BlinkM_begin();
  delay(300); // wait a bit for things to stabilize
  lookForBlinkM();
  BlinkM_off(0);  // turn everyone off
  
  tmElements_t tm;
  Serial.println(F("Initialising RTC"));
  if (RTC.read(tm)) {
    Serial.print(F("RTC:OK Time: "));
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    Serial.print(F(", Date: "));
    Serial.print(tm.Day);
    Serial.write('/');
    Serial.print(tm.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.println();
  } else {
    if (RTC.chipPresent()) {
      Serial.println(F("RTC:ERROR The DS1307 is stopped. Please set time."));
    } else {
      Serial.println(F("RTC:ERROR Read error! Please check the circuitry."));
    }
  }

  Serial.println(F("Initialising SD Card"));
  if (!SD.begin(SD_CHIPSELECT)) {
    setWhite(OFF);
    Serial.println(F("Failed to read SD card"));
    flashRed(2, 300);
  }
  
  Serial.println(F("Initialising Wifi"));
  setWhite(ON);
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    setWhite(OFF);
    Serial.println(F("WiFi shield not present"));
    flashRed(10, 1000);
  } else {
    Serial.println(F("Starting connection"));
    
    String fv = WiFi.firmwareVersion();
    if( fv != "1.1.0" ) {
      Serial.println(F("Please upgrade the firmware"));
      setWhite(OFF);
      flashRed(3, 1000);
    }
    // attempt to connect to Wifi network:
    while ( status != WL_CONNECTED) {
      // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
      Serial.println(F("Connecting"));
      status = WiFi.begin(ssid, pass);
      // wait 10 seconds for connection:
      delay(10000);
    }
  }
  setWhite(OFF);
  setRed(OFF);
  server.begin();
  printWifiStatus();
}

void decodeRequest(String request) {
    String buf;
    int i = 0,j = request.indexOf(' ');
    // Break out the first part of the request, upto the first space => r.method
    
    i = j + 1;
    // Detect if the URL is or isn't encoded (look for http:// in the string)
    if (request.indexOf("http://") >= 0) {
        // Break out the second part of the request, from the end of http:// to the next / => r.host
        j = request.indexOf('/', i + 7);
        //buf = request.substring(i, j);
        i = j;
    }
    // The next part of the request consitutes the path and query string
    j = request.indexOf('?', i);
    if (j < 0) {
        j =  j = request.indexOf(' ', i);
    }
    buf = request.substring(i+1, j);
    buf.toUpperCase();
    buf.toCharArray(rpath, 128);
}

boolean serveFile (WiFiClient client) {
    String contentType = "text/plain";
    String path = String(rpath);
    
    if (path.length() == 0) { 
      path = String("INDEX.HTM");
    }
    char buf[13];
    path.toCharArray(buf, 13);
    File dataFile = SD.open(buf);
    
    
    int es = path.indexOf('.', path.length() - 6) + 1;
    int ee = path.length();
    String extension = path.substring(es, ee);
    extension.toLowerCase();
    
    if (extension == "jpg") {
        contentType = String("image/jpeg");
    } else if (extension == "png") {
        contentType = String("image/png");
    } else if (extension == "js") {
        contentType = String("text/javascript");
    } else if (extension == "css") {
        contentType = String("text/css");
    } else if (extension == "htm") {
        contentType = String("text/html");
    } else if (extension == "jsn") {
        contentType = String("text/json");
    }
    
    // if the file is available, write to it:
    if (dataFile) {
       if (path == "404.HTM") {
         client.print(F("HTTP/1.1 404 OK\r\nContent-Type: "));
       } else {
         client.print(F("HTTP/1.1 200 OK\r\nContent-Type: "));
       }
       client.println(contentType);
       client.println(F("Connection: close"));
       client.println();
       while (dataFile.available()) {
         client.write(dataFile.read());
       }
       dataFile.close();
       return true;
    }
    return false;
}

boolean handleAction(WiFiClient client) {
  
  String path = String(rpath);
  
  if (path == "METHODS/MAKETEA") {
    // Make tea if we're good to make tea!
    bool success = makeTea();
    client.println(F("HTTP/1.1 200 OK\n\rContent-Type: text/json\n\rConnection: close\n"));
    client.println(F("{ \"success\" : "));
    if (success) client.println (F("true }"));
    else client.println (F("false }"));
    return true;
  }  else if (path.startsWith("METHODS/TIME")) {
    // Set the time in the RTC
    String datetime = path.substring(13);
    String date = datetime.substring(0, datetime.indexOf('/'));
    String time = datetime.substring(datetime.indexOf('/')+1);
    
    char t[time.length()];
    time.toCharArray(t, time.length());
    
    char d[date.length()];
    date.toCharArray(d, date.length());
    
    if (getDate(d) && getTime(t)) {
      RTC.write(tm);
    }
  } else if (path.startsWith("METHODS/FACETIME")) {
    // TODO: Calculate the necessary functions to set the clock face to an accurate time.
    // Needs to be told what time the face reads at present. 
    
  } else if (path.startsWith("METHODS/FACENUDGE")) {
    // TODO: Nudge the face time by a number of seconds
    
  } else if (path.startsWith("METHODS/LIGHT")) {
     // Get the colour and light from the path
     String cmd = path.substring(14);
     byte r, g, b;
     String color = cmd.substring(2);
     char c[6];
     color.toCharArray(c, 6);
     char red[3];
     red[0] = c[0];
     red[1] = c[1];
     red[2] = '\0';
     char green[3];
     green[0] = c[2];
     green[1] = c[3];
     green[2] = '\0';
     char blue[3];
     blue[0] = c[4];
     blue[1] = c[3];
     blue[2] = '\0';
     unsigned long int x = strtoul(red, NULL, 8);
     r = (byte)x;
     x = strtoul(green, NULL, 8);
     g = (byte)x;
     x = strtoul(blue, NULL, 8);
     b = (byte)x;
     if (cmd.startsWith("0")) {
       setLeft(r,g,b);
     } else {
       setRight(r,g,b);
     }
  } else if (path == "STATUS") {
    // Print the current status
    client.println(F("HTTP/1.1 200 OK\n\rContent-Type: text/json\n\rConnection: close\n"));
    client.print(F("{\"kettlePresent\" : "));
    if (kettlePresent)
      client.print("true, ");
    else
      client.print("false, ");
    client.print(F("\"teapotPresent\" : "));
    if (teapot)
      client.print(F("true, "));
    else
      client.print(F("false, "));
      
    client.print(F("\"makingTea\" : "));
    if (makingTea)
      client.print(F("true, "));
    else
      client.print(F("false,"));
    client.print(F("\"leftColour\" : \"#"));
    PrintHex8(client, left_r);
    PrintHex8(client, left_g);
    PrintHex8(client, left_b);
    client.print(F("\", \"rightColour\" : \"#"));
    PrintHex8(client, right_r);
    PrintHex8(client, right_g);
    PrintHex8(client, right_b);
    
    client.println(F("\" }"));
    return true;
  }
  
  client.println(F("HTTP/1.1 200 OK\n\rContent-Type: text/plain\n\rConnection: close\n"));
  client.println(F("ok"));
  
  return true;
}


bool clock = false;
bool lights = false;

// 15:33 shows 12:30

void loop() {
  int buttons = readButtons();
  if (buttons & BUTTON_LEFT) {
    makeTea(); 
    delay(600);
  }
  
  if (buttons & BUTTON_RIGHT) {
    if (!lights) {
      setLeft(0xff,0xD1,0xA0);
      lights = true;
      delay(600);
    } else {
      lights = false;
      setLeft(0x00,0x00,0x00);
      delay(600);
    }
    buttons = readButtons();
    if (buttons & BUTTON_RIGHT) {
      // If we're still pressing the button wait 4 seconds total
      delay(3400);
      buttons = readButtons();
      if (buttons & BUTTON_RIGHT) {
        softwareReset();
      }
    }
  }
  
  if (!(buttons & KETTLE_PRESENT)) {
    setRed(ON); 
  } else {
    setRed(OFF);
  }
  
  if (!(buttons & TEAPOT_PRESENT)) {
    setWhite(ON); 
  } else {
    setWhite(OFF);
  }
 
  if (makingTea) {
    setLeft(0x00, 0x00, 0xFF); 
  }
  if (teasmade) {
    setLeft(0x00, 0xFF, 0x00);
  }
  
  /*if (clock == true) {
    clock = false;
    digitalWrite(PIN_CLOCK_A, LOW);
    digitalWrite(PIN_CLOCK_B, HIGH);
    Serial.println("clock a");
    delay(40);
    // delay(62); 
  } else {
    digitalWrite(PIN_CLOCK_A, HIGH);
    digitalWrite(PIN_CLOCK_B, LOW);
    Serial.println("clock b");
    clock = true;
    delay(40);
    // delay(63);
  }
  return;*/
  
    WiFiClient client = server.available();
    if (client) {
        boolean currentLineIsBlank = true;
        String request = "";
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                request += c;
                if (c == '\n' && currentLineIsBlank) {
                    // Try to serve a file straight from the SD card
                    byte i = request.indexOf('\n');
                    byte j = request.indexOf('\r');
                    if (j < i) { 
                      i = j;
                    }
                    String r = request.substring(0,i);
                    if (r.startsWith("BREW") || request.startsWith("WHEN")) {
                     client.print(F("HTTP/1.1 418 OK\r\nContent-Type: "));
                     break;
                    }
                    decodeRequest(r);
                    bool served = serveFile(client);
                    // Here's where you can do some custom stuff to handle the request
                    if (!served) {
                        // First we try to handle the action
                        served = handleAction(client);
                    }
                    /*if (!served) {
                        // Otherwise we try to construct some data
                        served = dataResponse(client, request);
                    }*/
                    
                    if (!served) {
                      String fof = "404.HTM";
                      fof.toCharArray(rpath, 128);
                      served = serveFile(client);
                    }
                    
                    break;
                }
                if (c == '\n') {
                    currentLineIsBlank = true;
                } else if (c != '\r') {
                    currentLineIsBlank = false;
                }
            }
        }
        delay(1);
        client.stop();
    }
}
