'use strict';


var localVideo = document.querySelector('#localVideo');
var remoteVideo = document.querySelector('#remoteVideo');
var localStream = null;


function openLocalStream(stream) {
    console.log('Open local stream');

    localVideo.srcObject = stream;
    localStream = stream;
}


function initLocalStream() {
    navigator.mediaDevices.getUserMedia({
        audio: true,
        video: true
    })
    .then(openLocalStream)
    .catch(function(e) {
        alert("getUserMedia() error: " + e.name);
    });
}

document.getElementById('joinBtn').onclick = function() {
    console.log("加入按钮被点击");3
    // 初始化本地码流
    initLocalStream();
}