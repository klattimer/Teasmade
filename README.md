Teasmade
========

Copyright (c) Karl Lattimer
License: MIT

This is the code for my Arduino driven Goblin Teasmade upcycle project.

The teasmade is wired to a Arduino Mega with an Arduino WiFi shield and the following I/O configuration;
 - Analog clock mechanism coil side A  - D35
 - Analog clock mechanism coil side B  - D33
 - Left side LED (red)                 - D29
 - Left side button                    - D23
 - Right side LED (white)              - D31
 - Right side button                   - D25
 - Teapot microswitch (original mech)  - D18
 - Kettle microswitch (original mech)  - D19

There are also 2 ghetto pixel (blinkm clones) connected to I2C and a DS1307 RTC to maintain the on-device
time.
 
ERROR Codes 
-----------

Red Light Flashes, fast = 300ms delay, slow = 1000ms delay

 - FAST 5 times = Can't make tea, either the kettle is empty or either the kettle or teapot is not present.
 - FAST 2 times = Failed to start the SD card reader
 - SLOW 10 times = Wifi shield can't be found
 - SLOW 3 times = Upgrade wifi shield firmware
 
States
------

 - Kettle not present - Red light on
 - Teapot not present - White light on
 
Button controls
---------------

 - Left side button   - Make tea
 - Right side button  - Turn main light on (ghetto pixels, uses a default colour)


Web interface
--------------
 - /METHODS/MAKETEA          - Make tea, returns JSON success
 - /STATUS                   - Return JSON for the current status
 - /METHODS/LIGHT/i/#rrggbb  - Set ghetto pixel (i 0 or 1) to a specific colour
 - /METHODS/TIME/???         - Set the time
 - /METHODS/FACETIME/???     - Set the face to the correct time by telling it the current time
 - /METHODS/FACENUDGE/i      - Nudge the face time by x number of seconds
 
Files stored on the WiFi shield SD card will be served up as HTML/JS/CSS to the client. 
