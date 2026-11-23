package com.darren.ffmpeg_4_2

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.PersistableBundle
import android.view.SurfaceView
import android.widget.TextView
import com.darren.ffmpeg_4_1.R
import com.darren.ffmpeg_4_2.player.NativePlayer
import kotlinx.android.synthetic.main.activity_main.*

class MainActivity : AppCompatActivity() {

    private var fFmpegPlay: NativePlayer? = null

    private var btnPlay: TextView? = null
    private var surfaceView: SurfaceView ? = null
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
    }

    override fun onPostCreate(savedInstanceState: Bundle?, persistentState: PersistableBundle?) {
        super.onPostCreate(savedInstanceState, persistentState)
    }

    private void setupVideo() {
        btnPlay = findViewById(R.id.video_play_btn_start)

    }
}
