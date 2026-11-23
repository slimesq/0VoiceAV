#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "pullwork.h"
#include "mediabase.h"
using namespace LQF;
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    int DrawVideo(const Frame *video_frame);
    int EventCallback(int what, int arg1, int arg2, int arg3, void *data);
private slots:
    void on_playButton_clicked();

    void on_stopButton_clicked();

    void on_bufDurationBox_currentIndexChanged(int index);

    void on_jitterBufBox_currentIndexChanged(int index);

    void on_speedBox_currentIndexChanged(int index);

    void on_UpdateAudioCacheDuration(int duration);
    void on_UpdateVideoCacheDuration(int duration);
signals:
    void sig_UpdateAudioCacheDuration(int duration);
    void sig_UpdateVideoCacheDuration(int duration);
private:
    Ui::MainWindow *ui;
    std::string url_;
    PullWork *pull_work_ = NULL;
    int max_cache_duration_ = 400;  // 默认200ms
    int network_jitter_duration_ = 100; // 默认100ms
    float accelerate_speed_factor_ = 1.2; //默认加速是1.2
    float normal_speed_factor_ = 1.0;     // 正常播放速度1.0

    // 缓存长度
    float audio_buf_duration = 0;
    float video_buf_duration = 0;

    // 码率
    float audio_bitrate_duration = 0;
    float video_bitrate_duration = 0;

};

#endif // MAINWINDOW_H
