#ifndef RTMPBASE_H
#define RTMPBASE_H
#include <iostream> // c++系统文件

#include "librtmp/rtmp.h" // 其他库文件

namespace LQF
{

enum RTMP_BASE_TYPE
{
    RTMP_BASE_TYPE_UNKNOW,
    RTMP_BASE_TYPE_PLAY,
    RTMP_BASE_TYPE_PUSH
};

class RTMPBase
{
public:
    // RTMP Server   //由于crtmpserver是每个一段时间(默认8s)发送数据包,需大于发送间隔才行
    virtual bool Connect(std::string url);
    //必须确保已经设置过url
    bool Connect();
    void SetConnectUrl(std::string& url);
    // 断开
    void Disconnect();

    bool IsConnect();
    //是否接受音频
    bool SetReceiveAudio(bool is_recv_audio);
    bool SetReceiveVideo(bool is_recv_video);
    static uint32_t  GetSampleRateByFreqIdx(uint8_t freq_idx);

    RTMPBase();
    RTMPBase(RTMP_BASE_TYPE rtmp_obj_type);
    RTMPBase(RTMP_BASE_TYPE rtmp_obj_type, std::string&  url);
    //此构造函数默认构造player
    RTMPBase(std::string& url,bool is_recv_audio,bool is_recv_video);
    virtual ~RTMPBase();
private:
    bool initRtmp();
    RTMP_BASE_TYPE rtmp_obj_type_;
protected:
    RTMP* rtmp_;
    std::string url_;
    bool enable_video_;      //是否打开视频
    bool enable_audio_;      //是否打开音频
};
}


#endif // RTMPBASE_H
