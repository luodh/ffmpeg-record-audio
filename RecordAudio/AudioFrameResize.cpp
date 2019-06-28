#include "AudioFrameResize.h"

CAudioFrameResize::CAudioFrameResize()
{
}

CAudioFrameResize::~CAudioFrameResize()
{
	if (mFifo)
		av_audio_fifo_free(mFifo);
}

int CAudioFrameResize::Init(AVCodecContext* dec_ctx)
{
	if (!dec_ctx)
		return -1;

	mOutAVcodec_ctx = dec_ctx;
	mFifo = av_audio_fifo_alloc(dec_ctx->sample_fmt, dec_ctx->channels, 1);
	if (!mFifo)
		return -1;
	return 0;
}

int CAudioFrameResize::WriteFrame(AVFrame* frame)
{
	if (!frame)
		return -1;

	return av_audio_fifo_write(mFifo, (void**)frame->data, frame->nb_samples);
}

int CAudioFrameResize::ReadFrame(AVFrame* frame)
{
	if (frame)
		av_frame_free(&frame);
	frame = av_frame_alloc();

	if (av_audio_fifo_size(mFifo) < (mOutAVcodec_ctx->frame_size > 0 ? mOutAVcodec_ctx->frame_size : 1024))
		return -1;

	frame->nb_samples = mOutAVcodec_ctx->frame_size > 0 ? mOutAVcodec_ctx->frame_size : 1024;
	frame->channel_layout = mOutAVcodec_ctx->channel_layout;
	frame->format = mOutAVcodec_ctx->sample_fmt;
	frame->sample_rate = mOutAVcodec_ctx->sample_rate;
	av_frame_get_buffer(frame, 0);

	int ret = av_audio_fifo_read(mFifo, (void **)frame->data, (mOutAVcodec_ctx->frame_size > 0 ? mOutAVcodec_ctx->frame_size : 1024));
	return ret;
}