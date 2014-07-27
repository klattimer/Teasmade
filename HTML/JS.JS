String.prototype.reverse = function() {
    splitext = this.split("");
    revtext = splitext.reverse();
    reversed = revtext.join("");
    return reversed;
}

String.prototype.stripTags = function() {
    return this.replace(/(<([^>]+)>)/ig,""); 
}

String.prototype.stripScripts = function() {
    return this.replace(/(<script.*\/script>)/ig, "");
}

String.prototype.startsWith = function (str){
    return this.slice(0, str.length) == str;
};

String.prototype.endsWith = function (str){
    return this.slice(-str.length) == str;
};

HTMLElement.prototype.addClass = function(className) {
    if (this.className.search(className) == -1) {
        this.className = this.className + " " + className;
        this.className.replace(/\s{2,}/g, ' ');
        this.className = this.className.trim();
    }
}

HTMLElement.prototype.removeClass = function(className) {
    if (this.className.search(className) != -1) {
        this.className = this.className.replace(new RegExp(className+"\\b"), "").replace(new RegExp("  \\b"), " ");
        this.className.replace(/\s{2,}/g, ' ');
        this.className = this.className.trim();
    }
}

HTMLElement.prototype.hasClass = function(className) {
    if (this.className.search(className) != -1) return true;
    else return false;
}

HTMLElement.prototype.swapClass = function(oldclass, newclass) {
    if (this.hasClass(oldclass)) this.removeClass(oldclass);
    if (this.hasClass(newclass)) return;
    this.addClass(newclass);
}

HTMLElement.prototype.toggleClass = function(className) {
    if (this.hasClass(className)) this.removeClass(className);
    else this.addClass(className);
}

HTMLElement.prototype.absoluteLeft = function () {
    var l = 0;
    var obj = this
    if (obj.offsetParent) {
        while(1) {
            l += obj.offsetLeft;
            if(!obj.offsetParent)
                break;
            obj = obj.offsetParent;
        }
    } else if(this.x) {
        l += this.x;
    }
    return l;
}

HTMLElement.prototype.absoluteTop = function() {
    var t = 0;
    var obj = this;
    if(obj.offsetParent) {
        while(1) {
            t += obj.offsetTop;
            if(!obj.offsetParent)
                break;
            obj = obj.offsetParent;
        }
    } else if(this.y) {
        t += this.y;
    }
    return t;
}

Array.prototype.remove = function(obj) {
    var i;
    while((i = this.indexOf(obj)) !== -1) {
        this.splice(i, 1);
    }
}

Object.prototype.merge = function (otherObject) {
    for (i in otherObject) {
        try {
            if (this[i].constructor == Object) {
                this[i].merge(otherObject[i]);
            } else {
                this[i] = otherObject[i];
            }
        } catch (e) {
            this[i] = otherObject[i];
        }
    }
}

// javascript
// Author: Karl Lattimer
// Module: Ajax handler object
// 

function AJAXObject() {
    var that = this;
    if (window.ActiveXObject) {
        this.request = new ActiveXObject("Microsoft.XMLHTTP");
    } else if (window.XMLHttpRequest) {
        this.request = new XMLHttpRequest();
    }
    this.params = null;
    this.callback = null;
    this.url = null;
    this.post = false;
    this.async = true;
    this.responseType = null;
    
    this.send = function () {
        if (!this.url) return;
        var proto;
        if (this.url.startsWith("http://")) {
            proto = "http://";
        } else if (this.url.startsWith("https://")) {
            proto = "https://";
        } else {
            return;
        }
        if (!this.url.startsWith(proto + window.location.host)) {
            var str = proto + window.location.host;
            var path = this.url.slice(proto.length, this.url.length);
            path = path.slice(path.indexOf("/"), path.length);
            this.url = str + path;
        }
        if (this.responseType && !this.callback) return;
        if (this.post) {
            this.request.open("POST", this.url, this.async);
            this.request.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
            this.request.send(this.params);
        } else {
            if (this.params) url = this.url+"?"+this.params;
            else url = this.url;
            this.request.open("GET", url, this.async);
            this.request.send(null);
        }
    }
    
    this.addParameter = function (param, value) {
        value = encodeURIComponent(value);
        if (this.params == null) {
           this.params = param+"="+value;
        } else {
           this.params = this.params+"&"+param+"="+value;
        }
    }
    
    this.request.onreadystatechange = function () {
        if ((that.request.readyState == 4) && (that.request.status == 200)) {
            var response = that.request.responseText;
            if (that.responseType == null) {
                return;
            } else if (that.responseType == "nix") {
                that.callback();
            } else if (that.responseType == "int") {
                that.callback(parseInt(response.stripTags()));
            } else if (that.responseType == "text") {
                that.callback(response.stripTags());
            } else if (that.responseType == "html") {
                that.callback(response.stripScripts());
            } else if (that.responseType == "xml") {
                if (window.DOMParser) {
                    parser = new DOMParser();
                    response = parser.parseFromString(response,"text/xml");
                } else {
                    response = new ActiveXObject("Microsoft.XMLDOM");
                    response.async = "false";
                    response.loadXML(response); 
                }
                that.callback(response);
            } else if (that.responseType == "json") {
                if (response == "") {
                    that.callback(null);
                }
                try {
                    that.callback(eval('('+response+')'));
                } catch (e) {
                    that.callback(null);
                }
            } else if (that.callback) {
                that.callback();
            }
        }
    }
}

function checkStatus() {
    var status = new AJAXObject();
    status.url = "/STATUS";
    status.responseType = "json";
    status.callback = function(response) {
        
    };
    status.send();
}

function makeTea() {
    var tea = new AJAXObject();
    tea.url = "/METHODS/MAKETEA";
    tea.responseType = "json";
    tea.callback = function(response) {
        if (response.success == true) {
            setLights(false, "#0482F2");
        } else {
            flashRed(5, 300);
        }
    };
    tea.send();
}

function setLights(on, colour) {
    var lights = document.getElementById("lights");
    if (on) {
        lights.style.opacity = 0.5;
    } else {
        lights.style.opacity = 0;
    }
    lights.style.backgroundColor = colour;
    
    var lights = new AJAXObject();
    lights.url = "/METHODS/LIGHTS/0/"+colour;
    lights.responseType = "json";
    lights.callback = function(response) {
        
    };
    lights.send();
}

var lampOn = false;

function toggleLamp() {
    lampOn = !lampOn;
    if (lampOn) { 
        setLights(true, "#FFFF00");
    } else {
        setLights(false, "#FFFFFF");
    }
}

function flashRed(times, delay) {
    if (times == 0) return;
    var led = document.getElementById("leftbutton");
    led.addClass("illuminated");
    
    setTimeout(function () {
        led.removeClass("illuminated");
        setTimeout(function () { 
            flashRed(times - 1, delay);
        }, delay);
    }, delay);
}

function flashWhite(times, delay) {
    if (times == 0) return;
    var led = document.getElementById("rightbutton");
    led.addClass("illuminated");
    
    setTimeout(function () {
        led.removeClass("illuminated");
        setTimeout(function () { 
            flashWhite(times - 1, delay);
        }, delay);
    }, delay);
}

window.onload = function () {
    setInterval(updateClock, 1000);
    checkStatus();
    setInterval(checkStatus, 10000);
}

function updateClock() {
    var h = document.getElementById("hours");
    var m = document.getElementById('minutes');
    var s = document.getElementById('seconds');
    
    var date = new Date();
    var ch = date.getHours();
    var cm = date.getMinutes();
    var cs = date.getSeconds();
    
    var ans = (((cs / 60.0) * 360) + 90) % 360;
    var anm = (((cm / 60.0) * 360) + 90) % 360;
    
    var hadj = (cm / 60.0) * (360 / 12);
    var anh = (((ch / 12.0) * 360) + 90 + hadj) % 360;
    
    
    h.style.transform = "rotateZ("+anh+"deg)";
    m.style.transform = "rotateZ("+anm+"deg)";
    s.style.transform = "rotateZ("+ans+"deg)";
}