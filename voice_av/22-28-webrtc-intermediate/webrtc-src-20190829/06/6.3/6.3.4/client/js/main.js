'use strict';

// join 主动加入房间
// leave 主动离开房间
// new-peer 有人加入房间，通知已经在房间的人
// peer-leave 有人离开房间，通知已经在房间的人
// offer 发送offer给对端peer
// answer发送offer给对端peer
// candidate 发送candidate给对端peer
const SIGNAL_TYPE_JOIN = "join";
const SIGNAL_TYPE_RESP_JOIN = "resp-join";  // 告知加入者对方是谁
const SIGNAL_TYPE_LEAVE = "leave";
const SIGNAL_TYPE_NEW_PEER = "new-peer";
const SIGNAL_TYPE_PEER_LEAVE = "peer-leave";
const SIGNAL_TYPE_OFFER = "offer";
const SIGNAL_TYPE_ANSWER = "answer";
const SIGNAL_TYPE_CANDIDATE = "candidate";


var localUserId = Math.random().toString(36).substr(2); // 本地uid
var remoteUserId = -1;      // 对端
var roomId = 0;

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

    var jsonMsg = JSON.parse(event.data);
    switch(jsonMsg.cmd) {
        case SIGNAL_TYPE_NEW_PEER:
            handleRemoteNewPeer(jsonMsg);
            break;
        case SIGNAL_TYPE_RESP_JOIN:
            handleResponseJoin(jsonMsg);
            break;
    }
}

ZeroRTCEngine.prototype.onError = function(event) {
    console.log("onError: " + event.data);
}

ZeroRTCEngine.prototype.onClose = function(event) {
    console.log("onClose -> code: " + event.code + ", reason:" + EventTarget.reason);
}

ZeroRTCEngine.prototype.sendMessage = function(message) {
    this.signaling.send(message);
}

function handleResponseJoin(message) {
    console.info("handleResponseJoin, remoteUid: " + message.remoteUid);
    remoteUserId = message.remoteUid;
    // doOffer();
}

function handleRemoteNewPeer(message) {
    console.info("handleRemoteNewPeer, remoteUid: " + message.remoteUid);
    remoteUserId = message.remoteUid;
    // doOffer();
}

function doJoin(roomId) {
    var jsonMsg = {
        'cmd': 'join',
        'roomId': roomId,
        'uid': localUserId,
    }; 
    var message = JSON.stringify(jsonMsg);
    zeroRTCEngine.sendMessage(message);
    console.info("doJoin message: " + message);
}

function openLocalStream(stream) {
    console.log('Open local stream');
    doJoin(roomId);
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
    roomId = document.getElementById('zero-roomId').value;
    if( roomId == "" || roomId == "请输入房间ID") {
        alert("请输入房间ID");
        return;
    }
    console.log("加入按钮被点击, roomId: " + roomId);
    // 初始化本地码流
    initLocalStream();
}