#include <iostream>
#include <stdio.h>
#include <string>
#include <fstream>
#pragma warning(disable:4996)
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/dict.h>
    #include <libavdevice/avdevice.h>
    #include <libavutil/log.h>
    #include <libavutil/error.h>
}
//Global variables
int64_t TOTAL_FRAMES = 0;
unsigned int CLIP_TIME_SEC = 3;
unsigned int NUM_CLIPS = 1;
unsigned int OUTPUT_TIME = 1;
unsigned int CLIP_FRAMES = CLIP_TIME_SEC * 60; //The number of frames in a clip. Testing purpuse = 5. 

bool withinRange(int frame_number, int startIdx, int endIdx) {
    if (frame_number > startIdx && frame_number < endIdx) {
        return true;
    }
}

//encode frame
void encode_frame(AVCodecContext* codec_ctx2, AVFrame* frame, AVPacket* write_pkt, FILE* outfile) {
    int ret;
    //Send framee to the codec.
    ret = avcodec_send_frame(codec_ctx2, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret == AVERROR(EINVAL)) {
        av_log(NULL, AV_LOG_ERROR, std::to_string(ret).c_str());
        return;
    }
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error during sending packet\n");
        return;
    }

    //Receive packet from the codec.
    ret = avcodec_receive_packet(codec_ctx2, write_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, std::to_string(ret).c_str());
        return;
    }
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error during receiving packet\n");
        return;
    }
    std::cout << "Write packet: " << write_pkt->pts << " " << write_pkt->size << std::endl;
    fwrite(write_pkt->data, 1, write_pkt->size, outfile);
    av_packet_unref(write_pkt);
}

//Takes in a video packet and decodes it into frame. 
void decode_packet(AVPacket* avpkt, AVCodecContext* avctx, AVFrame *frame, int &startIdx, int&endIdx, int &rangeLow, int &rangeHigh, int & clipsCreated, AVCodecContext* codec_ctx2, FILE* f) {
    int ret;
    //Send packet to the vcodec.
    ret = avcodec_send_packet(avctx, avpkt);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error Seding a packet\n");
        return;
    } 
    //Receive the frame from vcodec. 
    ret = avcodec_receive_frame(avctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_log(NULL, AV_LOG_ERROR, std::to_string(ret).c_str());
        return;
    }
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error during decoding\n");
        return;
    }
    //std::cout << "Frames pts: " << frame->pts << std::endl;
    //std::cout << "Frames returned from decoder so far: " << avctx->frame_number << std::endl;
    //std::cout << "Frame presentation time stamp: " << frame->pts << std::endl; 
    //std::cout << "Coded Picture Number: " << frame->coded_picture_number << std::endl;
    //std::cout << "Picture Type: " << av_get_picture_type_char(frame->pict_type) << std::endl;
    //std::cout << "-------------------------" << std::endl;
    //free reference
    if (avctx->frame_number >= startIdx && avctx->frame_number < endIdx) {
        //encode the following frame
        std::cout << "Frame needed: " << avctx->frame_number << std::endl;
        encode_frame(codec_ctx2, frame, avpkt, f);
    }
    if (avctx->frame_number >= endIdx) {
        rangeLow = rangeHigh;
        rangeHigh = rangeHigh + ((TOTAL_FRAMES / NUM_CLIPS) - CLIP_FRAMES);
        startIdx = rand() % (rangeHigh - rangeLow + 1) + rangeLow;
        endIdx = startIdx + CLIP_FRAMES;
        clipsCreated++;
        std::cout << "Clips generated: " << clipsCreated << std::endl;
    }
    av_frame_unref(frame);
};



int main()
{
    srand(time(0));
    AVFormatContext* fmt_ctx = avformat_alloc_context();
    AVDictionaryEntry* tag = NULL;

    //Register all muxers and demuxers. 
    avdevice_register_all();

    int ret;
    char fileName[] = "test_video.mp4";

    //Open file and read the metada. 
    ret = avformat_open_input(&fmt_ctx, fileName, NULL, NULL);
    if (ret < 0) {
        printf("Could not open file\n");
    }

    //Read stream info.
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
    }

    //Find the video and audio stream indexes. 
    int videoStreamIdx = -1;
    int audioStreamIdx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        auto params = fmt_ctx->streams[i]->codecpar;
        if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
        }
        if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = i;
        }
    }

    //Configure
    TOTAL_FRAMES = fmt_ctx->streams[videoStreamIdx]->nb_frames;


    //Framerate of video
    std::cout << "FPS(num): " << fmt_ctx->streams[videoStreamIdx]->r_frame_rate.num << std::endl;
    std::cout << "FPS(den): " << fmt_ctx->streams[videoStreamIdx]->r_frame_rate.den << std::endl;

    //Initialize vcodec and vcodec Context
    AVCodec* vcodec = NULL;
    AVCodecContext* codec_ctx = NULL;
    AVCodecParameters* codec_params = avcodec_parameters_alloc();
    codec_params = fmt_ctx->streams[0]->codecpar;
    //figures out the vcodec used. 
    vcodec = avcodec_find_decoder(codec_params->codec_id);
    
    if (!vcodec) {
        av_log(NULL, AV_LOG_ERROR, "Could not Find Decoder\n");
    }
    else {
        //alocate memory for AVCodecContext
        codec_ctx = avcodec_alloc_context3(vcodec);
        if (!codec_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Could not create AVCodecContext\n");
        }
        else {
            std::cout << "Created AVCodecContext" << std::endl;
            //Initialize for AVCodecContext
            if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Could not initialize AVCodecContext\n");
            }
            else {
                std::cout << "Initialzed AVCodecContext" << std::endl;
                //Open AVCodecContext
                if (avcodec_open2(codec_ctx, vcodec, NULL) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "could not open AVCodecContext\n");
                }
            }
        }
    }
    
    int clipsCreated = 0;
    AVPacket* packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    //Receive packet by reading the frame. //This is where the pipeline starts. 
    int startIdx;
    int endIdx;
    int rangeLow =  0;
    int rangeHigh = (TOTAL_FRAMES / NUM_CLIPS) - CLIP_FRAMES;
    int packetIdx = 0;

    startIdx = rand() % (rangeHigh - rangeLow + 1) + rangeLow;
    endIdx = startIdx + CLIP_FRAMES;

    //Create the second codec context. 
    AVCodecContext* codec_ctx2 = NULL;
    AVCodec* encoder = avcodec_find_encoder(codec_params->codec_id);
    codec_ctx2 = avcodec_alloc_context3(encoder);
    avcodec_parameters_to_context(codec_ctx2, codec_params);


    //Frame rate
    codec_ctx2->time_base = AVRational{1, 60};
    codec_ctx2->framerate = AVRational{ 60, 1 };
    //bitrate
    codec_ctx2->bit_rate = codec_ctx->bit_rate;
    //resolution
    codec_ctx2->width = codec_ctx->width;
    codec_ctx2->height = codec_ctx->height;

    codec_ctx2->gop_size = 10;
    codec_ctx2->max_b_frames = 1;
    codec_ctx2->pix_fmt = AV_PIX_FMT_YUV420P;

    if (encoder->id == AV_CODEC_ID_H264)
        av_opt_set(codec_ctx2->priv_data, "preset", "slow", 0);



    if (avcodec_open2(codec_ctx2, encoder, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "could not open Encoder context\n");
        exit(1);
    }


    FILE* f;
    f = fopen("output.mp4", "wb");
    if (!f) {
        std::cout << "Could not open file" << std::endl;
        exit(1);
    }
    
    
   while (av_read_frame(fmt_ctx, packet) >= 0 && clipsCreated < NUM_CLIPS) {
        if (packet->stream_index == videoStreamIdx)
        {
            decode_packet(packet, codec_ctx, frame, startIdx, endIdx, rangeLow, rangeHigh, clipsCreated, codec_ctx2, f);
         
        }
        av_packet_unref(packet);
    };

    std::cout << "---------------" << std::endl;

    //Flush the decoder at end of stream. 
    //decode_packet(NULL, codec_ctx, frame, startIdx, endIdx, rangeLow, rangeHigh, clipsCreated, codec_ctx2, f);

    //avcodec_parameters_free(&codec_params); //free allocated space for AVCodecParameters
    //avcodec_free_context(&codec_ctx); //Free allocated space AVCodecContext
    //av_frame_free(&frame); //Free allocated space for AVFrame
    //avformat_close_input(&fmt_ctx); //Free allocated space for AVFormatContext
    return 0;
}