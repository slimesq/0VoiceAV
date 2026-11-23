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

var localUserId = Math.random().toString(36).substr(2); // store local userId
var remoteUserId = -1;
var localStream = null; // local video stream object
var remoteStream = null;
var pc = null; // webrtc RTCPeerConnection

var roomId = '100';

var localVideo = document.querySelector('#localVideo'); // 本地头像
var remoteVideo = document.querySelector('#remoteVideo');// 远端头像


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
    console.log('setRemoteDescription');
    pc.setRemoteDescription(desc);
    doAnswer();
}

function handleRemoteAnswer(message) {
    console.log('Remote answer received: ', message.msg);
    let desc = JSON.parse(message.msg);
    console.log('setRemoteDescription');
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
    pc.addIceCandidate(candidate).catch(e => {
        console.log("Failure during addIceCandidate(): " + e.name);
    });
}


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

function closeLocalStream() {
    if(!(localStream === null || localStream === undefined)){
		localStream.getTracks().forEach((track)=>{
			track.stop();
		});
	}
    localStream = null;
    localVideo.srcObject = null;
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

        pc = new RTCPeerConnection(null);
        // 获取到candidate事件
        pc.onicecandidate = handleIceCandidate;
        // 远端码流加入事件
        pc.ontrack = handleRemoteStreamAdded;
        // 远端码流删除事件
        pc.onremovestream = handleRemoteStreamRemoved;
         // sdp信令状态
        pc.onsignalingstatechange = handleSignalingStateChange;
        // Peer连接状态
        pc.onconnectionstatechange = handleConnectionStateChange;
        // 用于描述连接的ICE连接状态
        pc.oniceconnectionstatechange = handleIceconnectionStateChange;

        localStream.getTracks().forEach((track) => pc.addTrack(track, localStream));
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
    pc.createOffer().then(createOfferAndSendMessage).catch(handleCreateOfferError);
}

// 创建answer并发发送
function doAnswer() {
    console.log('Starting answer: Sending answer to remote peer');
    if (pc == null) {
        createPeerConnection()
    }
    pc.createAnswer().then(createAnswerAndSendMessage).catch(handleCreateAnswerError);
}

function createOfferAndSendMessage(sessionDescription) {
    console.log('CreateOfferAndSendMessage sending message', sessionDescription);
    pc.setLocalDescription(sessionDescription).then(function() {
        console.log('offer setLocalDescription ok');
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
    })
    .catch(function(reason) {
        console.error('offer setLocalDescription failed:' + reason);
    });
     
}

function createAnswerAndSendMessage(sessionDescription) {
    console.log('CreateAnswerAndSendMessage sending message', sessionDescription);
    pc.setLocalDescription(sessionDescription)
    .then(function() {
        console.info('answer setLocalDescription ok');
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
    })
    .catch(function(reason) {
        console.error('answer setLocalDescription failed:' + reason);
    });
    
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

function handleRemoteStreamAdded(e) {
    console.log('Handle remote stream added.');
    remoteStream = e.streams[0];
    remoteVideo.srcObject = e.streams[0];
}

function handleRemoteStreamRemoved(event) {
    console.log('Handle remote stream removed. Event: ', event);
    remoteVideo.srcObject = null;
}

function handleSignalingStateChange(){
    if(pc)
        console.info("PeerConnection: signalingState -> ", pc.signalingState);
};

function handleConnectionStateChange() {
    if(pc)
        console.info("PeerConnection: connectionState -> ", pc.connectionState);
}
// 具体状态https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState
function handleIceconnectionStateChange() {
    if(pc)
        console.info("PeerConnection: iceConnectionState -> ", pc.iceConnectionState);
}

function hangup() {
    console.log('Hanging up !');
    remoteVideo.srcObject = null;
    closeLocalStream();
    if (pc != null) {
        pc.close();
        pc = null;
    }
}

/////////////////////////////////////////////////////////
zeroRTCEngine = new ZeroRTCEngine("ws://192.168.221.134:8099");
zeroRTCEngine.createWebsocket();

document.getElementById('joinBtn').onclick = function () {
    roomId = document.getElementById("zero-roomId").value; 
    if (roomId == "" || roomId == "请输入房间ID") {
        alert("请输入房间ID");
        return;
    }
    console.log('doJoin roomId: ' + roomId);
    initLocalStream();
};

document.getElementById('leaveBtn').onclick = function () {
    console.log('doLeave');
    doLeave(roomId);
};


