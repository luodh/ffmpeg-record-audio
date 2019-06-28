#pragma once
/*
*  使用过滤器对音频重新采样 
*  本类使用4.13版本ffmpeg
*  2019-6-27
*/
#include <windows.h>
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavfilter\buffersrc.h>
#include <libavfilter\buffersink.h>
#include <libavutil\opt.h>
#include "libavformat/avformat.h"
#include "libavutil/audio_fifo.h"

#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#ifdef __cplusplus
};
#endif


class CAudioResample
{
public:
	CAudioResample();
	~CAudioResample();

public:
	AVFilterContext* GetBuffersinkPtr();
	AVFilterContext* GetBuffersrcoPtr();

	bool InitFilter(AVCodecContext* dec_ctx,AVCodecContext* en_ctx);
	int AudioResample_Frame(AVFrame* srcFrame, int flags, AVFrame* destFrame);

private:
	AVFilterContext* buffersink_ctx;
	AVFilterContext* buffersrc_ctx;
	AVFilterGraph* filter_graph;
};

