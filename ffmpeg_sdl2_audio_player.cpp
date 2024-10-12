// References
// [1] https://git.ffmpeg.org/gitweb/ffmpeg.git/blob/HEAD:/doc/examples/demux_decode.c
// [2] https://www.mail-archive.com/ffmpeg-user@ffmpeg.org/msg30274.html

#include <atomic>
#include <chrono>
#include <queue>
#include <stdexcept>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libavutil/timestamp.h>
}

extern "C" {
#define SDL_MAIN_HANDLED
#define __STDC_CONSTANT_MACROS
#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_types.h>
}

#ifdef av_err2str
#undef av_err2str
#include <string>
av_always_inline std::string av_err2string(int errnum)
{
    char str[AV_ERROR_MAX_STRING_SIZE];
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#define av_err2str(err) av_err2string(err).c_str()
#endif // av_err2str

#ifdef av_ts2timestr
#undef av_ts2timestr
#include <string>
av_always_inline std::string av_ts2timestring(int64_t ts, const AVRational *tb)
{
    char str[AV_ERROR_MAX_STRING_SIZE];
    return av_ts_make_time_string2(str, ts, *tb);
}
#define av_ts2timestr(ts, tb) av_ts2timestring(ts, tb).c_str()
#endif // av_ts2timestr

class Processor {
    const char*           src_filename_      = nullptr;
    AVFormatContext*      fmt_ctx_           = nullptr;
    AVCodecContext*       video_dec_ctx_     = nullptr;
    AVCodecContext*       audio_dec_ctx_     = nullptr;
    AVStream*             video_stream_      = nullptr;
    AVStream*             audio_stream_      = nullptr;
    int                   video_stream_idx_  = -1;
    int                   audio_stream_idx_  = -1;
    AVFrame*              frame_             = nullptr;
    AVPacket*             pkt_               = nullptr;
    int                   video_frame_count_ = 0;
    int                   audio_frame_count_ = 0;
    SwrContext*           swr_ctx_           = nullptr;
    SDL_AudioSpec         wanted_spec_;
    std::mutex            mutex_;
    std::queue<uint8_t**> fifo_;
    std::atomic<bool>     is_finished_ = false;

public:
    ~Processor();

    int         process(const char* src_filename);
    friend void sdl_audio_callback(void* udata, Uint8* stream, int len);

private:
    int open_codec_context(int* stream_idx, AVCodecContext** dec_ctx, AVFormatContext* fmt_ctx, enum AVMediaType type);
    int decode_packet(AVCodecContext* dec, const AVPacket* pkt);

    void get_data(uint8_t* buf, int len);
};

void sdl_audio_callback(void* udata, Uint8* stream, int len)
{
    auto processor = (Processor*)udata;
    processor->get_data(stream, len);
}

Processor::~Processor()
{
    avformat_close_input(&fmt_ctx_);
    avcodec_free_context(&audio_dec_ctx_);
    avcodec_free_context(&video_dec_ctx_);
    av_packet_free(&pkt_);
    av_frame_free(&frame_);
    swr_free(&swr_ctx_);

    SDL_CloseAudio();

    while (!fifo_.empty()) {
        uint8_t** a = fifo_.front();
        fifo_.pop();
        av_freep(&a[0]);
    }
}

int Processor::process(const char* src_filename)
{
    int ret = 0;

    src_filename_ = src_filename;

    // open input file, and allocate format context
    if (avformat_open_input(&fmt_ctx_, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        return -1;
    }

    // retrieve stream information
    if (avformat_find_stream_info(fmt_ctx_, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return -1;
    }

    open_codec_context(&video_stream_idx_, &video_dec_ctx_, fmt_ctx_, AVMEDIA_TYPE_VIDEO);
    video_stream_ = fmt_ctx_->streams[video_stream_idx_];

    open_codec_context(&audio_stream_idx_, &audio_dec_ctx_, fmt_ctx_, AVMEDIA_TYPE_AUDIO);
    audio_stream_ = fmt_ctx_->streams[audio_stream_idx_];

    // dump input information to stderr
    av_dump_format(fmt_ctx_, 0, src_filename_, 0);

    if (!audio_stream_ && !video_stream_) {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        return 1;
    }

    swr_alloc_set_opts2(&swr_ctx_,
                        &audio_dec_ctx_->ch_layout,
                        AV_SAMPLE_FMT_S32, // aac encoder only receive this format
                        audio_dec_ctx_->sample_rate,
                        &audio_dec_ctx_->ch_layout,
                        (AVSampleFormat)audio_stream_->codecpar->format,
                        audio_stream_->codecpar->sample_rate,
                        0,
                        nullptr);
    swr_init(swr_ctx_);

    SDL_memset(&wanted_spec_, 0, sizeof(wanted_spec_));
    wanted_spec_.freq     = audio_stream_->codecpar->sample_rate;
    wanted_spec_.format   = AUDIO_S32SYS;
    wanted_spec_.channels = audio_stream_->codecpar->ch_layout.nb_channels;
    wanted_spec_.silence  = 0;
    wanted_spec_.samples  = 4096; // samples per channel. It equals to frame->nb_samples.
    wanted_spec_.callback = sdl_audio_callback;
    wanted_spec_.userdata = this;

    if (SDL_OpenAudio(&wanted_spec_, nullptr) != 0) {
        throw std::runtime_error("fail to open audio device in SDL.");
    }
    SDL_PauseAudio(0);

    frame_ = av_frame_alloc();
    if (!frame_) {
        fprintf(stderr, "Could not allocate frame\n");
        return AVERROR(ENOMEM);
    }

    pkt_ = av_packet_alloc();
    if (!pkt_) {
        fprintf(stderr, "Could not allocate packet\n");
        return AVERROR(ENOMEM);
    }

    // read frames from the file
    while (av_read_frame(fmt_ctx_, pkt_) >= 0) {
        // check if the packet belongs to a stream we are interested in, otherwise skip it
        if (pkt_->stream_index == video_stream_idx_)
            ret = decode_packet(video_dec_ctx_, pkt_);
        else if (pkt_->stream_index == audio_stream_idx_)
            ret = decode_packet(audio_dec_ctx_, pkt_);
        av_packet_unref(pkt_);
        if (ret < 0)
            break;
    }

    // flush the decoders
    if (video_dec_ctx_)
        decode_packet(video_dec_ctx_, NULL);
    if (audio_dec_ctx_)
        decode_packet(audio_dec_ctx_, NULL);

    while (!is_finished_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return ret;
}

void Processor::get_data(uint8_t* buf, int len)
{
    memset(buf, 0, len);
    mutex_.lock();
    if (!fifo_.empty()) {
        uint8_t** data = fifo_.front();
        fifo_.pop();
        mutex_.unlock();
        memcpy(buf, data[0], len);
        av_freep(&data[0]);
    } else {
        mutex_.unlock();
        SDL_PauseAudio(1);
        puts("Play Finish.");
        is_finished_ = true;
    }
}

int Processor::open_codec_context(int* stream_idx, AVCodecContext** dec_ctx, AVFormatContext* fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream* st;
    const AVCodec* dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename_);
        return ret;
    }

    stream_index = ret;
    st = fmt_ctx->streams[stream_index];

    // find decoder for the stream
    dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) {
        fprintf(stderr, "Failed to find %s codec\n",
                av_get_media_type_string(type));
        return AVERROR(EINVAL);
    }

    // Allocate a codec context for the decoder
    *dec_ctx = avcodec_alloc_context3(dec);
    if (!*dec_ctx) {
        fprintf(stderr, "Failed to allocate the %s codec context\n",
                av_get_media_type_string(type));
        return AVERROR(ENOMEM);
    }

    // Copy codec parameters from input stream to output codec context
    if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(type));
        return ret;
    }

    // Init the decoders
    if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
        fprintf(stderr, "Failed to open %s codec\n",
                av_get_media_type_string(type));
        return ret;
    }
    *stream_idx = stream_index;

    return 0;
}

int Processor::decode_packet(AVCodecContext* dec, const AVPacket* pkt)
{
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame_);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;

            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        if (dec->codec->type == AVMEDIA_TYPE_VIDEO)
            printf("video_frame n:%d\n", video_frame_count_++);
        else {
            printf("audio_frame n:%d nb_samples:%d pts:%s\n", audio_frame_count_++, frame_->nb_samples, av_ts2timestr(frame_->pts, &audio_dec_ctx_->time_base));

            uint8_t** cSamples = nullptr;
            ret = av_samples_alloc_array_and_samples(&cSamples, nullptr, audio_dec_ctx_->ch_layout.nb_channels, frame_->nb_samples, AV_SAMPLE_FMT_S32, 0);
            if (ret < 0)
                return ret;
            ret = swr_convert(swr_ctx_, cSamples, frame_->nb_samples, (const uint8_t**)frame_->extended_data, frame_->nb_samples);
            if (ret < 0)
                return ret;

            // 100 packet is about 2s
            while (fifo_.size() > 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            mutex_.lock();
            fifo_.push(cSamples);
            mutex_.unlock();
        }

        av_frame_unref(frame_);
    }

    return ret;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
        return 1;

    if (SDL_Init(SDL_INIT_AUDIO))
        return 1;

    Processor p;
    p.process(argv[1]);

    return 0;
}
