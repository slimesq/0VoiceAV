package com.webrtc.droid.demo.signal;

import android.util.Log;

import org.java_websocket.client.WebSocketClient;
import org.java_websocket.drafts.Draft_6455;
import org.java_websocket.handshake.ServerHandshake;
import org.json.JSONException;
import org.json.JSONObject;

import java.net.URI;
import java.net.URISyntaxException;


public class RTCSignalClient {

    private static final String TAG = "RTCSignalClient";
    private static  final  String WS_URL = "ws://192.168.2.112:8099";

    // join 主动加入房间
    // leave 主动离开房间
    // new-peer 有人加入房间，通知已经在房间的人
    // peer-leave 有人离开房间，通知已经在房间的人
    // offer 发送offer给对端peer
    // answer发送offer给对端peer
    // candidate 发送candidate给对端peer
    public static final String SIGNAL_TYPE_JOIN = "join";
    public static final String SIGNAL_TYPE_RESP_JOIN = "resp-join";  // 告知加入者对方是谁
    public static final String SIGNAL_TYPE_LEAVE = "leave";
    public static final String SIGNAL_TYPE_NEW_PEER = "new-peer";
    public static final String SIGNAL_TYPE_PEER_LEAVE = "peer-leave";
    public static final String SIGNAL_TYPE_OFFER = "offer";
    public static final String SIGNAL_TYPE_ANSWER = "answer";
    public static final String SIGNAL_TYPE_CANDIDATE = "candidate";


    private OnSignalEventListener mOnSignalEventListener;

    private WebSocketClient mWebSocketClient = null;
    private String mUserId;
    private String mRoomId;



    public RTCSignalClient(final OnSignalEventListener listener) {
        URI uri = null;
        try {
            uri = new URI(WS_URL);
        } catch (URISyntaxException e) {
            e.printStackTrace();
        }
        mOnSignalEventListener = listener;// 设置回调函数
        mWebSocketClient = new JWebSocketClient(uri);   // 新建websocket类
        mOnSignalEventListener.onConnecting();  // 通知应用层正在连接信令服务器

        mWebSocketClient.connect(); // 去连接信令服务器
        Log.i(TAG, "RTCSignalClient");
    }


    public class JWebSocketClient extends WebSocketClient {
        public JWebSocketClient(URI serverUri) {
            super(serverUri, new Draft_6455());
        }

        @Override
        public void onOpen(ServerHandshake handshakedata) { // 说明websocket连接成功
            Log.i(TAG, "onOpen()");
            if(mOnSignalEventListener != null) {
                mOnSignalEventListener.onConnected();
            }
        }

        @Override
        public void onMessage(String message) { // 接收服务器发送过来的数据
            Log.i(TAG, "onMessage()");
            if(mOnSignalEventListener != null) {
                try {
                    JSONObject jsonMsg = new JSONObject(message);
                    String cmd = jsonMsg.getString("cmd");
                    switch (cmd) {
                        case SIGNAL_TYPE_NEW_PEER:
                            mOnSignalEventListener.onRemoteNewPeer(jsonMsg);
                            break;
                        case SIGNAL_TYPE_RESP_JOIN:
                            mOnSignalEventListener.onResponseJoin(jsonMsg);
                            break;
                        case SIGNAL_TYPE_PEER_LEAVE:
                            mOnSignalEventListener.onRemotePeerLeave(jsonMsg);
                            break;
                        case SIGNAL_TYPE_OFFER:
                            mOnSignalEventListener.onRemoteOffer(jsonMsg);
                            break;
                        case SIGNAL_TYPE_ANSWER:
                            mOnSignalEventListener.onRemoteAnswer(jsonMsg);
                            break;
                        case SIGNAL_TYPE_CANDIDATE:
                            mOnSignalEventListener.onRemoteCandidate(jsonMsg);
                            break;
                        default:
                            Log.e(TAG,"can't handle the cmd " + cmd);
                            break;
                    }
                } catch (JSONException e) {
                    Log.e(TAG, "WebSocket message JSON parsing error: " + e.toString());
                }
            }
        }

        @Override
        public void onClose(int code, String reason, boolean remote) {
            Log.w(TAG, "onClose() reason:" + reason + ", clode:" +code);
            if(mOnSignalEventListener != null) {
                mOnSignalEventListener.onClosse();
            }
        }

        @Override
        public void onError(Exception ex) {
            Log.e(TAG, "onError()" + ex);
            if(mOnSignalEventListener != null) {
                mOnSignalEventListener.onDisconnected();
            }
        }
    }

    public interface OnSignalEventListener {
        void onConnected();
        void onConnecting();
        void onDisconnected();
        void onClosse();
        void onRemoteNewPeer(JSONObject message);   // 新人加入
        void onResponseJoin(JSONObject message);    // 加入回应
        void onRemotePeerLeave(JSONObject message);
        void onRemoteOffer(JSONObject message);
        void onRemoteAnswer(JSONObject message);
        void onRemoteCandidate(JSONObject message);
    }

    public String getUserId() {
        return mUserId;
    }

    public void joinRoom(String userId, String roomId) {
        Log.i(TAG, "joinRoom: " + userId + ", " + roomId);
        if (mWebSocketClient == null) {
            return;
        }
        mUserId = userId;
        mRoomId = roomId;
        try {
            JSONObject args = new JSONObject();
            args.put("cmd", SIGNAL_TYPE_JOIN);
            args.put("roomId", mRoomId);
            args.put("uid", mUserId);
            mWebSocketClient.send(args.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void leaveRoom() {
        Log.i(TAG, "leaveRoom: " + mRoomId);
        if (mWebSocketClient == null) {
            return;
        }
        try {
            JSONObject args = new JSONObject();
            args.put("cmd", SIGNAL_TYPE_LEAVE);
            args.put("roomId", mRoomId);
            args.put("uid", mUserId);
            mWebSocketClient.send(args.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void sendOffer(String offer, String remoteUid) {
        Log.i(TAG, "send offer");
        if (mWebSocketClient == null) {
            return;
        }
        try {
            JSONObject args = new JSONObject();
            args.put("cmd", SIGNAL_TYPE_OFFER);
            args.put("roomId", mRoomId);
            args.put("uid", mUserId);
            args.put("remoteUid", remoteUid);
            args.put("msg", offer);
            mWebSocketClient.send(args.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void sendAnswer(String answer, String remoteUid) {
        Log.i(TAG, "send answer");
        if (mWebSocketClient == null) {
            return;
        }
        try {
            JSONObject args = new JSONObject();
            args.put("cmd", SIGNAL_TYPE_ANSWER);
            args.put("roomId", mRoomId);
            args.put("uid", mUserId);
            args.put("remoteUid", remoteUid);
            args.put("msg", answer);
            mWebSocketClient.send(args.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void sendCandidate(String candidate, String remoteUid) {
        Log.i(TAG, "send candidate");
        if (mWebSocketClient == null) {
            return;
        }
        try {
            JSONObject args = new JSONObject();
            args.put("cmd", SIGNAL_TYPE_CANDIDATE);
            args.put("roomId", mRoomId);
            args.put("uid", mUserId);
            args.put("remoteUid", remoteUid);
            args.put("msg", candidate);
            mWebSocketClient.send(args.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }
    public void disConnect() {
        if (mWebSocketClient == null) {
            return;
        }
        mWebSocketClient.close();
        mWebSocketClient = null;
    }
}
