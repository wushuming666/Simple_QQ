// gcc -g -o merge merge.c `pkg-config --libs libavformat libavutil` -lavcodec
#include <stdio.h>
#include <stdlib.h>
#include <libavutil/log.h>
#include <libavutil/timestamp.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
 
// 分别抽取 合并 剪切
// ./avmerge 1.mp4 2.mp4 3.mp4 5 15
int main(int argc, char *argv[])
{
    // 1、处理一些参数
    char *src1;  // 源1文件名
    char *src2;  // 源2文件名
    char *dst;   // 目标文件名
    double starttime = 0; // 剪切起始时间
    double endtime = 0;    // 剪切结束时间
    int idx1 = -1;  // 1视频流的下标
    int idx2 = -1;  // 2音频流的下标
    AVFormatContext *pFmtCtx1 = NULL; // 多媒体1的上下文
    AVFormatContext *pFmtCtx2 = NULL; // 多媒体2的上下文
    AVFormatContext *oFmtCtx = NULL; 
    AVOutputFormat *outFmt = NULL;
    AVStream *outStream1 = NULL;
    AVStream *outStream2 = NULL;
    AVStream *inStream1 = NULL;
    AVStream *inStream2 = NULL;
    AVPacket pkt;
    int64_t *dts_start_time = NULL;
    int64_t *pts_start_time = NULL;
 
    int ret = -1;
 
    av_log_set_level(AV_LOG_INFO);
    if (argc != 6)
    {
        av_log(NULL, AV_LOG_INFO, "example: ./avmerge 1.mp4 2.mp4 3.mp4 5 15\n");
        exit(-1);
    }
    src1 = argv[1];
    src2 = argv[2];
    dst = argv[3];
    starttime = atof(argv[4]);
    endtime = atof(argv[5]);
 
    // 2、将两个多媒体文件打开
    ret = avformat_open_input(&pFmtCtx1, src1, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "open file 1 error %s\n", av_err2str(ret));
        goto _ERROR;
    }
    ret = avformat_open_input(&pFmtCtx2, src2, NULL, NULL);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "open file 2 error %s\n", av_err2str(ret));
        goto _ERROR;
    }
 
    // 3、打开目标文件的上下文
    oFmtCtx = avformat_alloc_context();
    if(!oFmtCtx){
        av_log(NULL, AV_LOG_ERROR, "NO MEMORY!\n");
        goto _ERROR;
    }
    outFmt = av_guess_format(NULL, dst, NULL);
    oFmtCtx->oformat = outFmt;      // 格式的基本信息
 
    // 为源1创建一个新的视频流  源2音频流  再将这两个流剪切、再合并（依次写入）
 
    // 4、从1里面找到视频流、从2里面找到音频流
    idx1 = av_find_best_stream(pFmtCtx1, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if(idx1 < 0) {
        av_log(pFmtCtx1, AV_LOG_ERROR, "Does not include audio stream!\n");
        goto _ERROR;
    } 
    idx2 = av_find_best_stream(pFmtCtx2, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if(idx2 < 0) {
        av_log(pFmtCtx2, AV_LOG_ERROR, "Does not include audio stream!\n");
        goto _ERROR;
    } 
 
    // 5、为目的文件 创建一个视频流和一个音频流
    outStream1 = avformat_new_stream(oFmtCtx, NULL);
    outStream2 = avformat_new_stream(oFmtCtx, NULL);
 
    // 6、设置视频参数 和 音频参数
    inStream1 = pFmtCtx1->streams[idx1];
    avcodec_parameters_copy(outStream1->codecpar, inStream1->codecpar);
    outStream1->codecpar->codec_tag = 0;
 
    inStream2 = pFmtCtx2->streams[idx2];
    avcodec_parameters_copy(outStream2->codecpar, inStream2->codecpar);
    outStream2->codecpar->codec_tag = 0;
 
    // 绑定
    ret = avio_open2(&oFmtCtx->pb, dst, AVIO_FLAG_WRITE, NULL, NULL);
    if (ret < 0)
    {
        av_log(oFmtCtx, AV_LOG_ERROR, "%s", av_err2str(ret));
        goto _ERROR;
    }
 
    // 7、写多媒体头到目标文件
    ret = avformat_write_header(oFmtCtx, NULL);
    if (ret < 0)
    {
        av_log(oFmtCtx, AV_LOG_ERROR, "%s", av_err2str(ret));
        goto _ERROR;
    }
 
    //seek
    ret = av_seek_frame(pFmtCtx1, -1, starttime*AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
    if(ret < 0) {
        av_log(oFmtCtx, AV_LOG_ERROR, "%s", av_err2str(ret));
        goto _ERROR;
    }
    ret = av_seek_frame(pFmtCtx2, -1, starttime*AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
    if(ret < 0) {
        av_log(oFmtCtx, AV_LOG_ERROR, "%s", av_err2str(ret));
        goto _ERROR;
    }
 
    // 8、从源多媒体文件中将视频、音频 写到目的文件中   并剪切
    int dts_start_time1 = -1;
    int pts_start_time1 = -1;
    int dts_start_time2 = -1;
    int pts_start_time2 = -1;
    while (av_read_frame(pFmtCtx1, &pkt) >= 0)
    {
        AVStream *inStream, *outStream;
        if (pkt.stream_index == idx1)
        {
            if (dts_start_time1 == -1 && pkt.dts >= 0) dts_start_time1 = pkt.dts;
            if (pts_start_time1 == -1 && pkt.pts >= 0) pts_start_time1 = pkt.pts;
            pkt.stream_index = 0;
            inStream = pFmtCtx1->streams[idx1];
            if(av_q2d(inStream->time_base) * pkt.pts > endtime) {
                av_log(oFmtCtx, AV_LOG_INFO, "success!\n");
                break;
            }
 
            pkt.pts = pkt.pts - pts_start_time1;
            pkt.dts = pkt.dts - dts_start_time1;
 
            if (pkt.dts > pkt.pts)
            {
                pkt.pts = pkt.dts;
            }
 
            outStream = oFmtCtx->streams[pkt.stream_index];
            av_packet_rescale_ts(&pkt, inStream->time_base, outStream->time_base);
            pkt.pos = -1;
            av_interleaved_write_frame(oFmtCtx, &pkt);
            av_packet_unref(&pkt);
        }
    }
    while (av_read_frame(pFmtCtx2, &pkt) >= 0)
    {
        AVStream *inStream, *outStream;
        if (pkt.stream_index == idx2)
        {
            if (dts_start_time2 == -1 && pkt.dts >= 0) dts_start_time2 = pkt.dts;
            if (pts_start_time2 == -1 && pkt.pts >= 0) pts_start_time2 = pkt.pts;
            pkt.stream_index = 1;
            inStream = pFmtCtx2->streams[idx2];
            if(av_q2d(inStream->time_base) * pkt.pts > endtime) {
                av_log(oFmtCtx, AV_LOG_INFO, "success!\n");
                break;
            }
            pkt.pts = pkt.pts - pts_start_time2;
            pkt.dts = pkt.dts - dts_start_time2;
 
            if (pkt.dts > pkt.pts)
            {
                pkt.pts = pkt.dts;
            }
 
            outStream = oFmtCtx->streams[pkt.stream_index];
            av_packet_rescale_ts(&pkt, inStream->time_base, outStream->time_base);
            pkt.pos = -1;
            av_interleaved_write_frame(oFmtCtx, &pkt);
            av_packet_unref(&pkt);
        }
    }
 
    //9. 写多媒体文件尾到文件中
    av_write_trailer(oFmtCtx);
 
    //11. 将申请的资源释放掉
_ERROR:
    if(pFmtCtx1){
        avformat_close_input(&pFmtCtx1);
        pFmtCtx1= NULL;
    }
    if(pFmtCtx2){
        avformat_close_input(&pFmtCtx2);
        pFmtCtx2= NULL;
    }
    if(oFmtCtx->pb){
        avio_close(oFmtCtx->pb);
    }
    if(oFmtCtx){
        avformat_free_context(oFmtCtx);
        oFmtCtx = NULL;
    }
    if(dts_start_time){
        av_free(dts_start_time);
    }
    if(pts_start_time){
        av_free(pts_start_time);
    }
    return 0;
}