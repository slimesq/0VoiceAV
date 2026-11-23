#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

#include "pullwork.h"
using namespace LQF;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->urlEdit->setText("rtmp://120.27.131.197/live/livestream");
    ui->bufDurationBox->setCurrentIndex(3);
    ui->jitterBufBox->setCurrentIndex(1);
    ui->speedBox->setCurrentIndex(1);
    ui->stopButton->setEnabled(false);

    // 初始化为0ms
    ui->audioBufEdit->setText("0ms");
    ui->videoBufEdit->setText("0ms");
    // 0字节
//    ui->audioBitrateEdit->setText("0");
//    ui->videoBitrateEdit->setText("0");
    connect(this, &MainWindow::sig_UpdateAudioCacheDuration, this, &MainWindow::on_UpdateAudioCacheDuration);
    connect(this, &MainWindow::sig_UpdateVideoCacheDuration, this, &MainWindow::on_UpdateVideoCacheDuration);
}


MainWindow::~MainWindow()
{
    if(pull_work_) {            // 退出的时候
        delete pull_work_;
        pull_work_ = NULL;
        ui->stopButton->setEnabled(false);
        ui->playButton->setEnabled(true);
        // 初始化为0ms
        ui->audioBufEdit->setText("0ms");
        ui->videoBufEdit->setText("0ms");
        // 0字节
//        ui->audioBitrateEdit->setText("0");
//        ui->videoBitrateEdit->setText("0");
    }
    delete ui;
}

int MainWindow::DrawVideo(const Frame *video_frame)
{
    return ui->videoWidget->draw(video_frame, false, false);
}

int MainWindow::EventCallback(int what, int arg1, int arg2, int arg3, void *data)
{
    int cache_duration;
    switch (what) {
    case EVENT_UPDATE_AUDIO_CACHE_DURATION:
        cache_duration = arg1 + arg2;
        emit sig_UpdateAudioCacheDuration(cache_duration);
        break;
    case EVENT_UPDATE_VIDEO_CACHE_DURATION:
        cache_duration = arg1 + arg2;
        emit sig_UpdateVideoCacheDuration(cache_duration);
        break;
    default:
        break;
    }
//    switch (what) {
//    case EVENT_UPDATE_AUDIO_CACHE_DURATION:
//        int audio_cache = arg1 + arg2;
//        emit sig_UpdateAudioCacheDuration(audio_cache);
//        break;
//    default:
//        LogError("no case handle");
//        break;
//    }
    return 0;
}



void MainWindow::on_playButton_clicked()
{
    if(!pull_work_) {
        // 启动推流
        pull_work_ = new PullWork();
        // 读取url
        url_ = ui->urlEdit->text().toStdString();
        Properties   pull_properties;

        pull_properties.SetProperty("rtmp_url", url_);
        pull_properties.SetProperty("video_out_width", 720);
        pull_properties.SetProperty("video_out_height", 480);
        pull_properties.SetProperty("audio_out_sample_rate", 48000);

        pull_properties.SetProperty("network_jitter_duration", network_jitter_duration_);
        pull_properties.SetProperty("accelerate_speed_factor", accelerate_speed_factor_);
        pull_properties.SetProperty("max_cache_duration", max_cache_duration_);    //缓存时间

        pull_work_->AddVideoRefreshCallback(std::bind(&MainWindow::DrawVideo, this,
                                                      std::placeholders::_1));
        pull_work_->AddEventCallback(std::bind(&MainWindow::EventCallback, this,
                                               std::placeholders::_1,
                                               std::placeholders::_2,
                                               std::placeholders::_3,
                                               std::placeholders::_4,
                                               std::placeholders::_5));
        if(pull_work_->Init(pull_properties) != RET_OK)
        {
            LogError("pushwork.Init failed");
            QMessageBox::warning(this, "PushWork", "Init failed");
            delete pull_work_;
            pull_work_ = NULL;
            return ;
        }
        ui->stopButton->setEnabled(true);
        ui->playButton->setEnabled(false);
    }
}

void MainWindow::on_stopButton_clicked()
{
    if(pull_work_) {
        delete pull_work_;
        pull_work_ = NULL;
        ui->stopButton->setEnabled(false);
        ui->playButton->setEnabled(true);
    }
}

void MainWindow::on_bufDurationBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        max_cache_duration_ = 30;
        break;
    case 1:
        max_cache_duration_ = 100;
        break;
    case 2:
        max_cache_duration_ = 200;
        break;
    case 3:
        max_cache_duration_ = 400;
        break;
    case 4:
        max_cache_duration_ = 600;
        break;
    case 5:
        max_cache_duration_ = 800;
        break;
    case 6:
        max_cache_duration_ = 1000;
        break;
    case 7:
        max_cache_duration_ = 2000;
        break;
    case 8:
        max_cache_duration_ = 4000;
        break;
    default:
        break;
    }
    if(pull_work_)
        pull_work_->setMax_cache_duration(max_cache_duration_);
    LogInfo("max_cache_duration_ set to %dms", max_cache_duration_);
}

void MainWindow::on_jitterBufBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        network_jitter_duration_ = 30;
        break;
    case 1:
        network_jitter_duration_ = 100;
        break;
    case 2:
        network_jitter_duration_ = 200;
        break;
    case 3:
        network_jitter_duration_ = 400;
        break;
    case 4:
        network_jitter_duration_ = 600;
        break;
    case 5:
        network_jitter_duration_ = 800;
        break;
    case 6:
        network_jitter_duration_ = 1000;
        break;
    default:
        LogError("can't handle jitterBufBox index:%d", index);
        break;
    }
    if(pull_work_)
        pull_work_->setNetwork_jitter_duration(network_jitter_duration_);
    LogInfo("network_jitter_duration_ set to %dms", network_jitter_duration_);
}

void MainWindow::on_speedBox_currentIndexChanged(int index)
{
    switch (index) {
    case 0:
        accelerate_speed_factor_ = 1.0;
        break;
    case 1:
        accelerate_speed_factor_ = 1.2;
        break;
    case 2:
        accelerate_speed_factor_ = 1.5;
        break;
    case 3:
        accelerate_speed_factor_ = 2.0;
        break;
    default:
        LogError("can't handle speedBox index:%d", index);
        break;
    }
    if(pull_work_)
        pull_work_->setAccelerate_speed_factor(accelerate_speed_factor_);
    LogInfo("accelerate_speed_factor_ set to %f", accelerate_speed_factor_);
}

void MainWindow::on_UpdateAudioCacheDuration(int duration)
{
    ui->audioBufEdit->setText(QString("%1ms").arg(duration));
}


void MainWindow::on_UpdateVideoCacheDuration(int duration)
{
    ui->videoBufEdit->setText(QString("%1ms").arg(duration));
}
