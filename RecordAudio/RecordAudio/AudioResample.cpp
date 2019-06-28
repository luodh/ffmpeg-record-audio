#include "stdafx.h"
#include "AudioResample.h"

CAudioResample::CAudioResample()
{
}

CAudioResample::~CAudioResample()
{
	avfilter_graph_free(&filter_graph);
	filter_graph = nullptr;
	buffersrc_ctx = nullptr;
	buffersink_ctx = nullptr;
}

AVFilterContext* CAudioResample::GetBuffersinkPtr()
{
	return buffersink_ctx;
}

AVFilterContext* CAudioResample::GetBuffersrcoPtr()
{
	return buffersrc_ctx;
}

bool CAudioResample::InitFilter(AVCodecContext* dec_ctx, AVCodecContext* enc_ctx)
{
	if (!dec_ctx || !enc_ctx)
		return false;

	if (dec_ctx->codec_type != AVMEDIA_TYPE_AUDIO)
		return false;

	AVFilterInOut* inputs = avfilter_inout_alloc();
	AVFilterInOut* outputs = avfilter_inout_alloc();
	filter_graph = avfilter_graph_alloc();
	const AVFilter* buffersrc = nullptr;
	const AVFilter* buffersink = nullptr;
	buffersrc_ctx = nullptr;
	buffersink_ctx = nullptr;
	bool result = true;
	char args[512];
	char args2[512];

	if (!inputs || !outputs || !filter_graph)
	{
		result = false;
		goto End;
	}
	buffersrc = avfilter_get_by_name("abuffer");
	buffersink = avfilter_get_by_name("abuffersink");
	if (!buffersrc || !buffersink)
	{
		result = false;
		goto End;
	}
	_snprintf(args, sizeof(args),
		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
		dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
		av_get_sample_fmt_name(dec_ctx->sample_fmt),
		dec_ctx->channel_layout);
	if (avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph) < 0)
	{
		result = false;
		goto End;
	}
	if (avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph)  < 0)
	{
		result = false;
		goto End;
	}

	if (av_opt_set_bin(buffersink_ctx, "sample_fmts", (uint8_t*)&enc_ctx->sample_fmt,
		sizeof(enc_ctx->sample_fmt), AV_OPT_SEARCH_CHILDREN) < 0)
	{
		result = false;
		goto End;
	}
	if (av_opt_set_bin(buffersink_ctx, "sample_rates", (uint8_t*)&enc_ctx->sample_rate,
		sizeof(enc_ctx->sample_rate), AV_OPT_SEARCH_CHILDREN) < 0)
	{
		result = false;
		goto End;
	}
	if (av_opt_set_bin(buffersink_ctx, "channel_layouts", (uint8_t*)&enc_ctx->channel_layout,
		sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN) < 0)
	{
		result = false;
		goto End;
	}
	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;
	if (!inputs->name || !outputs->name)
	{
		result = false;
		goto End;
	}
	if (avfilter_graph_parse_ptr(filter_graph, "anull", &inputs, &outputs, NULL) < 0)
	{
		result = false;
		goto End;
	}
	if (avfilter_graph_config(filter_graph, NULL) < 0)
	{
		result = false;
		goto End;
	}
End:
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);
    
	return result;
}

int CAudioResample::AudioResample_Frame(AVFrame* srcFrame, int flags, AVFrame* destFrame)
{
	if (!srcFrame || !destFrame)
		return -1;

	if (av_buffersrc_add_frame_flags(buffersrc_ctx, srcFrame, flags) < 0)
		return -1;

	if (av_buffersink_get_frame(buffersink_ctx, destFrame) < 0)
		return -1;

	return 0;
}
