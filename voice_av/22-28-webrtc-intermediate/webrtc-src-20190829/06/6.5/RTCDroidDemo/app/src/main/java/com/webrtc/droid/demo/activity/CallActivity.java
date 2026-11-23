package com.webrtc.droid.demo.activity;

import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import com.webrtc.droid.demo.R;
import com.webrtc.droid.demo.signal.MyIceServer;
import com.webrtc.droid.demo.signal.RTCSignalClient;

import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.AudioSource;
import org.webrtc.AudioTrack;
import org.webrtc.Camera1Enumerator;
import org.webrtc.Camera2Enumerator;
import org.webrtc.CameraEnumerator;
import org.webrtc.DataChannel;
import org.webrtc.DefaultVideoDecoderFactory;
import org.webrtc.DefaultVideoEncoderFactory;
import org.webrtc.EglBase;
import org.webrtc.IceCandidate;
import org.webrtc.Logging;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaStream;
import org.webrtc.MediaStreamTrack;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.RendererCommon;
import org.webrtc.RtpReceiver;
import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;
import org.webrtc.SurfaceTextureHelper;
import org.webrtc.SurfaceViewRenderer;
import org.webrtc.VideoCapturer;
import org.webrtc.VideoDecoderFactory;
import org.webrtc.VideoEncoderFactory;
import org.webrtc.VideoFrame;
import org.webrtc.VideoSink;
import org.webrtc.VideoSource;
import org.webrtc.VideoTrack;
import org.webrtc.audio.JavaAudioDeviceModule;

import java.util.ArrayList;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 *
 */

public class CallActivity extends AppCompatActivity {

    private static final int VIDEO_RESOLUTION_WIDTH = 640;
    private static final int VIDEO_RESOLUTION_HEIGHT = 480;
    private static final int VIDEO_FPS = 15;

    private TextView mLogcatView;
    private TextView mStartCallBtn;
    private TextView mEndCallBtn;



    private static final String TAG = "CallActivity";

    //**************************************各种约束******************************************/
    private static final String AUDIO_ECHO_CANCELLATION_CONSTRAINT = "googEchoCancellation";
    private static final String AUDIO_AUTO_GAIN_CONTROL_CONSTRAINT = "googAutoGainControl";
    private static final String AUDIO_HIGH_PASS_FILTER_CONSTRAINT = "googHighpassFilter";
    private static final String AUDIO_NOISE_SUPPRESSION_CONSTRAINT = "googNoiseSuppression";

    public static final String VIDEO_TRACK_ID = "ARDAMSv0";
    public static final String AUDIO_TRACK_ID = "ARDAMSa0";
    public static final String LOCAL_STREAM_ID = "ARDAMS";

    private EglBase mRootEglBase;

    private PeerConnection mPeerConnection; // 类似js的RTCPeerConnection
    private PeerConnectionFactory mPeerConnectionFactory;
    private MediaStream mLocalStream = null;

    private SurfaceTextureHelper mSurfaceTextureHelper;

    private SurfaceViewRenderer mLocalSurfaceView;
    private SurfaceViewRenderer mRemoteSurfaceView;

    private VideoTrack mVideoTrack;
    private AudioTrack mAudioTrack;

    private VideoCapturer mVideoCapturer;

    private String mRemoteUserId;

    private ExecutorService mExecutor;
    private  String mRoomId;
    private boolean mIsJoinRoom = false;

    private ArrayList<PeerConnection.IceServer> ICEServers;
    // turn and stun
    private static MyIceServer[] iceServers = {
        new MyIceServer("stun:192.168.2.112:3478"),
        new MyIceServer("turn:192.168.2.112:3478?transport=udp",
                "lqf",
                "123456"),
        new MyIceServer("turn:192.168.2.112:3478?transport=tcp",
                "lqf",
                "123456")
    };

    private AudioManager mAudioManager;
    private RTCSignalClient mSignalClient = null;

    private MediaConstraints createAudioConstraints() {
        MediaConstraints audioConstraints = new MediaConstraints();
        audioConstraints.mandatory.add(
                new MediaConstraints.KeyValuePair(AUDIO_ECHO_CANCELLATION_CONSTRAINT, "true"));
        audioConstraints.mandatory.add(
                new MediaConstraints.KeyValuePair(AUDIO_AUTO_GAIN_CONTROL_CONSTRAINT, "false"));
        audioConstraints.mandatory.add(
                new MediaConstraints.KeyValuePair(AUDIO_HIGH_PASS_FILTER_CONSTRAINT, "true"));
        audioConstraints.mandatory.add(
                new MediaConstraints.KeyValuePair(AUDIO_NOISE_SUPPRESSION_CONSTRAINT, "true"));
        return audioConstraints;
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_call);

        mLogcatView = findViewById(R.id.LogcatView);
        mStartCallBtn = findViewById(R.id.StartCallButton);
        mEndCallBtn = findViewById(R.id.EndCallButton);

        mExecutor = Executors.newSingleThreadExecutor();

        mSignalClient = new RTCSignalClient(mOnSignalEventListener);    // 创建信令

        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        mAudioManager.setSpeakerphoneOn(true);
        mAudioManager.setMicrophoneMute(false);

        mRoomId = getIntent().getStringExtra("roomId");


        mRootEglBase = EglBase.create();

        mLocalSurfaceView = findViewById(R.id.LocalSurfaceView);
        mRemoteSurfaceView = findViewById(R.id.RemoteSurfaceView);

        mLocalSurfaceView.init(mRootEglBase.getEglBaseContext(), null);
        mLocalSurfaceView.setScalingType(RendererCommon.ScalingType.SCALE_ASPECT_FILL);
        mLocalSurfaceView.setMirror(true);
        mLocalSurfaceView.setEnableHardwareScaler(false /* enabled */);

        mRemoteSurfaceView.init(mRootEglBase.getEglBaseContext(), null);
        mRemoteSurfaceView.setScalingType(RendererCommon.ScalingType.SCALE_ASPECT_FILL);
        mRemoteSurfaceView.setMirror(true);
        mRemoteSurfaceView.setEnableHardwareScaler(true /* enabled */);
        mRemoteSurfaceView.setZOrderMediaOverlay(true);

        ProxyVideoSink videoSink = new ProxyVideoSink();
        videoSink.setTarget(mLocalSurfaceView);

        mPeerConnectionFactory = createPeerConnectionFactory(this);

        // NOTE: this _must_ happen while PeerConnectionFactory is alive!
        Logging.enableLogToDebugOutput(Logging.Severity.LS_VERBOSE);

        mVideoCapturer = createVideoCapturer();

        mSurfaceTextureHelper = SurfaceTextureHelper.create("CaptureThread", mRootEglBase.getEglBaseContext());
        VideoSource videoSource = mPeerConnectionFactory.createVideoSource(false);
        mVideoCapturer.initialize(mSurfaceTextureHelper, getApplicationContext(), videoSource.getCapturerObserver());

        mVideoTrack = mPeerConnectionFactory.createVideoTrack(VIDEO_TRACK_ID, videoSource);
        mVideoTrack.setEnabled(true);
        mVideoTrack.addSink(videoSink);

		AudioSource audioSource = mPeerConnectionFactory.createAudioSource(createAudioConstraints());
        mAudioTrack = mPeerConnectionFactory.createAudioTrack(AUDIO_TRACK_ID, audioSource);
        mAudioTrack.setEnabled(true);

        mLocalStream = mPeerConnectionFactory.createLocalMediaStream(LOCAL_STREAM_ID);

        // 本地流从mAudioTrack + mVideoTrack
        mLocalStream.addTrack(mAudioTrack);
        mLocalStream.addTrack(mVideoTrack);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mVideoCapturer.startCapture(VIDEO_RESOLUTION_WIDTH, VIDEO_RESOLUTION_HEIGHT, VIDEO_FPS);
    }

    @Override
    protected void onPause() {
        super.onPause();
        try {
            mVideoCapturer.stopCapture();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if(mIsJoinRoom) {
            mIsJoinRoom = false;
            doEndCall();
        }
        mLocalSurfaceView.release();
        mRemoteSurfaceView.release();
        mVideoCapturer.dispose();
        mSurfaceTextureHelper.dispose();
        PeerConnectionFactory.stopInternalTracingCapture();
        PeerConnectionFactory.shutdownInternalTracer();

        mSignalClient.disConnect();
    }

    public static class ProxyVideoSink implements VideoSink {
        private VideoSink mTarget;
        @Override
        synchronized public void onFrame(VideoFrame frame) {
            if (mTarget == null) {
                Log.d(TAG, "Dropping frame in proxy because target is null.");
                return;
            }
            mTarget.onFrame(frame);
        }
        synchronized void setTarget(VideoSink target) {
            this.mTarget = target;
        }
    }

    public static class SimpleSdpObserver implements SdpObserver {
        @Override
        public void onCreateSuccess(SessionDescription sessionDescription) {
            Log.i(TAG, "SdpObserver: onCreateSuccess !");
        }

        @Override
        public void onSetSuccess() {
            Log.i(TAG, "SdpObserver: onSetSuccess");
        }

        @Override
        public void onCreateFailure(String msg) {
            Log.e(TAG, "SdpObserver onCreateFailure: " + msg);
        }

        @Override
        public void onSetFailure(String msg) {
            Log.e(TAG, "SdpObserver onSetFailure: " + msg);
        }
    }

    public void onClickStartCallButton(View v) {
        Log.i(TAG, "onClickStartCallButton");
        mSignalClient.joinRoom(UUID.randomUUID().toString(), mRoomId);
        mIsJoinRoom = true;
        updateCallState(false);
    }

    public void onClickEndCallButton(View v) {
        Log.i(TAG, "onClickEndCallButton");
        doEndCall();
        mIsJoinRoom =  false;
        updateCallState(true);
    }

    private void updateCallState(boolean idle) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (idle) {
                    mStartCallBtn.setVisibility(View.VISIBLE);
                    mEndCallBtn.setVisibility(View.GONE);
                    mRemoteSurfaceView.setVisibility(View.GONE);
                } else {
                    mStartCallBtn.setVisibility(View.GONE);
                    mEndCallBtn.setVisibility(View.VISIBLE);
                    mRemoteSurfaceView.setVisibility(View.VISIBLE);
                }
            }
        });
    }

    public void doStartCall() {
        logcatOnUI("Start Call, Wait ...");
        if (mPeerConnection == null) {
            mPeerConnection = createPeerConnection();
        }
        MediaConstraints mediaConstraints = new MediaConstraints();
        mediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"));
        mediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", "true"));
        mediaConstraints.optional.add(new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));
        mPeerConnection.createOffer(new SimpleSdpObserver() {
            @Override
            public void onCreateSuccess(SessionDescription sessionDescription) {
                Log.i(TAG, "Create local offer success: \n" + sessionDescription.description);
                mPeerConnection.setLocalDescription(new SimpleSdpObserver(), sessionDescription);
                mSignalClient.sendOffer(toJsonSessionDescription(sessionDescription), mRemoteUserId);

            }
        }, mediaConstraints);
    }

    public void doEndCall() {
        if(!mIsJoinRoom) {
            Log.w(TAG,"no join room");
        }
        logcatOnUI("End Call, Wait ...");
        hanup();
        mSignalClient.leaveRoom();
    }

    public void doAnswerCall(String description) {
        logcatOnUI("Answer Call, Wait ...");
        if (mPeerConnection == null) {
            mPeerConnection = createPeerConnection();
        }
        mPeerConnection.setRemoteDescription(new SimpleSdpObserver(), new SessionDescription(SessionDescription.Type.OFFER, description));

        MediaConstraints sdpMediaConstraints = new MediaConstraints();
        MediaConstraints mediaConstraints = new MediaConstraints();
        ArrayList<MediaConstraints.KeyValuePair> keyValuePairs = new ArrayList<>();
        keyValuePairs.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"));
        keyValuePairs.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", "true"));
        mediaConstraints.mandatory.addAll(keyValuePairs);
        Log.i(TAG, "Create answer ...");
        mPeerConnection.createAnswer(new SimpleSdpObserver() {
            @Override
            public void onCreateSuccess(SessionDescription sessionDescription) {
                Log.i(TAG, "Create answer success !");
                mPeerConnection.setLocalDescription(new SimpleSdpObserver(), sessionDescription);
                mSignalClient.sendAnswer(toJsonSessionDescription(sessionDescription), mRemoteUserId);
            }
        }, sdpMediaConstraints);
        updateCallState(false);
    }

    private void hanup() {
        logcatOnUI("Hanup Call, Wait ...");
        if (mPeerConnection == null) {
            return;
        }
        mPeerConnection.close();
        mPeerConnection = null;
        logcatOnUI("Hanup Done.");
        updateCallState(true);
    }

    public PeerConnection createPeerConnection() {
        Log.i(TAG, "Create PeerConnection ...");
        this.ICEServers = new ArrayList<>();
        if (iceServers != null) {
            for (MyIceServer myIceServer : iceServers) {
                PeerConnection.IceServer iceServer = PeerConnection.IceServer
                        .builder(myIceServer.uri)
                        .setUsername(myIceServer.username)
                        .setPassword(myIceServer.password)
                        .createIceServer();
                ICEServers.add(iceServer);
            }
        }

        PeerConnection.RTCConfiguration configuration = new PeerConnection.RTCConfiguration(ICEServers); // 传入coturn的stun和turn的地址
        configuration.iceTransportsType = PeerConnection.IceTransportsType.RELAY;   // 设置为中继模式
        PeerConnection connection = mPeerConnectionFactory.createPeerConnection(configuration, mPeerConnectionObserver);
        if (connection == null) {
            Log.e(TAG, "Failed to createPeerConnection !");
            return null;
        }
        connection.addStream(mLocalStream);
//        connection.addTrack(mVideoTrack);
//        connection.addTrack(mAudioTrack);
        return connection;
    }

    public PeerConnectionFactory createPeerConnectionFactory(Context context) {
        final VideoEncoderFactory encoderFactory;
        final VideoDecoderFactory decoderFactory;

        encoderFactory = new DefaultVideoEncoderFactory(
                mRootEglBase.getEglBaseContext(), false /* enableIntelVp8Encoder */, true);
        decoderFactory = new DefaultVideoDecoderFactory(mRootEglBase.getEglBaseContext());

        PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions.builder(context)
                .setEnableInternalTracer(true)
                .createInitializationOptions());

        PeerConnectionFactory.Builder builder = PeerConnectionFactory.builder()
                .setVideoEncoderFactory(encoderFactory)
                .setVideoDecoderFactory(decoderFactory);
        builder.setOptions(null);

        return builder.createPeerConnectionFactory();
    }

    /*
     * Read more about Camera2 here
     * https://developer.android.com/reference/android/hardware/camera2/package-summary.html
     **/
    private VideoCapturer createVideoCapturer() {
        if (Camera2Enumerator.isSupported(this)) {
            return createCameraCapturer(new Camera2Enumerator(this));
        } else {
            return createCameraCapturer(new Camera1Enumerator(true));
        }
    }

    private VideoCapturer createCameraCapturer(CameraEnumerator enumerator) {
        final String[] deviceNames = enumerator.getDeviceNames();

        // First, try to find front facing camera
        Log.d(TAG, "Looking for front facing cameras.");
        for (String deviceName : deviceNames) {
            if (enumerator.isFrontFacing(deviceName)) {
                Logging.d(TAG, "Creating front facing camera capturer.");
                VideoCapturer videoCapturer = enumerator.createCapturer(deviceName, null);
                if (videoCapturer != null) {
                    return videoCapturer;
                }
            }
        }

        // Front facing camera not found, try something else
        Log.d(TAG, "Looking for other cameras.");
        for (String deviceName : deviceNames) {
            if (!enumerator.isFrontFacing(deviceName)) {
                Logging.d(TAG, "Creating other camera capturer.");
                VideoCapturer videoCapturer = enumerator.createCapturer(deviceName, null);
                if (videoCapturer != null) {
                    return videoCapturer;
                }
            }
        }
        return null;
    }

    private PeerConnection.Observer mPeerConnectionObserver = new PeerConnection.Observer() {
        @Override
        public void onSignalingChange(PeerConnection.SignalingState signalingState) {
            Log.i(TAG, "onSignalingChange: " + signalingState);
        }

        @Override
        public void onIceConnectionChange(PeerConnection.IceConnectionState iceConnectionState) {
            Log.i(TAG, "onIceConnectionChange: " + iceConnectionState);
        }

        @Override
        public void onIceConnectionReceivingChange(boolean b) {
            Log.i(TAG, "onIceConnectionChange: " + b);
        }

        @Override
        public void onIceGatheringChange(PeerConnection.IceGatheringState iceGatheringState) {
            Log.i(TAG, "onIceGatheringChange: " + iceGatheringState);
        }

        @Override
        public void onIceCandidate(IceCandidate iceCandidate) {
            Log.i(TAG, "onIceCandidate: " + iceCandidate);

            mSignalClient.sendCandidate(toJsonCandidate(iceCandidate), mRemoteUserId);
        }

        @Override
        public void onIceCandidatesRemoved(IceCandidate[] iceCandidates) {
            for (int i = 0; i < iceCandidates.length; i++) {
                Log.i(TAG, "onIceCandidatesRemoved: " + iceCandidates[i]);
            }
            mPeerConnection.removeIceCandidates(iceCandidates);
        }

        @Override
        public void onAddStream(MediaStream mediaStream) {
            Log.i(TAG, "onAddStream: " + mediaStream.videoTracks.size());
        }

        @Override
        public void onRemoveStream(MediaStream mediaStream) {
            Log.i(TAG, "onRemoveStream");
        }

        @Override
        public void onDataChannel(DataChannel dataChannel) {
            Log.i(TAG, "onDataChannel");
        }

        @Override
        public void onRenegotiationNeeded() {
            Log.i(TAG, "onRenegotiationNeeded");
        }

        @Override
        public void onAddTrack(RtpReceiver rtpReceiver, MediaStream[] mediaStreams) {
            MediaStreamTrack track = rtpReceiver.track();
            if (track instanceof VideoTrack) {  // 判别是不是VideoTrack
                Log.i(TAG, "onAddVideoTrack");
                VideoTrack remoteVideoTrack = (VideoTrack) track;
                remoteVideoTrack.setEnabled(true);
                ProxyVideoSink videoSink = new ProxyVideoSink();
                videoSink.setTarget(mRemoteSurfaceView);
                remoteVideoTrack.addSink(videoSink);
            }
        }
    };

    private RTCSignalClient.OnSignalEventListener mOnSignalEventListener = new RTCSignalClient.OnSignalEventListener() {
        @Override
        public void onConnected() {
            logcatOnUI("Signal Server Connected !");
        }

        @Override
        public void onConnecting() {
            logcatOnUI("Signal Server Connecting !");
        }

        @Override
        public void onDisconnected() {
            logcatOnUI("Signal Server Connecting !");
        }

        @Override
        public void onClosse() {
            logcatOnUI("Signal Server close");
        }


        @Override
        public void onRemoteNewPeer(JSONObject message) {
            try {
                mRemoteUserId = message.getString("remoteUid");
            }catch (JSONException e) {
                Log.e(TAG, "onRemoteNewPeer JSON parsing error: " + e.toString());
            }

            logcatOnUI("Remote User Joined: " + mRemoteUserId);
            mExecutor.execute(() -> {
                doStartCall();
            });
        }

        @Override
        public void onResponseJoin(JSONObject message) {
            try {
                mRemoteUserId = message.getString("remoteUid");
            }catch (JSONException e) {
                Log.e(TAG, "onResponseJoin JSON parsing error: " + e.toString());
            }

            logcatOnUI("Receive Remote User Joined");
        }

        @Override
        public void onRemotePeerLeave(JSONObject message) {
            logcatOnUI("Receive Remote Hanup  ...");
            hanup();
        }

        @Override
        public void onRemoteOffer(JSONObject message) {
            logcatOnUI("Receive Remote Offer ...");
            if (mPeerConnection == null) {
                mPeerConnection = createPeerConnection();
            }
            try {
                JSONObject sdpJson = new JSONObject(message.getString("msg"));
                String description = sdpJson.getString("sdp");
                doAnswerCall(description);

            } catch (JSONException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void onRemoteAnswer(JSONObject message) {
            logcatOnUI("Receive Remote Answer ...");
            try {
                JSONObject sdpJson = new JSONObject(message.getString("msg"));
                String description = sdpJson.getString("sdp");
                mPeerConnection.setRemoteDescription(new SimpleSdpObserver(), new SessionDescription(SessionDescription.Type.ANSWER, description));
            } catch (JSONException e) {
                e.printStackTrace();
            }
            updateCallState(false);
        }

        @Override
        public void onRemoteCandidate(JSONObject message) {
            logcatOnUI("Receive Remote Candidate ...");
            try {
                String candidateString = message.getString("msg");
                IceCandidate iceCandidate = toJavaCandidate(new JSONObject(candidateString));
                mPeerConnection.addIceCandidate(iceCandidate);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
    };

    private void logcatOnUI(String msg) {
        Log.i(TAG, msg);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String output = mLogcatView.getText() + "\n" + msg;
                mLogcatView.setText(output);
            }
        });
    }

    private String toJsonSessionDescription(SessionDescription sdp) {
        Log.i(TAG, "toJsonSessionDescription");
        JSONObject json = new JSONObject();

        String type = "offer";
        if (sdp.type == SessionDescription.Type.OFFER) {
            type = "offer";
        } else if (sdp.type == SessionDescription.Type.ANSWER){
            type = "answer";
        } else if (sdp.type == SessionDescription.Type.PRANSWER){
            type = "pranswer";
        } else {
            type = "unkown";
            Log.e(TAG, "toJsonSessionDescription failed: unknown the sdp type");
        }
        String sdpDescription = sdp.description;

        try {
            json.put("sdp", sdpDescription);
            json.put("type", type);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }

        return json.toString();
    }

    private String toJsonCandidate(IceCandidate candidate) {
        JSONObject json = new JSONObject();

        try {
            json.put("id", candidate.sdpMid);
            json.put("label", candidate.sdpMLineIndex);
            json.put("candidate", candidate.sdp);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }

        return json.toString();
    }

    private IceCandidate toJavaCandidate(JSONObject json) throws JSONException {
        return new IceCandidate(
                json.getString("id"), json.getInt("label"), json.getString("candidate"));
    }
}
