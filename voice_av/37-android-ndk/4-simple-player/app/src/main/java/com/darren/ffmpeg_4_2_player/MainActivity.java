package com.darren.ffmpeg_4_2_player;

import android.Manifest;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.util.List;

import pub.devrel.easypermissions.AfterPermissionGranted;
import pub.devrel.easypermissions.AppSettingsDialog;
import pub.devrel.easypermissions.EasyPermissions;

public class MainActivity extends AppCompatActivity implements View.OnClickListener,
        EasyPermissions.PermissionCallbacks,
        SeekBar.OnSeekBarChangeListener,
        MediaPlayer.OnPrepareListener,
        MediaPlayer.OnVideoSizeChangedListener,
        MediaPlayer.OnErrorListener{

    private static final String TAG = "MediaPlayer";
    private static final String TAG_PERMISSION = "Permission";
    private static final int PERMISSION_STORAGE_CODE = 10001;
    private static final String PERMISSION_STORAGE_MSG = "需要SD卡读写权限，否则无法正常使用";
    private static final String[] PERMS = {Manifest.permission.WRITE_EXTERNAL_STORAGE};
    private static final String FILE_NAME = "input.mp4";
    private SurfaceView mSurfaceView;
    private SeekBar seekBar;
    private int progress;
    private MediaPlayer mPlayer;
    private boolean  isPlay = false;
    int videoWidth = 1280;
    int videoHeight = 720;
    //屏幕宽度
    private int mScreenWidth;
    //屏幕高度
    private int mScreenHeight;
    //记录现在的播放位置
    private int mCurrentPos;
    boolean isLand = false;
    private DisplayMetrics displayMetrics;
    public static final float SHOW_SCALE = 16 * 1.0f / 9;

    private RelativeLayout mSurfaceLayout;

    private String filePath;
    private Button btnSelect;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
//        设置布局文件
        setContentView(R.layout.activity_main);
//      获取屏幕宽高
        displayMetrics = new DisplayMetrics();
        this.getWindow().getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        mScreenWidth = displayMetrics.widthPixels;
        mScreenHeight = displayMetrics.heightPixels;
        // 获取SurfaceLayout，RelativeLayout是为了自适应视频的宽高比例
        mSurfaceLayout = (RelativeLayout) findViewById(R.id.layout_gesture);
        RelativeLayout.LayoutParams lp = (RelativeLayout.LayoutParams) mSurfaceLayout.getLayoutParams();
        lp.height = (int) (mScreenWidth * SHOW_SCALE);      // 按照16：9
        mSurfaceLayout.setLayoutParams(lp);

        // 真正显示的控件 SurfaceView
        mSurfaceView = findViewById(R.id.surfaceView);
        // 进度条，大家自己去实现进度
        seekBar = findViewById(R.id.seekBar);

        // 选择文件
        btnSelect = findViewById(R.id.open_file);   // 打开文件浏览器
        btnSelect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openSystemFile();
            }
        });

        initData();
        initEvent();
    }

    public void openSystemFile() {
        if (!EasyPermissions.hasPermissions(this, PERMS)) {
            // 申请权限
            EasyPermissions.requestPermissions(this, PERMISSION_STORAGE_MSG,
                    PERMISSION_STORAGE_CODE, PERMS);
            return;
        }

        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        // 所有类型
        intent.setType("*/*");  //intent.setType("video/*");
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            startActivityForResult(Intent.createChooser(intent, "请选择文件"), 1);
        } catch (ActivityNotFoundException e) {
            e.printStackTrace();
            Toast.makeText(MainActivity.this, "请安装文件管理器", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 1 && resultCode == Activity.RESULT_OK) {
            //Get the Uri of the selected file
            Uri uri = data.getData();
            if (null != uri) {
                filePath = ContentUriUtil.getPath(this, uri);   // 返回完整的路径
                Log.i("filepath", " = " + filePath);
            }
        }
    }

    private void initData() {
        mPlayer = new MediaPlayer();        // 自己封装的MediaPlayer，和jni进行打交道
        mPlayer.setSurfaceView(mSurfaceView);
    }

    private void initEvent() {
        findViewById(R.id.play_video).setOnClickListener(this);     // 播放按钮响应
        findViewById(R.id.stop_video).setOnClickListener(this);     // 停止按钮响应
        seekBar.setOnSeekBarChangeListener(this);   // 自己去实现进度条
        mPlayer.setOnPrepareListener(this);         // 和播放器状态有关系
        mPlayer.setOnVideoSizeChangedListener(this);
        mPlayer.setOnErrorListener(this);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.play_video:
                playVideo();
                break;
            case R.id.stop_video:
                mPlayer.stop();
                isPlay = false;
                break;
        }
    }

    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {

    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onPrepare() {
        isPlay = true;
        mPlayer.start();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        isLand = newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE;
        getWindowManager().getDefaultDisplay().getMetrics(displayMetrics);
        mScreenWidth = displayMetrics.widthPixels;
        mScreenHeight = displayMetrics.heightPixels;

        resetSize();
    }

    public void changeOrientation(View view) {
        if (Configuration.ORIENTATION_LANDSCAPE == this.getResources()
                .getConfiguration().orientation) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        }

    }

    private void resetSize( ) {

        float areaWH = 0.0f;
        int height;

        if (!isLand) {
            // 竖屏16:9
            height = (int) (mScreenWidth / SHOW_SCALE);
            areaWH = SHOW_SCALE;
        } else {
            //横屏按照手机屏幕宽高计算比例
            height = mScreenHeight;
            areaWH = mScreenWidth / mScreenHeight;
        }

        RelativeLayout.LayoutParams layoutParams =
                new RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height);
        mSurfaceLayout.setLayoutParams(layoutParams);

        int mediaWidth = videoWidth;
        int mediaHeight = videoHeight;

        float mediaWH = mediaWidth * 1.0f / mediaHeight;

        RelativeLayout.LayoutParams layoutParamsSV = null;

        if (areaWH > mediaWH) {
            //直接放会矮胖
            int svWidth = (int) (height * mediaWH);
            layoutParamsSV = new RelativeLayout.LayoutParams(svWidth, height);
            layoutParamsSV.addRule(RelativeLayout.CENTER_IN_PARENT);
            mSurfaceView.setLayoutParams(layoutParamsSV);
        }

        if (areaWH < mediaWH) {
            //直接放会瘦高。
            int svHeight = (int) (mScreenWidth / mediaWH);
            layoutParamsSV = new RelativeLayout.LayoutParams(mScreenWidth, svHeight);
            layoutParamsSV.addRule(RelativeLayout.CENTER_IN_PARENT);
            mSurfaceView.setLayoutParams(layoutParamsSV);
        }

    }
    @Override
    public void  onError(int errorCode) {
        Log.d(TAG, "errorCode: " + errorCode);
    }

    @Override
    public void onVideoSizeChanged(int w, int h) {
        videoWidth = w;
        videoHeight = h;
        MainActivity.this.runOnUiThread(new Runnable() {
            public void run() {
                resetSize();
            }
        });
    }

    private void playVideo() {
        if (isPlay) {
            Toast.makeText(this, "当前正在播放", Toast.LENGTH_SHORT).show();
        } else {
             isPlay = true;
             mPlayer.prepare(filePath);
//           mPlayer.prepare("rtmp://114.215.169.66/live/livestream");
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        EasyPermissions.onRequestPermissionsResult(requestCode, permissions, grantResults, this);
    }

    @Override
    public void onPermissionsGranted(int requestCode, @NonNull List<String> perms) {
        if (requestCode == PERMISSION_STORAGE_CODE) {
            Log.d(TAG_PERMISSION, "onPermissionsGranted: ");
        }
    }

    @Override
    public void onPermissionsDenied(int requestCode, @NonNull List<String> perms) {
        if (EasyPermissions.somePermissionPermanentlyDenied(this, perms)) {
            // 拒绝权限，并不再询问
            new AppSettingsDialog
                    .Builder(this)
                    .setTitle("授权提醒")
                    .setRationale(PERMISSION_STORAGE_MSG)
                    .setPositiveButton("打开设置")
                    .setNegativeButton("取消")
                    .build()
                    .show();
        } else {
            // 拒绝权限
            if (requestCode == PERMISSION_STORAGE_CODE) {
                Log.d(TAG_PERMISSION, "onPermissionsDenied: ");
            }
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if(isPlay) {
            mPlayer.stop();
            isPlay = false;
        }
        super.onBackPressed();
    }
}
