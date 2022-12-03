#ifndef PNG2BR_VIDEODECODER_H
#define PNG2BR_VIDEODECODER_H

#include <filesystem>
#include "image.h"

extern "C"
{
    #include <libavutil/imgutils.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/timestamp.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
}

class VideoDecoder
{
    public:
        explicit VideoDecoder(std::filesystem::path& path);
        [[nodiscard]] bool decodeFrame(GImage& image);
        [[nodiscard]] bool hasFrame() const;
        [[nodiscard]] double getPTS() const;
        [[nodiscard]] double getTimeBase() const;
        ~VideoDecoder();

    private:
        bool openCodecContext(int& stream_idx, AVCodecContext*& dec_ctx, enum AVMediaType type);
        int decodePacket(AVCodecContext* dec, const AVPacket* packet);
        void outputAudioFrame(AVFrame* frm);
        void outputVideoFrame(AVFrame* frm);

        bool buffersFlushed = false;
        mutable bool frameReady = false;
        GImage* targetImage = nullptr;
        int64_t frameNum = 0;

        std::filesystem::path path;
        int video_stream_idx = -1;
        int audio_stream_idx = -1;
        SwsContext* swsContext = nullptr;

        AVFormatContext* fmt_ctx = nullptr;
        AVCodecContext* video_dec_ctx = nullptr;
        AVCodecContext* audio_dec_ctx = nullptr;
        AVStream* video_stream = nullptr;
        AVStream* audio_stream = nullptr;

        AVFrame* frame = nullptr;

        AVPacket* packet = nullptr;
};


#endif //PNG2BR_VIDEODECODER_H
