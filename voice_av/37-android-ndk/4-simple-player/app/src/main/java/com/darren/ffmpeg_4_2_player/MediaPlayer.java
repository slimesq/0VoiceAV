package com.darren.ffmpeg_4_2_player;


import android.content.Context;
import android.content.res.Configuration;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;


public class MediaPlayer implements SurfaceHolder.Callback {

    private static final String TAG = "MediaPlayer";

    static {
        System.loadLibrary("native-lib");
    }

    private SurfaceHolder mHolder;

    private OnProgressListener onProgressListener;
    private OnErrorListener onErrorListener;
    private OnPrepareListener onPrepareListener;
    private OnVideoSizeChangedListener onVideoSizeChangedListener;

    private native void native_prepare(String path);    // 准备播放，设置url，解析url有效性，解析对应的码流情况
    private native void native_start();     // 数据包当前线程等等的工作
    private native void native_stop();      // 停止播放
    private native void native_set_surface(Surface surface);    // 设置到jni，渲染是jni里面拿到对象进行渲染

    public void setSurfaceView(SurfaceView surfaceView) {
        if (mHolder != null) {
            mHolder.removeCallback(this);
        }
        mHolder = surfaceView.getHolder();
        mHolder.addCallback(this);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "surfaceCreated: ");
        native_set_surface(mHolder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.v(TAG, "surfaceChanged, format is " + format + ", width is "
                + width + ", height is " + height);
        holder.setFixedSize(width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "surfaceDestroyed: ");
    }


    public void prepare(String path) {
        native_prepare(path);
    }


    public void start() {
        native_start();
    }

    public void stop() {native_stop();}

    public void setOnPrepareListener(OnPrepareListener onPrepareListener) {
        this.onPrepareListener = onPrepareListener;
    }

    public void setOnErrorListener(OnErrorListener onErrorListener) {
        this.onErrorListener = onErrorListener;
    }

    public void setOnProgressListener(OnProgressListener onProgressListener) {
        this.onProgressListener = onProgressListener;
    }

    public void setOnVideoSizeChangedListener(OnVideoSizeChangedListener onVideoSizeChangedListener) {
        this.onVideoSizeChangedListener = onVideoSizeChangedListener;
    }

    public void onPrepare() {
        if (onPrepareListener != null) {
            onPrepareListener.onPrepare();
        }
    }

    public void onError(int errorCode) {
        if (onErrorListener != null) {
            onErrorListener.onError(errorCode);
        }
    }

    public void onProgress(int progress) {
        if (onProgressListener != null) {
            onProgressListener.onProgress(progress);
        }
    }

    public void onVideoSizeChanged(int width, int height) {
        Log.v(TAG, "onVideoSizeChanged, " + "width is "
                + width + ", height is " + height);
//        holder.setFixedSize(width, height);
        if (onVideoSizeChangedListener != null) {
            onVideoSizeChangedListener.onVideoSizeChanged(width, height);
        }
    }



    public interface OnPrepareListener {
        void onPrepare();
    }


    public interface OnErrorListener {
        void onError(int error);
    }


    public interface OnProgressListener {
        void onProgress(int progress);
    }

    public interface OnVideoSizeChangedListener{
        void onVideoSizeChanged(int  width, int height);
    }

    public static Size getOptimalDisplaySize(Context context, int videoWidth, int videoHeight) {
        int orientation = context.getResources().getConfiguration().orientation;
        int screenWidth = context.getResources().getDisplayMetrics().widthPixels;
        int screenHeight = context.getResources().getDisplayMetrics().heightPixels;
        float screenScale = (orientation == Configuration.ORIENTATION_PORTRAIT ? ((float) screenWidth / screenHeight) : ((float) screenHeight / screenWidth));
        float videoScale = (orientation == Configuration.ORIENTATION_PORTRAIT ? ((float) videoWidth / videoHeight) : ((float) videoHeight / videoWidth));
        int screenMin = (orientation == Configuration.ORIENTATION_PORTRAIT ? screenWidth : screenHeight);
        int screenMax = (orientation == Configuration.ORIENTATION_PORTRAIT ? screenHeight : screenWidth);
        int surfaceMax = (screenScale < videoScale ? screenMin : screenMax);
        int surfaceMin = (int) (screenScale <= videoScale ? ((float) surfaceMax / videoScale) : ((float) surfaceMax * videoScale));
        return (videoWidth > videoHeight ? new Size(surfaceMax, surfaceMin) : new Size(surfaceMin, surfaceMax));
    }
}