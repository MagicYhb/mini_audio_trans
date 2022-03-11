#ifndef AUDIO_TRANS_H
#define AUDIO_TRANS_H

#include "opus/opus_types.h"

#define FEC_ENABLE 1
#define FEC_DISABLE 0

#define ENCODE_MODE 0
#define DECODE_MODE 1

struct audio_param_t
{
    int sampling_rate;
    int channels;
    int fps;
    int bit_depth;
};

int opus_trans_init(int mode, struct audio_param_t audio_param);
int opus_encode_frame(unsigned char *frame_data, int frame_len, unsigned char *out_buf, int out_len);
int opus_decode_frame(unsigned char *frame_data, int frame_len, opus_int16 *out_buf, int out_len);
void opus_trans_deinit();

int g711a_encode(char *pcm_data, int pcm_len, char *g711a_data, int g711a_len);
int g711a_decode(char *g711a_data, int g711a_len, char *pcm_buf, int pcm_len);

#endif