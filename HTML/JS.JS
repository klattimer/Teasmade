window.onload = function () {
    updateClock();
    setInterval(updateClock, 1000);
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
    
    
    h.style.transform = "rotate("+anh+"deg)";
    console.log("Hour angle = "+anh);
    
    m.style.transform = "rotate("+anm+"deg)";
    console.log("Minutes angle = "+anm);
    
    s.style.transform = "rotate("+ans+"deg)";
    console.log("Seconds angle = "+ans);
}