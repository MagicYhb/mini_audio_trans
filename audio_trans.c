#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_trans.h"
#include "opus/opus.h"

static int frame_size;
static OpusEncoder *encoder = NULL;
static OpusDecoder *decoder = NULL;

static short aLawDecompressTable[] = {-5504, -5248,
                                      -6016, -5760, -4480, -4224, -4992, -4736, -7552, -7296, -8064,
                                      -7808, -6528, -6272, -7040, -6784, -2752, -2624, -3008, -2880,
                                      -2240, -2112, -2496, -2368, -3776, -3648, -4032, -3904, -3264,
                                      -3136, -3520, -3392, -22016, -20992, -24064, -23040, -17920,
                                      -16896, -19968, -18944, -30208, -29184, -32256, -31232, -26112,
                                      -25088, -28160, -27136, -11008, -10496, -12032, -11520, -8960,
                                      -8448, -9984, -9472, -15104, -14592, -16128, -15616, -13056,
                                      -12544, -14080, -13568, -344, -328, -376, -360, -280, -264, -312,
                                      -296, -472, -456, -504, -488, -408, -392, -440, -424, -88, -72,
                                      -120, -104, -24, -8, -56, -40, -216, -200, -248, -232, -152, -136,
                                      -184, -168, -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
                                      -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696, -688, -656,
                                      -752, -720, -560, -528, -624, -592, -944, -912, -1008, -976, -816,
                                      -784, -880, -848, 5504, 5248, 6016, 5760, 4480, 4224, 4992, 4736,
                                      7552, 7296, 8064, 7808, 6528, 6272, 7040, 6784, 2752, 2624, 3008,
                                      2880, 2240, 2112, 2496, 2368, 3776, 3648, 4032, 3904, 3264, 3136,
                                      3520, 3392, 22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
                                      30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136, 11008,
                                      10496, 12032, 11520, 8960, 8448, 9984, 9472, 15104, 14592, 16128,
                                      15616, 13056, 12544, 14080, 13568, 344, 328, 376, 360, 280, 264,
                                      312, 296, 472, 456, 504, 488, 408, 392, 440, 424, 88, 72, 120, 104,
                                      24, 8, 56, 40, 216, 200, 248, 232, 152, 136, 184, 168, 1376, 1312,
                                      1504, 1440, 1120, 1056, 1248, 1184, 1888, 1824, 2016, 1952, 1632,
                                      1568, 1760, 1696, 688, 656, 752, 720, 560, 528, 624, 592, 944, 912,
                                      1008, 976, 816, 784, 880, 848};
static int cClip = 32635;
static char aLawCompressTable[] = {1, 1, 2, 2, 3, 3, 3,
                                   3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                   5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                   6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                                   7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static int opus_audio_encoder_init(opus_int32 sampling_rate, int channels, int bit_depth)
{
    int err;
    unsigned int variable_duration = 0;

    if (frame_size == sampling_rate / 400)
        variable_duration = OPUS_FRAMESIZE_2_5_MS;
    else if (frame_size == sampling_rate / 200)
        variable_duration = OPUS_FRAMESIZE_5_MS;
    else if (frame_size == sampling_rate / 100)
        variable_duration = OPUS_FRAMESIZE_10_MS;
    else if (frame_size == sampling_rate / 50)
        variable_duration = OPUS_FRAMESIZE_20_MS;
    else if (frame_size == sampling_rate / 25)
        variable_duration = OPUS_FRAMESIZE_40_MS;
    else if (frame_size == 3 * sampling_rate / 50)
        variable_duration = OPUS_FRAMESIZE_60_MS;
    else if (frame_size == 4 * sampling_rate / 50)
        variable_duration = OPUS_FRAMESIZE_80_MS;
    else if (frame_size == 5 * sampling_rate / 50)
        variable_duration = OPUS_FRAMESIZE_100_MS;
    else
        variable_duration = OPUS_FRAMESIZE_120_MS;

    /*
     * OPUS_APPLICATION_VOIP 视频会议
     * OPUS_APPLICATION_AUDIO 高保真
     * OPUS_APPLICATION_RESTRICTED_LOWDELAY 低延迟，但是效果差
     */
    encoder = opus_encoder_create(sampling_rate, channels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK)
    {
        fprintf(stderr, "Cannot create encoder: %s\n", opus_strerror(err));
        return -1;
    }

    /*
     * OPUS_SIGNAL_VOICE 语音
     * OPUS_SIGNAL_MUSIC 音乐
     */
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_AUTO)); // 控制最大比特率，AUTO在不说话时减少带宽。
    opus_encoder_ctl(encoder, OPUS_SET_BANDWIDTH(OPUS_AUTO));
    opus_encoder_ctl(encoder, OPUS_SET_VBR(1));                                   // 0固定码率，1动态码率
    opus_encoder_ctl(encoder, OPUS_SET_VBR_CONSTRAINT(1));                        // 0不受约束，1受约束（默认）
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(0));                            // 编码复杂度0~10
    opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(FEC_DISABLE));                  // 算法修复丢失的数据包，0关，1开
    opus_encoder_ctl(encoder, OPUS_SET_FORCE_CHANNELS(channels));                 // 声道数
    opus_encoder_ctl(encoder, OPUS_SET_DTX(0));                                   // 不连续传输，0关，1开
    opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH(bit_depth));                     // 位深
    opus_encoder_ctl(encoder, OPUS_SET_EXPERT_FRAME_DURATION(variable_duration)); // 帧持续时间
    opus_encoder_ctl(encoder, OPUS_SET_APPLICATION(OPUS_APPLICATION_VOIP));       //同编码器创建参数

    return err;
}

static int opus_audio_decoder_init(opus_int32 sampling_rate, int channels)
{
    int err;

    decoder = opus_decoder_create(sampling_rate, channels, &err);
    if (err != OPUS_OK)
    {
        fprintf(stderr, "Cannot create decoder: %s\n", opus_strerror(err));
        return -1;
    }

    return err;
}

int opus_trans_init(int mode, struct audio_param_t audio_param)
{
    int ret;
    frame_size = audio_param.sampling_rate / audio_param.fps;

    switch (mode)
    {
    case ENCODE_MODE:
        ret = opus_audio_encoder_init(audio_param.sampling_rate, audio_param.channels, audio_param.bit_depth);
        break;
    case DECODE_MODE:
        ret = opus_audio_decoder_init(audio_param.sampling_rate, audio_param.channels);
        break;

    default:
        break;
    }

    return ret;
}

int opus_encode_frame(unsigned char *frame_data, int frame_len, unsigned char *out_buf, int out_len)
{
    opus_int32 opus_data_len;

    if ((encoder == NULL) || frame_data == NULL || frame_len == 0)
        return 0;

    opus_data_len = opus_encode(encoder, (opus_int16 *)frame_data, frame_len, out_buf, out_len);
    if (opus_data_len < 0)
    {
        fprintf(stderr, "opus_encode() returned %d\n", opus_data_len);
        return -1;
    }

    printf("pcm_len:%d, opus_len:%d\n", frame_len, opus_data_len);

    return opus_data_len;
}

int opus_decode_frame(unsigned char *frame_data, int frame_len, opus_int16 *out_buf, int out_len)
{
    opus_int32 pcm_data_len;

    if ((decoder == NULL) || frame_data == NULL || frame_len == 0)
        return 0;

    pcm_data_len = opus_decode(decoder, frame_data, frame_len, out_buf, out_len, FEC_DISABLE);

    return pcm_data_len;
}

void opus_trans_deinit()
{
    if (encoder)
    {
        opus_encoder_destroy(encoder);
    }
    if (decoder)
    {
        opus_decoder_destroy(decoder);
    }
}

static char g711a_linearToALawSample(short sample)
{
    int sign;
    int exponent;
    int mantissa;
    int s;
    sign = ((~sample) >> 8) & 0x80;
    if (!(sign == 0x80))
    {
        sample = (short)-sample;
    }
    if (sample > cClip)
    {
        sample = cClip;
    }
    if (sample >= 256)
    {
        exponent = (int)aLawCompressTable[(sample >> 8) & 0x7F];
        mantissa = (sample >> (exponent + 3)) & 0x0F;
        s = (exponent << 4) | mantissa;
    }
    else
    {
        s = sample >> 4;
    }
    s ^= (sign ^ 0x55);
    return (char)s;
}

int g711a_encode(char *pcm_data, int pcm_len, char *g711a_data, int g711a_len)
{
    int j = 0;
    int count = pcm_len / 2;
    short sample = 0;
    int i = 0;

    if (g711a_len * 2 < pcm_len)
    {
        fprintf(stderr, "The g711a_data do not have enough space!");
        return -1;
    }

    for (i = 0; i < count; i++)
    {
        sample = (short)(((pcm_data[j++] & 0xff) | (pcm_data[j++]) << 8));
        g711a_data[i] = g711a_linearToALawSample(sample);
    }
    return count;
}

int g711a_decode(char *g711a_data, int g711a_len, char *pcm_buf, int pcm_len)
{
    int j = 0;
    int tmp_offset = 0;
    int i = 0;

    if (g711a_len * 2 > pcm_len)
    {
        fprintf(stderr, "The pcm_buf do not have enough space!");
        return -1;
    }

    for (i = 0; i < g711a_len; i++)
    {
        short s = aLawDecompressTable[g711a_data[i + tmp_offset] & 0xff];
        pcm_buf[j++] = (char)s;
        pcm_buf[j++] = (char)(s >> 8);
    }
    return j;
}