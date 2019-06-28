#pragma once
/*
*  使用fifo重置不同格式的音频帧大小
*  本类使用4.13版本ffmpeg
*  2019-6-27
*/

#ifdef __cplusplus
extern "C"
{
	#endif

#include <libavcodec\avcodec.h>
//#include <libavutil\opt.h>
//#include "libavformat/avformat.h"
//#include "libswscale/swscale.h"
//#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"

//#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
//#pragma comment(lib, "avformat.lib")
//#pragma comment(lib, "avdevice.lib")
	//#ifdef __cplusplus
};
//#endif

class CAudioFrameResize
{
public:
	CAudioFrameResize();
	~CAudioFrameResize();

public:
	int Init(AVCodecContext* dec_ctx);
	int WriteFrame(AVFrame* frame);
	int ReadFrame(AVFrame* frame);

private:
	AVCodecContext* mOutAVcodec_ctx;
	AVAudioFifo* mFifo;
};

