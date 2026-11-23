/*
 * @file 	videoctl.cpp
 * @date 	2018/01/21 12:15
 *
 * @author 	itisyang
 * @Contact	itisyang@gmail.com
 *
 * @brief 	视频控制类
 * @note
 */


#include <QDebug>
#include <QMutex>

#include <thread>
#include "videoctl.h"


VideoCtl::VideoCtl(QObject *parent) :
    QObject(parent),
    player_(NULL)
{
    //注册所有复用器、编码器
    //网络格式初始化
    avformat_network_init();
}

bool VideoCtl::Init()
{
    player_ = (FFPlayer *)malloc(sizeof(FFPlayer));


    return true;
}
