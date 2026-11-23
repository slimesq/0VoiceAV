var ws = require("nodejs-websocket")
var prort = 8099;

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

/** ----- ZeroRTCMap ----- */
var ZeroRTCMap = function () {
    this._entrys = new Array();

    this.put = function (key, value) {
        if (key == null || key == undefined) {
            return;
        }
        var index = this._getIndex(key);
        if (index == -1) {
            var entry = new Object();
            entry.key = key;
            entry.value = value;
            this._entrys[this._entrys.length] = entry;
        } else {
            this._entrys[index].value = value;
        }
    };
    this.get = function (key) {
        var index = this._getIndex(key);
        return (index != -1) ? this._entrys[index].value : null;
    };
    this.remove = function (key) {
        var index = this._getIndex(key);
        if (index != -1) {
            this._entrys.splice(index, 1);
        }
    };
    this.clear = function () {
        this._entrys.length = 0;
    };
    this.contains = function (key) {
        var index = this._getIndex(key);
        return (index != -1) ? true : false;
    };
    this.size = function () {
        return this._entrys.length;
    };
    this.getEntrys = function () {
        return this._entrys;
    };
    this._getIndex = function (key) {
        if (key == null || key == undefined) {
            return -1;
        }
        var _length = this._entrys.length;
        for (var i = 0; i < _length; i++) {
            var entry = this._entrys[i];
            if (entry == null || entry == undefined) {
                continue;
            }
            if (entry.key === key) {// equal
                return i;
            }
        }
        return -1;
    };
}

var roomTableMap = new ZeroRTCMap();

function Client(uid, conn, roomId) {
    this.uid = uid;     // 用户所属的id
    this.conn = conn;   // uid对应的websocket连接
    this.roomId = roomId;
}

function handleJoin(message, conn) {
    var roomId = message.roomId;
    var uid = message.uid;

    console.info("uid: " + uid + "try to join room " + roomId);

    var roomMap = roomTableMap.get(roomId);
    if (roomMap == null) {
        roomMap = new  ZeroRTCMap();
        roomTableMap.put(roomId, roomMap);
    }

    if(roomMap.size() >= 2) {
        console.error("roomId:" + roomId + " 已经有两人存在，请使用其他房间");
        // 加信令通知客户端，房间已满
        return;
    }

    var client = new Client(uid, conn, roomId);
    roomMap.put(uid, client);
    if(roomMap.size() > 1) {
        // 房间里面已经有人了，加上新进来的人，那就是>=2了，所以要通知对方
        var clients = roomMap.getEntrys();
        for(var i in clients) {
            var remoteUid = clients[i].key;
            if (remoteUid != uid) {
                var jsonMsg = {
                    'cmd': SIGNAL_TYPE_NEW_PEER,
                    'remoteUid': uid
                };
                var msg = JSON.stringify(jsonMsg);
                var remoteClient =roomMap.get(remoteUid);
                console.info("new-peer: " + msg);
                remoteClient.conn.sendText(msg);

                jsonMsg = {
                    'cmd':SIGNAL_TYPE_RESP_JOIN,
                    'remoteUid': remoteUid
                };
                msg = JSON.stringify(jsonMsg);
                console.info("resp-join: " + msg);
                conn.sendText(msg);
            }
        }
    }
}

var server = ws.createServer(function(conn){
    console.log("创建一个新的连接--------")

    conn.sendText("我收到你的连接了....");
    conn.on("text", function(str) {
        console.info("recv msg:" + str);
        var jsonMsg = JSON.parse(str);

        switch (jsonMsg.cmd) {
            case SIGNAL_TYPE_JOIN:
                handleJoin(jsonMsg, conn);
            break;
        }

    });

    conn.on("close", function(code, reason) {
        console.info("连接关闭 code: " + code + ", reason: " + reason);
        
    });

    conn.on("error", function(err) {
        console.info("监听到错误:" + err);
    });
}).listen(prort);