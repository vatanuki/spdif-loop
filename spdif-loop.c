#define _GNU_SOURCE

#include <stdio.h>
#include <signal.h>
#include <getopt.h>

#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libswresample/swresample_internal.h> //need to remove this

#define SPDIF_SYNCWORD 0x72F81F4E
#define SPDIF_IN_CODEC_PROBESIZE 4096

#define ALSA_IO_BUFFER_SIZE 512
#define PCM_DETECT_DELAY_SIZE 512*1024

typedef struct looper_data_s {
	AVPacket in_pkt;
	int in_pkt_offset;
	int in_pcm_mode;

	AVInputFormat *alsa_fmt;
	AVInputFormat *spdif_fmt;

	AVFormatContext *in_alsa_ctx;
	AVFormatContext *in_spdif_ctx;
	AVCodecContext *in_codec_ctx;

	SwrContext *swr_ctx;

	AVFormatContext *out_ctx;
	AVStream *out_stream;
} looper_data_t;

static int verbose = 0;
static looper_data_t ld = {0};

static void usage(char *self){
	fprintf(stderr, "Usage: %s [-v verbose] -i <input> -o <output>\n", self);
	exit(-1);
}

static int cleanup_spdif(looper_data_t *ld, int err){
	avcodec_free_context(&ld->in_codec_ctx);
	if(ld->in_spdif_ctx){
		if(ld->in_spdif_ctx->pb){
			ld->in_pkt_offset = 0;
			av_packet_unref(&ld->in_pkt);
			av_freep(&ld->in_spdif_ctx->pb->buffer);
			avio_context_free(&ld->in_spdif_ctx->pb);
		}
		avformat_close_input(&ld->in_spdif_ctx);
	}
	return err;
}

static int cleanup(looper_data_t *ld, int err){
	swr_free(&ld->swr_ctx);
	cleanup_spdif(ld, err);
	if(ld->out_ctx){
		avformat_free_context(ld->out_ctx);
		ld->out_ctx = NULL;
	}
	avformat_close_input(&ld->in_alsa_ctx);
	return err;
}

static void die(int nsig){
	fprintf(stderr, "terminate, start cleanup");
	exit(0);
}

static void sf(int nsig){
	fprintf(stderr, "SEGMENTATION FAULT");
	exit(-1);
}

static int decode_audio_frame(looper_data_t *ld, AVFrame **frame){
	int err;
	AVPacket pkt;

	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if(!(*frame = av_frame_alloc())){
		err = AVERROR(ENOMEM);
		av_log(ld->in_codec_ctx, AV_LOG_ERROR, "cannot allocate frame\n");
		goto cleanup;
	}

	if((err = av_read_frame(ld->in_spdif_ctx, &pkt)) < 0){
		av_log(ld->in_spdif_ctx, AV_LOG_ERROR, "cannot read frame: %s\n", av_err2str(err));
		goto cleanup;
	}

	if((err = avcodec_send_packet(ld->in_codec_ctx, &pkt)) < 0){
		av_log(ld->in_codec_ctx, AV_LOG_ERROR, "cannot send packet: %s\n", av_err2str(err));
		goto cleanup;
	}

	if((err = avcodec_receive_frame(ld->in_codec_ctx, *frame)) < 0){
		av_log(ld->in_codec_ctx, AV_LOG_ERROR, "cannot receive frame: %s\n", av_err2str(err));
		goto cleanup;
	}

	ld->in_pcm_mode = 0;

cleanup:
	av_packet_unref(&pkt);
	return err;
}

static int convert_and_write(looper_data_t *ld, int in_sample_rate, int64_t in_ch_layout, int in_sample_fmt, const uint8_t **in_samples, int in_nb_samples){
	AVPacket pkt;
	uint8_t **out_samples = NULL;
	int err, out_nb_samples;

	if((!ld->swr_ctx
	|| in_ch_layout != ld->swr_ctx->in_ch_layout
	|| in_sample_rate != ld->swr_ctx->in_sample_rate
	|| in_sample_fmt != ld->swr_ctx->in_sample_fmt)){
		if(ld->swr_ctx)
			av_log(ld->swr_ctx, AV_LOG_INFO, "resampler reinit: %d Hz, %d (%ld) ch, %s -> %d Hz, %d (%ld) ch, %s\n",
				ld->swr_ctx->in_sample_rate, av_get_channel_layout_nb_channels(ld->swr_ctx->in_ch_layout), ld->swr_ctx->in_ch_layout,
				av_get_sample_fmt_name(ld->swr_ctx->in_sample_fmt), in_sample_rate, av_get_channel_layout_nb_channels(in_ch_layout),
				in_ch_layout, av_get_sample_fmt_name(in_sample_fmt));

		if(!(ld->swr_ctx = swr_alloc_set_opts(ld->swr_ctx,
			ld->out_stream->codecpar->channel_layout, ld->out_stream->codecpar->format, ld->out_stream->codecpar->sample_rate,
			in_ch_layout, in_sample_fmt, in_sample_rate, 0, NULL))){
				av_log(ld->out_ctx, AV_LOG_ERROR, "cannot allocate swr context\n");
				return AVERROR(ENOMEM);
			}

		if((err = swr_init(ld->swr_ctx)) < 0){
			swr_free(&ld->swr_ctx);
			av_log(ld->out_ctx, AV_LOG_ERROR, "cannot init swr: %s\n", av_err2str(err));
			return err;
		}
	}

	out_nb_samples = av_rescale_rnd(in_nb_samples, ld->out_stream->codecpar->sample_rate, in_sample_rate, AV_ROUND_UP);
	if((err = av_samples_alloc_array_and_samples(&out_samples, NULL, ld->out_stream->codecpar->channels, out_nb_samples, ld->out_stream->codecpar->format, 1)) < 0){
		av_log(ld->out_ctx, AV_LOG_ERROR, "cannot allocate %u samples: %s\n", out_nb_samples, av_err2str(err));
		goto cleanup;
	}

	if((err = swr_convert(ld->swr_ctx, out_samples, out_nb_samples, in_samples, in_nb_samples)) < 0){
		av_log(ld->swr_ctx, AV_LOG_ERROR, "cannot convert samples: %s\n", av_err2str(err));
		goto cleanup;
	}

	av_init_packet(&pkt);
	pkt.data = out_samples[0];
	pkt.size = av_samples_get_buffer_size(NULL, ld->out_stream->codecpar->channels, err, ld->out_stream->codecpar->format, 1);
	if((err = ld->out_ctx->oformat->write_packet(ld->out_ctx, &pkt)) < 0)
		av_log(ld->out_ctx, AV_LOG_ERROR, "cannot write packet: %s\n", av_err2str(err));

cleanup:
	if(out_samples){
		av_freep(&out_samples[0]);
		free(out_samples);
	}

	return err;
}

static int alsa_reader(void *data, uint8_t *buf, int buf_size){
	looper_data_t *ld = data;
	int readed = 0, tmp;
	AVCodecParameters *cp;
	uint8_t *in_samples[2];
	uint32_t state;

	while(buf_size > 0){
		if(!ld->in_pkt.size && (tmp = av_read_frame(ld->in_alsa_ctx, &ld->in_pkt)) < 0)
			return tmp;

		tmp = FFMIN(buf_size, ld->in_pkt.size - ld->in_pkt_offset);
		memcpy(buf, ld->in_pkt.data + ld->in_pkt_offset, tmp);
		buf+= tmp;
		buf_size-= tmp;
		readed+= tmp;
		ld->in_pkt_offset+= tmp;

		if(ld->in_pkt_offset >= ld->in_pkt.size){
			av_packet_unref(&ld->in_pkt);
			ld->in_pkt_offset = 0;
		}
	}

	if(ld->in_pcm_mode >= PCM_DETECT_DELAY_SIZE){
		if(readed > 3){
			buf-= readed;
			cp = ld->in_alsa_ctx->streams[0]->codecpar;
			for(tmp = 0, state = 0; tmp < readed; tmp++){
				state = (state << 8) | buf[tmp];
				if(state == SPDIF_SYNCWORD){
					cp = 0;
					ld->in_pcm_mode = 0;
					av_log(ld->in_alsa_ctx, AV_LOG_INFO, "SPDIF SYNC found, stop PCM loop\n");
					break;
				}
			}
			if(cp && cp->channels > 0 && cp->channels < 3){
				in_samples[0] = buf;
				in_samples[1] = NULL;
				convert_and_write(ld, cp->sample_rate, av_get_default_channel_layout(cp->channels), cp->format, (const uint8_t **)&in_samples, readed / cp->frame_size);
			}
		}
	}else
		ld->in_pcm_mode+= readed;

	return readed;
}

static int init_input(looper_data_t *ld, const char *in_dev_name){
	int err;

	if((err = avformat_open_input(&ld->in_alsa_ctx, in_dev_name, ld->alsa_fmt, NULL)) < 0){
		av_log(NULL, AV_LOG_ERROR, "cannot open alsa input: %s\n", av_err2str(err));
		return cleanup(ld, err);
	}

	if((err = avformat_find_stream_info(ld->in_alsa_ctx, NULL)) < 0){
		av_log(NULL, AV_LOG_ERROR, "cannot find stream info: %s\n", av_err2str(err));
		return cleanup(ld, err);
	}

	if(verbose)
		av_dump_format(ld->in_alsa_ctx, 0, in_dev_name, 0);

	return 0;
}

static int init_output(looper_data_t *ld, const char *out_dev_name, int format, int sample_rate, int channels, int64_t channel_layout){
	int err;

	if((err = avformat_alloc_output_context2(&ld->out_ctx, NULL, "alsa", out_dev_name)) < 0){
		av_log(NULL, AV_LOG_ERROR, "cannot open alsa %s output: %s\n", out_dev_name, av_err2str(err));
		return cleanup(ld, err);
	}

	if(!(ld->out_stream = avformat_new_stream(ld->out_ctx, NULL))){
		av_log(ld->out_ctx, AV_LOG_ERROR, "cannot allocate stream\n");
		return cleanup(ld, AVERROR(ENOMEM));
	}

	ld->out_stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
	ld->out_stream->codecpar->codec_id = ld->out_ctx->oformat->audio_codec;
	ld->out_stream->codecpar->channels = channels;
	ld->out_stream->codecpar->channel_layout = channel_layout;
	ld->out_stream->codecpar->sample_rate = sample_rate;
	ld->out_stream->codecpar->format = format;

	if((err = avformat_write_header(ld->out_ctx, NULL)) < 0){
		av_log(ld->out_ctx, AV_LOG_ERROR, "cannot write header: %s\n", av_err2str(err));
		return cleanup(ld, err);
	}

	if(verbose)
		av_dump_format(ld->out_ctx, 0, out_dev_name, 1);

	return 0;
}

static int init_spdif(looper_data_t *ld){
	AVCodec *codec;
	uint8_t *buffer;
	int err;

	av_init_packet(&ld->in_pkt);

	if(!(ld->in_spdif_ctx = avformat_alloc_context())){
		av_log(NULL, AV_LOG_ERROR, "cannot allocate spdif context\n");
		return cleanup_spdif(ld, AVERROR(ENOMEM));
	}

	ld->in_spdif_ctx->iformat = ld->spdif_fmt;
	ld->in_spdif_ctx->flags|= AVFMT_FLAG_CUSTOM_IO;
	ld->in_spdif_ctx->ctx_flags|= AVFMTCTX_NOHEADER;
	ld->in_spdif_ctx->duration = ld->in_spdif_ctx->start_time = AV_NOPTS_VALUE;

	if(!(buffer = av_malloc(ALSA_IO_BUFFER_SIZE))){
		av_log(ld->in_spdif_ctx, AV_LOG_ERROR, "cannot allocate input buffer\n");
		return cleanup_spdif(ld, AVERROR(ENOMEM));
	}

	if(!(ld->in_spdif_ctx->pb = avio_alloc_context(buffer, ALSA_IO_BUFFER_SIZE, 0, ld, alsa_reader, NULL, NULL))){
		av_freep(&buffer);
		av_log(ld->in_spdif_ctx, AV_LOG_ERROR, "cannot allocate alsa io context");
		return cleanup_spdif(ld, AVERROR(ENOMEM));
	}

	ld->in_spdif_ctx->probesize = SPDIF_IN_CODEC_PROBESIZE;
	if((err = avformat_find_stream_info(ld->in_spdif_ctx, NULL)) < 0){
		av_log(ld->in_spdif_ctx, AV_LOG_WARNING, "cannot find stream info: %s\n", av_err2str(err));
		return cleanup_spdif(ld, err);
	}
	avformat_flush(ld->in_spdif_ctx);

	if(verbose)
		av_dump_format(ld->in_spdif_ctx, 0, "alsa", 0);

	if(ld->in_spdif_ctx->nb_streams != 1){
		av_log(ld->in_spdif_ctx, AV_LOG_WARNING, "expected one audio input stream, but found %d\n", ld->in_spdif_ctx->nb_streams);
		return cleanup_spdif(ld, AVERROR_EXIT);
	}

	if(!(codec = avcodec_find_decoder(ld->in_spdif_ctx->streams[0]->codecpar->codec_id))){
		av_log(ld->in_spdif_ctx, AV_LOG_WARNING, "cannot find input codec: %d\n", ld->in_spdif_ctx->streams[0]->codecpar->codec_id);
		return cleanup_spdif(ld, AVERROR_EXIT);
	}

	if(!(ld->in_codec_ctx = avcodec_alloc_context3(codec))){
		av_log(ld->in_spdif_ctx, AV_LOG_WARNING, "cannot allocate input codec context\n");
		return cleanup_spdif(ld, AVERROR(ENOMEM));
	}

	if((err = avcodec_parameters_to_context(ld->in_codec_ctx, ld->in_spdif_ctx->streams[0]->codecpar)) < 0){
		av_log(ld->in_codec_ctx, AV_LOG_WARNING, "cannot move parameters to input codec: %s\n", av_err2str(err));
		return cleanup_spdif(ld, err);
	}

	if((err = avcodec_open2(ld->in_codec_ctx, codec, NULL)) < 0){
		av_log(ld->in_codec_ctx, AV_LOG_WARNING, "cannot open input codec: %s\n", av_err2str(err));
		return cleanup_spdif(ld, err);
	}

	return 0;
}

int main(int argc, char **argv){
	int err;
	char *in_dev_name = "hw:1";
	char *out_dev_name = "hw:1";
	AVFrame *frame = NULL;

	for(int opt = 0; (opt = getopt(argc, argv, "i:o:v")) != -1;){
		switch (opt){
		case 'i':
			in_dev_name = optarg;
			break;
		case 'o':
			out_dev_name = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(argv[0]);
		}
	}

	//signals
	signal(SIGINT, die);
	signal(SIGQUIT, die);
	signal(SIGTERM, die);
	signal(SIGSEGV, sf);
	signal(SIGPIPE, SIG_IGN);

	if(verbose){
		av_log_set_flags(av_log_get_flags()|AV_LOG_PRINT_LEVEL|AV_LOG_SKIP_REPEATED);
		av_log_set_level(AV_LOG_TRACE);
	}

	avdevice_register_all();

	//FORMAT
	if(!(ld.alsa_fmt = av_find_input_format("alsa"))){
		av_log(NULL, AV_LOG_ERROR, "cannot find alsa input driver\n");
		return -1;
	}

	if(!(ld.spdif_fmt = av_find_input_format("spdif"))){
		av_log(NULL, AV_LOG_ERROR, "cannot find spdif demux driver\n");
		return -1;
	}

	//INPUT
	if((err = init_input(&ld, in_dev_name)) < 0){
		av_log(NULL, AV_LOG_ERROR, "cannot init input: %s\n", av_err2str(err));
		return cleanup(&ld, err);
	}

	//OUTPUT
	if((err = init_output(&ld, out_dev_name, AV_SAMPLE_FMT_S16, 48000, 6, av_get_default_channel_layout(6))) < 0){
		av_log(NULL, AV_LOG_ERROR, "cannot init output: %s\n", av_err2str(err));
		return cleanup(&ld, err);
	}

	//LOOP
	while(1){

		//SPDIF
		if(!ld.in_spdif_ctx && (err = init_spdif(&ld)) < 0){
			av_log(NULL, AV_LOG_ERROR, "cannot init spdif: %s\n", av_err2str(err));
			return cleanup(&ld, err);
		}

		if(!(err = decode_audio_frame(&ld, &frame)))
			err = convert_and_write(&ld, frame->sample_rate, frame->channel_layout, frame->format, (const uint8_t**)frame->extended_data, frame->nb_samples);
		else
			cleanup_spdif(&ld, err);

		av_frame_free(&frame);
	}
}
