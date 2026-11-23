/*
 * @file 	videoctl.h
 * @date 	2018/01/07 10:48
 *
 * @author 	itisyang
 * @Contact	itisyang@gmail.com
 *
 * @brief 	视频控制类
 * @note
 */
#ifndef VIDEOCTL_H
#define VIDEOCTL_H

#include <QObject>
#include <QThread>
#include <QString>

#include "globalhelper.h"
#include "ff_ffplay.h"
#define FFP_PROP_FLOAT_PLAYBACK_RATE                    10003       // 设置播放速率
#define FFP_PROP_FLOAT_PLAYBACK_VOLUME                  10006

#define PLAYBACK_RATE_MIN           0.25     // 最慢
#define PLAYBACK_RATE_MAX           3.0     // 最快
#define PLAYBACK_RATE_SCALE         0.25    // 变速刻度
//单例模式
class VideoCtl : public QObject
{
    Q_OBJECT

public:
    explicit VideoCtl(QObject *parent = nullptr);

private:

    /**
     * @brief	初始化
     *
     * @return	true 成功 false 失败
     * @note
     */
    bool Init();

public:

private:
    FFPlayer *player_;
};

#endif // VIDEOCTL_H
