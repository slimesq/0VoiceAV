'use strict';


var localVideo = document.querySelector('#localVideo');
var remoteVideo = document.querySelector('#remoteVideo');
var localStream = null;

var zeroRTCEngine;

var ZeroRTCEngine = function(wsUrl) {
    this.init(wsUrl);
    zeroRTCEngine = this;
    return this;
}

ZeroRTCEngine.prototype.init = function(wsUrl) {
    // 设置websocket  url
    this.wsUrl = wsUrl;
    /** websocket对象 */
    this.signaling = null;
}

ZeroRTCEngine.prototype.createWebsocket = function() {
    zeroRTCEngine = this;
    zeroRTCEngine.signaling = new WebSocket(this.wsUrl);

    zeroRTCEngine.signaling.onopen = function() {
        zeroRTCEngine.onOpen();
    }

    zeroRTCEngine.signaling.onmessage = function(ev) {
        zeroRTCEngine.onMessage(ev);
    }

    zeroRTCEngine.signaling.onerror = function(ev) {
        zeroRTCEngine.onError(ev);
    }

    zeroRTCEngine.signaling.onclose = function(ev) {
        zeroRTCEngine.onClose(ev);
    }
}

ZeroRTCEngine.prototype.onOpen = function() {
    console.log("websocket open");
}
ZeroRTCEngine.prototype.onMessage = function(event) {
    console.log("onMessage: " + event.data);
}

ZeroRTCEngine.prototype.onError = function(event) {
    console.log("onError: " + event.data);
}

ZeroRTCEngine.prototype.onClose = function(event) {
    console.log("onClose -> code: " + event.code + ", reason:" + EventTarget.reason);
}



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

zeroRTCEngine = new ZeroRTCEngine("ws://192.168.221.134:8099");
zeroRTCEngine.createWebsocket();

document.getElementById('joinBtn').onclick = function() {
    console.log("加入按钮被点击");
    // 初始化本地码流
    initLocalStream();
}