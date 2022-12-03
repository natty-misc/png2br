#include "videodecoder.h"
#include "image.h"

#include <sstream>

extern "C" {
    #include <libavcodec/avcodec.h>
}

void VideoDecoder::outputVideoFrame(AVFrame* frm)
{
    this->frameNum++;

    AVFrame* g8Frame = av_frame_alloc();

    this->swsContext = sws_getCachedContext(this->swsContext, frm->width, frm->height, static_cast<AVPixelFormat>(frm->format),
                                            frm->width, frm->height, AV_PIX_FMT_GRAY8,
                                            SWS_BILINEAR, nullptr, nullptr, nullptr);

    this->targetImage->realloc_size(frm->width, frm->height);

    av_image_fill_arrays(g8Frame->data, g8Frame->linesize, this->targetImage->data(), AV_PIX_FMT_GRAY8, frm->width, frm->height, 1);

    sws_scale(this->swsContext, frm->data, frm->linesize, 0, frm->height, g8Frame->data, g8Frame->linesize);

    this->frameReady = true;

    av_frame_free(&g8Frame);
}

void VideoDecoder::outputAudioFrame(AVFrame* frm)
{
    // size_t unpadded_linesize = frm->nb_samples * av_get_bytes_per_sample(static_cast<AVSampleFormat>(frm->format));
    std::string tsString;
    tsString.resize(AV_TS_MAX_STRING_SIZE);
    av_ts_make_time_string(&tsString[0], frm->pts, &audio_dec_ctx->time_base);

    // printf("audio_frame n:%d nb_samples:%d pts:%s\n", audio_frame_count++, frm->nb_samples, tsString.c_str());

    /* Write the raw audio data samples of the first plane. This works
     * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
     * most audio decoders output planar audio, which uses a separate
     * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
     * In other words, this code will write only the first audio channel
     * in these cases.
     * You should use libswresample or libavfilter to convert the frm
     * to packed data. */

    // fwrite(frm->extended_data[0], 1, unpadded_linesize, audio_dst_file);
}

int VideoDecoder::decodePacket(AVCodecContext* dec, const AVPacket* pkt)
{
    int ret;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0)
    {
        char errBuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
        std::stringstream exceptionBuf;
        exceptionBuf << "Error submitting a packet for decoding  (" << errBuf << ")\n";
        throw std::runtime_error(exceptionBuf.str());
    }

    // get all the available frames from the decoder
    while (ret >= 0)
    {
        ret = avcodec_receive_frame(dec, this->frame);
        if (ret < 0)
        {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;

            char errBuf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(errBuf, AV_ERROR_MAX_STRING_SIZE, ret);
            std::stringstream exceptionBuf;
            exceptionBuf << "Error during decoding (" << errBuf << ")\n";
            throw std::runtime_error(exceptionBuf.str());
        }

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO)
            this->outputVideoFrame(this->frame);
        else
            this->outputAudioFrame(this->frame);

        av_frame_unref(this->frame);
    }

    return 0;
}

bool VideoDecoder::openCodecContext(int& stream_idx, AVCodecContext*& dec_ctx, AVMediaType type)
{
    AVStream* st;
    const AVCodec* dec;
    AVDictionary* opts = nullptr;

    int stream_index = av_find_best_stream(this->fmt_ctx, type, -1, -1, nullptr, 0);
    if (stream_index < 0)
    {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(type), this->path.c_str());
        return false;
    }
    else
    {
        st = this->fmt_ctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec)
        {
            std::stringstream exceptionBuf;
            exceptionBuf << "Failed to find " << av_get_media_type_string(type) << " codec context";
            throw std::runtime_error(exceptionBuf.str());
        }

        /* Allocate a codec context for the decoder */
        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx)
        {
            std::stringstream exceptionBuf;
            exceptionBuf << "Failed to allocate the " << av_get_media_type_string(type) << " codec context";
            throw std::runtime_error(exceptionBuf.str());
        }

        /* Copy codec parameters from input stream to output codec context */
        if (avcodec_parameters_to_context(dec_ctx, st->codecpar) < 0)
        {
            std::stringstream exceptionBuf;
            exceptionBuf << "Failed to copy " << av_get_media_type_string(type) << " codec parameters to decoder context";
            throw std::runtime_error(exceptionBuf.str());
        }

        /* Init the decoders */
        if (avcodec_open2(dec_ctx, dec, &opts) < 0)
        {
            std::stringstream exceptionBuf;
            exceptionBuf << "Failed to open " << av_get_media_type_string(type) << " codec";
            throw std::runtime_error(exceptionBuf.str());
        }

        stream_idx = stream_index;
    }

    return true;
}

static int get_format_from_sample_fmt(const char** fmt, enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry
    {
        enum AVSampleFormat sample_fmt;
        const char* fmt_be, * fmt_le;
    } sample_fmt_entries[] = {
            {AV_SAMPLE_FMT_U8,  "u8",    "u8"},
            {AV_SAMPLE_FMT_S16, "s16be", "s16le"},
            {AV_SAMPLE_FMT_S32, "s32be", "s32le"},
            {AV_SAMPLE_FMT_FLT, "f32be", "f32le"},
            {AV_SAMPLE_FMT_DBL, "f64be", "f64le"},
    };
    *fmt = nullptr;

    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++)
    {
        struct sample_fmt_entry* entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt)
        {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }

    fprintf(stderr, "sample format %s is not supported as output format\n", av_get_sample_fmt_name(sample_fmt));
    return -1;
}

VideoDecoder::VideoDecoder(std::filesystem::path& path) : path(path)
{
    std::string pathStr = this->path.string();
    const char* srcFilename = pathStr.c_str();

    /* open input file, and allocate format context */
    if (avformat_open_input(&this->fmt_ctx, srcFilename, nullptr, nullptr) < 0)
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "Could not open source file %s", srcFilename);
        throw std::runtime_error(buf);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(this->fmt_ctx, nullptr) < 0)
    {
        throw std::runtime_error("Could not find stream information");
    }

    if (this->openCodecContext(this->video_stream_idx, this->video_dec_ctx, AVMEDIA_TYPE_VIDEO))
        this->video_stream = this->fmt_ctx->streams[this->video_stream_idx];

    if (this->openCodecContext(this->audio_stream_idx, this->audio_dec_ctx, AVMEDIA_TYPE_AUDIO))
        this->audio_stream = this->fmt_ctx->streams[this->audio_stream_idx];

    /* dump input information to stderr */
    av_dump_format(this->fmt_ctx, 0, srcFilename, 0);

    if (!this->audio_stream && !this->video_stream)
    {
        throw std::runtime_error("Could not find audio or video stream in the input, aborting");
    }

    this->frame = av_frame_alloc();
    if (!this->frame)
    {
        throw std::runtime_error("Could not allocate frame");
    }

    /* initialize packet, set data to NULL, let the demuxer fill it */
    this->packet = av_packet_alloc();
    this->packet->data = nullptr;
    this->packet->size = 0;
}

VideoDecoder::~VideoDecoder()
{
    sws_freeContext(this->swsContext);

    avcodec_free_context(&this->video_dec_ctx);
    avcodec_free_context(&this->audio_dec_ctx);
    avformat_close_input(&this->fmt_ctx);

    av_packet_free(&this->packet);
    av_frame_free(&this->frame);
}

bool VideoDecoder::decodeFrame(GImage& image)
{
    if (buffersFlushed)
        return false;

    int ret = av_read_frame(this->fmt_ctx, this->packet);
    this->targetImage = &image;

    if (ret < 0)
        return false;

    if (this->packet->stream_index == this->video_stream_idx)
        ret = this->decodePacket(this->video_dec_ctx, this->packet);

    else if (this->packet->stream_index == this->audio_stream_idx)
        ret = this->decodePacket(this->audio_dec_ctx, this->packet);

    av_packet_unref(this->packet);

    if (ret >= 0)
        return true;

    if (this->video_dec_ctx)
        this->decodePacket(this->video_dec_ctx, nullptr);

    if (this->audio_dec_ctx)
        this->decodePacket(this->audio_dec_ctx, nullptr);

    this->buffersFlushed = true;
    return true;
}

bool VideoDecoder::hasFrame() const
{
    bool hadFrame = this->frameReady;
    this->frameReady = false;
    return hadFrame;
}

double VideoDecoder::getPTS() const
{
    if (this->frame->best_effort_timestamp != AV_NOPTS_VALUE)
        return static_cast<double>(this->frame->best_effort_timestamp);

    if (this->packet->pts != AV_NOPTS_VALUE)
        return static_cast<double>(this->packet->pts);

    if (this->packet->dts != AV_NOPTS_VALUE)
        return static_cast<double>(this->packet->dts);

    return static_cast<double>(this->frameNum) / av_q2d(this->video_stream->r_frame_rate) / av_q2d(this->video_stream->time_base);
}

double VideoDecoder::getTimeBase() const
{
    return this->video_stream->time_base.num / static_cast<double>(this->video_stream->time_base.den);
}
