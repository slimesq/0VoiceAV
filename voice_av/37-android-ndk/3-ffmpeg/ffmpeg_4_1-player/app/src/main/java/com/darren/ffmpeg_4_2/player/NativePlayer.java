package com.darren.ffmpeg_4_2.player;

public class NativePlayer {
    static {
        System.loadLibrary("native-player");
    }

    public native int playVideo(String videoPath, Object surface);
}
