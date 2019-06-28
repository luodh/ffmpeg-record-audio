// RecordAudio.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <DShow.h>
#include <conio.h>
#include "AudioResample.h"
#include "AudioFrameResize.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#ifdef __cplusplus
};
#endif

char *wchar_to_utf8(wchar_t *w)
{
	char *s = NULL;
	int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
	s = (char *)av_malloc(l);
	if (s)
		WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);
	return s;
}

int main()
{
	AVFormatContext*	pFmtInputCtx = NULL, *ofmt_ctx_a = NULL;
	AVInputFormat*	pAudioInputFmt = NULL;
	AVOutputFormat*	pAudioOutputFmt = NULL;
	AVStream*	pOutAudioStream = NULL;
	AVCodecContext*	pOutputCodecCtx = NULL;
	AVCodecContext*	pInputCodecCtx = NULL;
	AVCodec*	pCodec = NULL;
	AVFrame*	pAudioFrame = NULL;
	uint8_t*	pFrameBuffer = NULL;
	int	audioIndex = -1;

	avdevice_register_all();
	
	//使用ffmpeg dshow自带的音频采集设备，可以通过ffmpeg -list_devices true -f dshow -i dummy命令查看
	pAudioInputFmt = av_find_input_format("dshow");
	if (!pAudioInputFmt)
		return -1;
	char * psDevName = wchar_to_utf8(L"audio=virtual-audio-capturer");
	if (avformat_open_input(&pFmtInputCtx, psDevName, pAudioInputFmt, NULL) < 0)
		return -1;

	if (avformat_find_stream_info(pFmtInputCtx, NULL) < 0)
		return -1;

	// 查找音频流
	for (int i = 0; i < (int)pFmtInputCtx->nb_streams; i++)
	{
		if (pFmtInputCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioIndex = i;
			pInputCodecCtx = avcodec_alloc_context3(NULL);
			if (avcodec_parameters_to_context(pInputCodecCtx, pFmtInputCtx->streams[i]->codecpar) < 0)
			{
				return -1;
			}

			AVCodec *decodec = avcodec_find_decoder(pInputCodecCtx->codec_id);
			if (avcodec_open2(pInputCodecCtx, decodec, NULL) < 0)
			{
				return -1;
			}
			break;
		}
	}

	if (!pInputCodecCtx->channel_layout)
		pInputCodecCtx->channel_layout = av_get_default_channel_layout(pInputCodecCtx->channels);

	// 设置音频输出
	char* outAudioFileName = "abc.mp3";
	avformat_alloc_output_context2(&ofmt_ctx_a, NULL, NULL, outAudioFileName);

	pOutAudioStream = avformat_new_stream(ofmt_ctx_a, NULL);
	AVCodec *audioEnCoder = avcodec_find_encoder(ofmt_ctx_a->oformat->audio_codec);

	pOutputCodecCtx = avcodec_alloc_context3(audioEnCoder);

	pOutputCodecCtx->sample_rate = pInputCodecCtx->sample_rate;
	pOutputCodecCtx->channel_layout = pOutputCodecCtx->channel_layout;
	
	if (pOutputCodecCtx->channel_layout == 0)
	{
		pOutputCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
		//pOutputCodecCtx->channels = av_get_channel_layout_nb_channels(pOutputCodecCtx->channel_layout);
	}
	pOutputCodecCtx->channels = av_get_channel_layout_nb_channels(pOutputCodecCtx->channel_layout);
	pOutputCodecCtx->sample_fmt = pOutputCodecCtx->codec->sample_fmts[0];
	AVRational time_base = { 1, pOutputCodecCtx->sample_rate };
	pOutputCodecCtx->time_base = time_base;

	pOutputCodecCtx->codec_tag = 0;
	if (ofmt_ctx_a->oformat->flags & AVFMT_GLOBALHEADER)
		pOutputCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(pOutputCodecCtx, pOutputCodecCtx->codec, 0) < 0)
	{
		return -1;
	}

	int ret = avcodec_parameters_from_context(pOutAudioStream->codecpar, pOutputCodecCtx);
	if (ret < 0)
	{
		return ret;
	}

	if (!(ofmt_ctx_a->oformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&ofmt_ctx_a->pb, outAudioFileName, AVIO_FLAG_WRITE) < 0)
		{
			return -1;
		}
	}

	if (avformat_write_header(ofmt_ctx_a, NULL) < 0)
	{
		return -1;
	}
	// 音频重采样类
	CAudioResample* audioresample = new CAudioResample();
	if (!audioresample->InitFilter(pInputCodecCtx, pOutputCodecCtx))
	{
		return -1;
	}
	// 音频帧缓冲区
	CAudioFrameResize* audioframeresize = new CAudioFrameResize();
	if (audioframeresize->Init(pOutputCodecCtx) < 0)
	{
		return -1;
	}

	AVPacket pkt, pkt_out;
	AVFrame *frame = av_frame_alloc();
	int frameIndex = 0;
	while (av_read_frame(pFmtInputCtx, &pkt) == 0 && _kbhit() == 0)
	{
		pkt_out.data = NULL;
		pkt_out.size = 0;
		pkt_out.stream_index = pkt.stream_index;

		int iRes;
		if ((iRes = avcodec_send_packet(pInputCodecCtx, &pkt)) != 0)
			continue;

		if ((iRes = avcodec_receive_frame(pInputCodecCtx, frame)) != 0)
			continue;

		av_packet_unref(&pkt);

		AVFrame *filter_frame;
		// 当音频采样率、格式、声音通道数不同时需进行重新采样
		if (pInputCodecCtx->sample_fmt != pOutputCodecCtx->sample_fmt
			|| pInputCodecCtx->channels != pOutputCodecCtx->channels
			|| pInputCodecCtx->sample_rate != pOutputCodecCtx->sample_rate)
		{
			filter_frame = av_frame_alloc();

			ret = audioresample->AudioResample_Frame(frame, 0, filter_frame);
			if (ret < 0)
				continue;
		}
		else
		{
			filter_frame = frame;
		}

		// 音频输入源和输出源帧大小可能不一致，需对当前帧或重采样后的帧进行缓冲变换
		if (audioframeresize->WriteFrame(filter_frame) < 0)
			continue;

		if (filter_frame != frame)
		{
			av_frame_free(&filter_frame);
		}	

		AVFrame *frameInput = av_frame_alloc();
		//循环读取数据，直到fifo缓冲区里数据采样数不够
		while (audioframeresize->ReadFrame(frameInput) >= 0)
		{
			ret = avcodec_send_frame(pOutputCodecCtx, frameInput);
			if (ret < 0)
				continue;

			av_init_packet(&pkt_out);
			ret = avcodec_receive_packet(pOutputCodecCtx, &pkt_out);
			if (ret != 0)
				continue;

			pkt_out.pts = frameIndex * pOutputCodecCtx->frame_size;
			pkt_out.dts = frameIndex * pOutputCodecCtx->frame_size;
			pkt_out.duration = pOutputCodecCtx->frame_size;
			av_write_frame(ofmt_ctx_a, &pkt_out);
			av_packet_unref(&pkt_out);
			frameIndex++;
		}
	}
	av_frame_free(&frame);
	av_write_trailer(ofmt_ctx_a);
	avio_close(ofmt_ctx_a->pb);
	avformat_free_context(ofmt_ctx_a);

	delete audioresample;
	delete audioframeresize;
	if (pFmtInputCtx)
	{
		avformat_close_input(&pFmtInputCtx);
		pFmtInputCtx = NULL;
	}
	return 0;
}

