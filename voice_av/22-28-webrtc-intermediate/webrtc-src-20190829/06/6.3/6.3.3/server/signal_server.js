var ws = require("nodejs-websocket")
var prort = 8099;

var server = ws.createServer(function(conn){
    console.log("创建一个新的连接--------")

    conn.sendText("我收到你的连接了....");
    conn.on("text", function(str) {
        console.info("recv msg:" + str);

    });

    conn.on("close", function(code, reason) {
        console.info("连接关闭 code: " + code + ", reason: " + reason);
    });

    conn.on("error", function(err) {
        console.info("监听到错误:" + err);
    });
}).listen(prort);