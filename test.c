#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "audio_trans.h"

#define AUDIO_SAMPLERATE 16000
#define AUDIO_FPS 25
#define AUDIO_BIT_DEPTH 16
#define AUDIO_CHANNELS 1
#define DATA_MAX_LEN AUDIO_SAMPLERATE / AUDIO_FPS

#define SUPPORT_IMI 1

enum audio_t
{
    AUDIO_PCM,
    AUDIO_G711A,
    AUDIO_OPUS
};

#ifdef SUPPORT_IMI
static opus_uint32
char_to_int(unsigned char ch[4])
{
    return ((opus_uint32)ch[0] << 24) | ((opus_uint32)ch[1] << 16) | ((opus_uint32)ch[2] << 8) | (opus_uint32)ch[3];
}
static void int_to_char(opus_uint32 i, unsigned char ch[4])
{
    ch[0] = i >> 24;
    ch[1] = (i >> 16) & 0xFF;
    ch[2] = (i >> 8) & 0xFF;
    ch[3] = i & 0xFF;
}
#endif

static ssize_t get_file_content(char *file_name, uint8_t **out_buf)
{
    int fd;
    size_t file_size;
    ssize_t read_size;

    fd = open(file_name, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Cannot open %s!\n", file_name);
        return -1;
    }

    file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    *out_buf = (uint8_t *)malloc(sizeof(uint8_t) * file_size);

    read_size = read(fd, *out_buf, file_size);
    if (read_size != file_size)
    {
        fprintf(stderr, "Have not get all connect from %s!\n", file_name);
        return -1;
    }

    close(fd);
    return read_size;
}

static void destroy_file_content(uint8_t *buf)
{
    if (buf)
    {
        free(buf);
    }
}

static ssize_t write_to_file(char *file_name, int16_t *buf, ssize_t len)
{
    FILE *fp = NULL;
    size_t write_size;

    fp = fopen(file_name, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Cannot open %s!\n", file_name);
        return -1;
    }

    write_size = fwrite(buf, sizeof(int16_t), len, fp);

    fclose(fp);
    return write_size;
}

static void print_help()
{
    printf("usage: audio_trans [type] [filename] [type]\n");
    printf("type: pcm g711a opus\n");
}

static int get_audio_type(char *type_str)
{
    int type;

    if (!strncmp("pcm", type_str, 3))
    {
        type = AUDIO_PCM;
    }
    else if (!strncmp("g711a", type_str, 5))
    {
        type = AUDIO_G711A;
    }
    else if (!strncmp("opus", type_str, 4))
    {
        type = AUDIO_OPUS;
    }
    else
    {
        type = -1;
    }

    return type;
}

int main(int argc, char *argv[])
{
    int ret;
    int from_type, to_type;
    ssize_t read_size;
    uint8_t *read_buf = NULL;
    FILE *fp = NULL;
    struct audio_param_t audio_param;

    if (argc < 4)
    {
        print_help();
        exit(-1);
    }

    audio_param.bit_depth = AUDIO_BIT_DEPTH;
    audio_param.channels = AUDIO_CHANNELS;
    audio_param.fps = AUDIO_FPS;
    audio_param.sampling_rate = AUDIO_SAMPLERATE;

    from_type = get_audio_type(argv[1]);
    to_type = get_audio_type(argv[3]);
    if (from_type == -1 || to_type == -1)
    {
        print_help();
        exit(-1);
    }

    printf("start read %s\n", argv[2]);
    read_size = get_file_content(argv[2], &read_buf);
    if (read_size < 0)
    {
        return -1;
    }
    printf("read %ldBit data\n", read_size);

    switch (from_type)
    {
    case AUDIO_PCM:
    {
        switch (to_type)
        {
        case AUDIO_PCM:
        {
            printf("Need not translete!\n");
            break;
        }
        case AUDIO_G711A:
        {
            int g711a_len = (read_size + 1) / 2;
            char *g711a_buf = (char *)malloc(g711a_len);
            g711a_encode(read_buf, read_size, g711a_buf, g711a_len);

            fp = fopen("out.g711a", "w");
            if (fp == NULL)
            {
                fprintf(stderr, "out.g711a failed!!!\n");
                return -1;
            }
            fwrite(g711a_buf, sizeof(char), g711a_len, fp);
            fclose(fp);
            break;
        }
        case AUDIO_OPUS:
        {
            fp = fopen("out.opus", "w");
            if (fp == NULL)
            {
                fprintf(stderr, "Open out.opus failed!!!\n");
                return -1;
            }

            ret = opus_trans_init(ENCODE_MODE, audio_param);
            if (ret != 0)
            {
                fprintf(stderr, "opus_trans_init failed!!!\n");
                return -1;
            }

            uint8_t *opus_buf = (uint8_t *)malloc(sizeof(uint8_t) * DATA_MAX_LEN);
            int opus_buf_len;
            int decode_offset = 0;
            while (1)
            {
                opus_buf_len = opus_encode_frame(read_buf + decode_offset,
                                                 AUDIO_SAMPLERATE / AUDIO_FPS * sizeof(opus_int16),
                                                 opus_buf, DATA_MAX_LEN);
#ifdef SUPPORT_IMI
                unsigned char ch[4];
                int_to_char(opus_buf_len, ch);
                fwrite(ch, sizeof(unsigned char), 4, fp);
                memset(ch, 0, 4);
                fwrite(ch, sizeof(unsigned char), 4, fp);
#else
                fwrite(&opus_buf_len, sizeof(int), 1, fp);
#endif
                fwrite(opus_buf, sizeof(uint8_t), opus_buf_len, fp);
                decode_offset += AUDIO_SAMPLERATE / AUDIO_FPS * sizeof(opus_int16);

                if (decode_offset >= read_size)
                {
                    break;
                }
            }

            fclose(fp);
            free(opus_buf);
            opus_trans_deinit();
            break;
        }

        default:
            break;
        }
        break;
    }

    case AUDIO_G711A:
    {
        int pcm_len = read_size * 2;
        char *pcm_buf = (char *)malloc(pcm_len);
        g711a_decode(read_buf, read_size, pcm_buf, pcm_len);

        if (to_type == AUDIO_PCM)
        {
            fp = fopen("out.pcm", "w");
            if (fp == NULL)
            {
                fprintf(stderr, "Open out.pcm failed!!!\n");
                return -1;
            }
            fwrite(pcm_buf, sizeof(char), pcm_len, fp);
            free(pcm_buf);
            fclose(fp);
        }
        else if (to_type == AUDIO_G711A)
        {
            printf("Need not translete!\n");
        }
        else if (to_type == AUDIO_OPUS)
        {
            fp = fopen("out.opus", "w");
            if (fp == NULL)
            {
                fprintf(stderr, "Open out.opus failed!!!\n");
                return -1;
            }

            ret = opus_trans_init(ENCODE_MODE, audio_param);
            if (ret != 0)
            {
                fprintf(stderr, "opus_trans_init failed!!!\n");
                return -1;
            }

            uint8_t *opus_buf = (uint8_t *)malloc(sizeof(uint8_t) * DATA_MAX_LEN);
            int opus_buf_len;
            int encode_offset = 0;
            while (1)
            {
                opus_buf_len = opus_encode_frame(pcm_buf + encode_offset,
                                                 AUDIO_SAMPLERATE / AUDIO_FPS * sizeof(opus_int16),
                                                 opus_buf, DATA_MAX_LEN);
#ifdef SUPPORT_IMI
                unsigned char ch[4];
                int_to_char(opus_buf_len, ch);
                fwrite(ch, sizeof(unsigned char), 4, fp);
                memset(ch, 0, 4);
                fwrite(ch, sizeof(unsigned char), 4, fp);
#else
                fwrite(&opus_buf_len, sizeof(int), 1, fp);
#endif
                fwrite(opus_buf, sizeof(uint8_t), opus_buf_len, fp);
                encode_offset += AUDIO_SAMPLERATE / AUDIO_FPS * sizeof(opus_int16);

                if (encode_offset >= pcm_len)
                {
                    break;
                }
            }

            fclose(fp);
            free(pcm_buf);
            free(opus_buf);
            opus_trans_deinit();
        }
        break;
    }
    case AUDIO_OPUS:
    {
        if(to_type == AUDIO_OPUS)
        {
            printf("Need not translete!\n");
            break;
        }

        int g711a_len;
        char *g711a_buf = NULL;

        if (to_type == AUDIO_PCM)
        {
            fp = fopen("out.pcm", "w");
            if (fp == NULL)
            {
                fprintf(stderr, "Open out.pcm failed!!!\n");
                return -1;
            }
        }
        else if (to_type == AUDIO_G711A)
        {
            fp = fopen("out.g711a", "w");
            if (fp == NULL)
            {
                fprintf(stderr, "out.g711a failed!!!\n");
                return -1;
            }
            g711a_len = DATA_MAX_LEN;
            g711a_buf = (char *)malloc(g711a_len);
        }

        ret = opus_trans_init(DECODE_MODE, audio_param);
        if (ret != 0)
        {
            fprintf(stderr, "opus_trans_init failed!!!\n");
            return -1;
        }

        opus_int16 *pcm_buf = (opus_int16 *)malloc(sizeof(opus_int16) * DATA_MAX_LEN);
        int pcm_buf_len;
        int decode_offset = 0;
        int frame_len;
        while (1)
        {
#ifdef SUPPORT_IMI
            unsigned char ch[4];
            memcpy(ch, read_buf + decode_offset, 4);
            frame_len = char_to_int(ch);
            decode_offset += 8;
#else
            memcpy(&frame_len, read_buf + decode_offset, sizeof(int));
            decode_offset += sizeof(int);
#endif
            printf("frame_len is %d\n", frame_len);
            pcm_buf_len = opus_decode_frame(read_buf + decode_offset, frame_len, pcm_buf, DATA_MAX_LEN);
            if (to_type == AUDIO_PCM)
            {
                fwrite(pcm_buf, sizeof(opus_int16), pcm_buf_len, fp);
            }
            else if (to_type == AUDIO_G711A)
            {
                printf("pcm_len:%d g711a_len:%d\n", pcm_buf_len, g711a_len);
                g711a_encode((char *)pcm_buf, pcm_buf_len * sizeof(opus_int16), g711a_buf, g711a_len);
                fwrite(g711a_buf, sizeof(char), pcm_buf_len, fp);
            }
            decode_offset += frame_len;

            if (decode_offset >= read_size)
            {
                break;
            }
        }

        fclose(fp);
        free(pcm_buf);
        free(g711a_buf);
        opus_trans_deinit();
        break;
    }
    default:
        break;
    }

    printf("finish code!\n");
    destroy_file_content(read_buf);

    return 0;
}

#if 0
int main(int argc, char *argv[])
{
    ssize_t read_size;
    uint8_t *read_buf = NULL;
    char *g711a_buf = NULL;
    char *pcm_buf = NULL;
    FILE *fp = NULL;

    printf("start read %s\n", argv[2]);
    read_size = get_file_content(argv[2], &read_buf);
    printf("read %ldBit data\n", read_size);

    if (argv[1][0] == 'd')
    {
        int pcm_len = read_size * 2;
        pcm_buf = (char *)malloc(pcm_len);
        g711a_decode(read_buf, read_size, pcm_buf, pcm_len);

        fp = fopen("out.pcm", "w");
        fwrite(pcm_buf, sizeof(char), pcm_len, fp);
        fclose(fp);
    }
    else if (argv[1][0] == 'e')
    {
        int g711a_len = (read_size + 1) / 2;
        g711a_buf = (char *)malloc(g711a_len);
        g711a_encode(read_buf, read_size, g711a_buf, g711a_len);

        fp = fopen("out.g711a", "w");
        fwrite(g711a_buf, sizeof(char), g711a_len, fp);
        fclose(fp);
    }
    else
    {
        print_help();
    }
    destroy_file_content(read_buf);
    return 0;
}

int main(int argc, char *argv[])
{
    int ret;
    ssize_t read_size;
    uint8_t *read_buf = NULL;
    uint8_t *opus_buf = NULL;
    int opus_buf_len;
    ssize_t decode_offset = 0;
    struct audio_param_t audio_param;
    opus_int16 *pcm_buf = NULL;
    int pcm_buf_len;
    int running = 1;
    int frame_len = 0;
    FILE *fp = NULL;

    if (argc < 3)
    {
        print_help();
        exit(-1);
    }

    printf("start read %s\n", argv[2]);
    read_size = get_file_content(argv[2], &read_buf);
    printf("read %ldBit data\n", read_size);

    audio_param.bit_depth = AUDIO_BIT_DEPTH;
    audio_param.channels = AUDIO_CHANNELS;
    audio_param.fps = AUDIO_FPS;
    audio_param.sampling_rate = AUDIO_SAMPLERATE;

    if (argv[1][0] == 'd')
    {
        fp = fopen("out.pcm", "w");
        if (fp == NULL)
        {
            fprintf(stderr, "Open out.pcm failed!!!\n");
            return -1;
        }

        ret = opus_trans_init(DECODE_MODE, audio_param);
        if (ret != 0)
        {
            fprintf(stderr, "opus_trans_init failed!!!\n");
            return -1;
        }

        pcm_buf = (opus_int16 *)malloc(sizeof(opus_int16) * DATA_MAX_LEN);
        while (running)
        {
#ifdef SUPPORT_IMI
            unsigned char ch[4];
            memcpy(ch, read_buf + decode_offset, 4);
            frame_len = char_to_int(ch);
            decode_offset += 8;
#else
            memcpy(&frame_len, read_buf + decode_offset, sizeof(int));
            decode_offset += sizeof(int);
#endif
            printf("frame_len is %d\n", frame_len);
            pcm_buf_len = opus_decode_frame(read_buf + decode_offset, frame_len, pcm_buf, DATA_MAX_LEN);
            fwrite(pcm_buf, sizeof(opus_int16), pcm_buf_len, fp);
            decode_offset += frame_len;

            if (decode_offset >= read_size)
            {
                break;
            }
        }

        fclose(fp);
        free(pcm_buf);
    }
    else if (argv[1][0] == 'e')
    {
        fp = fopen("out.opus", "w");
        if (fp == NULL)
        {
            fprintf(stderr, "Open out.opus failed!!!\n");
            return -1;
        }

        ret = opus_trans_init(ENCODE_MODE, audio_param);
        if (ret != 0)
        {
            fprintf(stderr, "opus_trans_init failed!!!\n");
            return -1;
        }

        opus_buf = (uint8_t *)malloc(sizeof(uint8_t) * DATA_MAX_LEN);
        while (running)
        {
            opus_buf_len = opus_encode_frame(read_buf + decode_offset,
                                             AUDIO_SAMPLERATE / AUDIO_FPS * sizeof(opus_int16),
                                             opus_buf, DATA_MAX_LEN);
#ifdef SUPPORT_IMI
            unsigned char ch[4];
            int_to_char(opus_buf_len, ch);
            fwrite(ch, sizeof(unsigned char), 4, fp);
            memset(ch, 0, 4);
            fwrite(ch, sizeof(unsigned char), 4, fp);
#else
            fwrite(&opus_buf_len, sizeof(int), 1, fp);
#endif
            fwrite(opus_buf, sizeof(uint8_t), opus_buf_len, fp);
            decode_offset += AUDIO_SAMPLERATE / AUDIO_FPS * sizeof(opus_int16);

            if (decode_offset >= read_size)
            {
                break;
            }
        }

        fclose(fp);
        free(opus_buf);
    }
    else
    {
        print_help();
    }

    printf("finish code!\n");

    opus_trans_deinit();
    destroy_file_content(read_buf);
    return 0;
}
#endif