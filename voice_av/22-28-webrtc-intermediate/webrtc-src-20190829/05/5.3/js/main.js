'use strict';

const MESSAGE_TYPE_OFFER = 0x01;
const MESSAGE_TYPE_ANSWER = 0x02;
const MESSAGE_TYPE_CANDIDATE = 0x03;
const MESSAGE_TYPE_HANGUP = 0x04;

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
// toString(36)数字到字符串的转换的基数(从2到36);  为什么去提取？
var localUserId = Math.random().toString(36).substr(2); // store local userId
var remoteUserId = -1;
var localStream; // local video stream object
var pc = null; // webrtc RTCPeerConnection
var isInitor = false;

/////////////////////////////////////////////

var roomId = '100';

var zeroRTCEngine;

var ZeroRTCEngine = function (wsUrl) {
    this.init(wsUrl);
    zeroRTCEngine = this;
    return this;
}

ZeroRTCEngine.prototype.init = function (wsUrl) {
    // 设置websocket  url
    this.wsUrl = wsUrl;
    /** websocket对象 */
    this.signaling = null;
}

ZeroRTCEngine.prototype.createWebsocket = function () {
    var zeroRTCEngine = this;
    zeroRTCEngine.signaling = new WebSocket(this.wsUrl);
    zeroRTCEngine.signaling.onopen = function () {
        zeroRTCEngine.onOpen();
    };
    zeroRTCEngine.signaling.onmessage = function (ev) {
        zeroRTCEngine.onMessage(ev);
    };
    zeroRTCEngine.signaling.onerror = function (ev) {
        console.error("连接websocket失败请重试, msg:" + ev);
        zeroRTCEngine.onError(ev);
    };
    zeroRTCEngine.signaling.onclose = function (ev) {
        zeroRTCEngine.onClose(ev);
    };
};

ZeroRTCEngine.prototype.sendJsonMessage = function (parameters) {
    var message = JSON.stringify(parameters);
    this.signaling.send(message);
};
ZeroRTCEngine.prototype.sendMessage = function (message) {
    this.signaling.send(message);
};

ZeroRTCEngine.prototype.onOpen = function () {
    console.info('websocket open');
}

function parseJSON(json) {
    try {
        return JSON.parse(json);
    } catch (e) {
        console.error("Error parsing json: " + json);
    }
    return null;
}

ZeroRTCEngine.prototype.onMessage = function (event) {
    console.info("onMessage: " + event.data);
    var message = parseJSON(event.data);
    if(message == null) {
        console.error("parse msg:" + message + " failed");
        return;
    }
    switch (message.cmd) {
        case SIGNAL_TYPE_RESP_JOIN:
            handleResponseJoin(message);
            return;
        case SIGNAL_TYPE_NEW_PEER:  // 其他人进来
            handleRemoteNewPeer(message);
            return;
        case SIGNAL_TYPE_PEER_LEAVE:  // 其他人进来
            handleRemotePeerLeave(message);
            return;
        case SIGNAL_TYPE_OFFER:
            handleRemoteOffer(message);
            return;
        case SIGNAL_TYPE_ANSWER:
            handleRemoteAnswer(message);
            return;
        case SIGNAL_TYPE_CANDIDATE:
            handleRemoteCandidate(message);
            return;  
        case "hangup":
            handleRemoteHangup(message);
            return;       
        default:
            console.warn('Event ' + message.cmd);
    }
};
/**
 * onClose
 *
 */
ZeroRTCEngine.prototype.onClose = function (ev) {
    var ecerRTCEnv = this;
    console.warn('websocket close', ev);
    if (ev.code == 1000 && ev.reason == 'wsForcedClose') { // 如果自定义关闭ws连接，避免二次重连
        return;
    }
};
/**
 * onError
 *
 */
ZeroRTCEngine.prototype.onError = function (ev) {
    console.error('websocket error', ev);
};

function handleResponseJoin(message) {
    console.info("handleResponseJoin, msg: " + message);
    remoteUserId = message.remoteUid; // 保存新人id
}

function handleRemoteNewPeer(message) {
    console.info("handleRemoteNewPeer, msg: " + message);
    remoteUserId = message.remoteUid; // 保存新人id
    doOffer();
}

function handleRemotePeerLeave(message) {
    console.info("handleRemotePeerLeave, msg: " + message);
    if (pc != null) {
        pc.close();
        pc = null;
    }
    remoteVideo.srcObject = null;
}

function handleRemoteOffer(message) {
    console.log('Remote offer received: ', message.msg);
    if (pc == null) {
        createPeerConnection()
    }
    let desc = JSON.parse(message.msg);

    pc.setRemoteDescription(desc);
    doAnswer();
}

function handleRemoteAnswer(message) {
    console.log('Remote answer received: ', message.msg);
    let desc = JSON.parse(message.msg);
    
    pc.setRemoteDescription(desc);
}

function handleRemoteCandidate(message) {
    console.log('Remote candidate received: ', message.msg);

    var desc = JSON.parse(message.msg);
    var desc2 = {
        'sdpMLineIndex': desc.label,
        'sdpMid': desc.id,
        'candidate': desc.candidate
    };
    var candidate = new RTCIceCandidate(desc2);
    pc.addIceCandidate(candidate);
}

function handleRemoteHangup() {
    console.log('Remote hangup received');
    if (pc != null) {
        pc.close();
        pc = null;
    }
}



var localVideo = document.querySelector('#localVideo');
var remoteVideo = document.querySelector('#remoteVideo');

function initLocalStream(){
    navigator.mediaDevices.getUserMedia({
        audio: true,
        video: true
    })
        .then(openLocalStream)
        .catch(function (e) {
            alert('getUserMedia() error: ' + e.name);
        });
}

function openLocalStream(stream) {
    doJoin(roomId);

    console.log('Open local video stream');
    localVideo.srcObject = stream;
    localStream = stream;
}

function createPeerConnection() {
    try {
        var defaultConfiguration = {  
            bundlePolicy: "max-bundle",
            rtcpMuxPolicy: "require",
            iceTransportPolicy:"relay",//relay
            // 修改ice数组测试效果，需要进行封装
            iceServers: [
                {
                    "urls": [
                        "turn:129.204.197.215:3478?transport=udp",
                        "turn:129.204.197.215:3478?transport=tcp"       // 可以插入多个进行备选
                    ],
                    "username": "lqf",
                    "credential": "123456"
                },
                {
                    "urls": [
                        "stun:129.204.197.215:3478"
                    ]
                }
            ]
        };

        pc = new RTCPeerConnection(defaultConfiguration);
        pc.onicecandidate = handleIceCandidate;
        pc.onaddstream = handleRemoteStreamAdded;
        pc.onremovestream = handleRemoteStreamRemoved;
        pc.addStream(localStream);
        console.log('RTCPeerConnnection Created');
    } catch (e) {
        console.log('Failed to create PeerConnection, exception: ' + e.message);
        alert('Cannot create RTCPeerConnection object.');
        return;
    }
}

/////////////////////////////////////////////////////////

// 加入房间
function doJoin(roomId) {
    var jsonMsg = {
        'cmd': SIGNAL_TYPE_JOIN,
        'roomId': roomId,
        'uid': localUserId,
    };
    var message = JSON.stringify(jsonMsg);
    zeroRTCEngine.sendMessage(message);
    console.log("send join: " + message);
}

// 离开房间
function doLeave(roomId) {
    var jsonMsg = {
        'cmd': SIGNAL_TYPE_LEAVE,
        'roomId': roomId,
        'uid': localUserId,
        'remoteUid': remoteUserId
    };
    var message = JSON.stringify(jsonMsg);
    zeroRTCEngine.sendMessage(message);
    console.log("send leave: " + message);
    hangup();
}
// 创建offer并发送
function doOffer() {
    console.log('Starting offer: Sending offer to remote peer');
    if (pc == null) {
        createPeerConnection()
    }
    pc.createOffer(createOfferAndSendMessage, handleCreateOfferError);
}

// 创建answer并发发送
function doAnswer() {
    console.log('Starting answer: Sending answer to remote peer');
    if (pc == null) {
        createPeerConnection()
    }
    pc.createAnswer().then(createAnswerAndSendMessage, handleCreateAnswerError);
}

function createOfferAndSendMessage(sessionDescription) {
    console.log('CreateOfferAndSendMessage sending message', sessionDescription);
    pc.setLocalDescription(sessionDescription);
    var jsonMsg = {
        'cmd': SIGNAL_TYPE_OFFER,
        'roomId': roomId,
        'uid': localUserId,
        'remoteUid':remoteUserId,
        'msg': JSON.stringify(sessionDescription)
    };
    var message = JSON.stringify(jsonMsg);
    zeroRTCEngine.sendMessage(message);
    
    console.log('send offer:', message);
}

function createAnswerAndSendMessage(sessionDescription) {
    console.log('CreateAnswerAndSendMessage sending message', sessionDescription);
    pc.setLocalDescription(sessionDescription);
    var jsonMsg = {
        'cmd': SIGNAL_TYPE_ANSWER,
        'roomId': roomId,
        'uid': localUserId,
        'remoteUid':remoteUserId,
        'msg': JSON.stringify(sessionDescription)
    };
    var message = JSON.stringify(jsonMsg);
    zeroRTCEngine.sendMessage(message);
    
    console.log('send answer:', message);
}

function handleCreateOfferError(event) {
    console.error('CreateOffer() error: ', event);
}

function handleCreateAnswerError(error) {
    console.error('CreateAnswer() error: ', error);
}

function handleIceCandidate(event) {
    console.log('Handle ICE candidate event: ', event);
    if (event.candidate) {
        var candidateJson = {
            'label': event.candidate.sdpMLineIndex,
            'id': event.candidate.sdpMid,
            'candidate': event.candidate.candidate
        };
        var jsonMsg = {
            'cmd': SIGNAL_TYPE_CANDIDATE,
            'roomId': roomId,
            'uid': localUserId,
            'remoteUid':remoteUserId,
            'msg': JSON.stringify(candidateJson) 
        };
        var message = JSON.stringify(jsonMsg);
        zeroRTCEngine.sendMessage(message);
        console.log('send candidate:', message);
    } else {
        console.log('End of candidates.');
    }
}

function handleRemoteStreamAdded(event) {
    console.log('Handle remote stream added.');
    remoteVideo.srcObject = event.stream;
}

function handleRemoteStreamRemoved(event) {
    console.log('Handle remote stream removed. Event: ', event);
    remoteVideo.srcObject = null;
}

function hangup() {
    console.log('Hanging up !');
    remoteVideo.srcObject = null;
    localVideo.srcObject = null;
    if (pc != null) {
        pc.close();
        pc = null;
    }
}

/////////////////////////////////////////////////////////
zeroRTCEngine = new ZeroRTCEngine("ws://192.168.221.132:8099");
zeroRTCEngine.createWebsocket();

// 是否强制关闭浏览器操作
var isFouce = true;
document.getElementById('joinBtn').onclick = function () {
    roomId = document.getElementById("zero-roomId").value; 
    if (roomId == "" || roomId == "请输入房间ID") {
        alert("请输入房间ID");
        return;
    }
    console.log('doJoin roomId: ' + roomId);
    isFouce = false;
    initLocalStream();
};

document.getElementById('leaveBtn').onclick = function () {
    console.log('doLeave');
    doLeave(roomId);
    isFouce = true;
};

window.onunload = function () {
    if (isFouce) {
        doLeave(roomId);
    }
};
