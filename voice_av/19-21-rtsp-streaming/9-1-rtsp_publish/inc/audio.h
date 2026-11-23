#ifndef AUDIO_H
#define AUDIO_H
#include "mediabase.h"
#include "codecs.h"


//class AudioEncoder
//{
//public:
//    // Must have virtual destructor to ensure child class's destructor is called
//    virtual ~AudioEncoder(){};
//    virtual int   Encode(uint8_t *in,int inLen,uint8_t* out,int out_len)=0;
//    virtual uint32_t TrySetRate(uint32_t rate)=0;
//    virtual uint32_t GetRate()=0;
//    virtual uint32_t GetClockRate()=0;
//    virtual uint32_t GetFrameSampleSize()=0;

//    virtual uint32_t GetFrameSampleSize() = 0;

//    AudioCodec::Type	type;
//    int			nb_frame_samples;   // 一帧的sample数量, 算单个channel
//    int			nb_frame_size;      // 一帧的输入byte size
//};

//class AudioDecoder
//{
//public:
//    // Must have virtual destructor to ensure child class's destructor is called
//    virtual ~AudioDecoder(){};
//    virtual int   Decode(const BYTE *in,int inLen,SWORD* out,int out_len)=0;
//    virtual DWORD TrySetRate(DWORD rate)=0;
//    virtual DWORD GetRate()=0;
//    AudioCodec::Type	type;
//    int			nb_frame_samples;
//};


#endif // AUDIO_H
