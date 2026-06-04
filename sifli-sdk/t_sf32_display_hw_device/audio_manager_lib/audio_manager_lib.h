#ifndef __AUDIO_MANAGER_LIB_H__
#define __AUDIO_MANAGER_LIB_H__

#include "rtthread.h"
#include <stdbool.h>
#include <drivers/audio.h>
#include <drv_audprc.h>
#include "audio_server.h"
#include "audio_mp3ctrl.h"

#define AUDCODEC_DEVICE_NAME "audcodec"
#define AUDPRC_DEVICE_NAME   "audprc"

#define AUDIO_BUF_SIZE  (640)
#define AUDRX_BUF_MAX (1024*1024)

#define SET_VOLUME_DEFAULT 8
#define VOLUME_MIN   0
#define VOLUME_MAX   15
#define VOLUME_STEP  1
#define DEFAULT_CHANNELS 1
#define DEFAULT_SAMPLERATE 44100
#define DEFAULT_BIT_DEPTH 16
#define MP3_PLAYLIST_MAX  64
#define MP3_FILENAME_MAX  128

#define RECORD_WAV_FILE  "record_test.wav"
#define RECORD_PCM_FILE  "record_test.pcm"
#define DEFAULT_MUSIC_DIR "/music"

/* WAV header (44 bytes for PCM) */
typedef struct __attribute__((packed))
{
    char     riff_id[4];      /* "RIFF" */
    uint32_t riff_size;       /* file size - 8 */
    char     wave_id[4];      /* "WAVE" */
    char     fmt_id[4];       /* "fmt " */
    uint32_t fmt_size;        /* 16 */
    uint16_t audio_format;    /* 1 = PCM */
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_id[4];      /* "data" */
    uint32_t data_size;
} wav_header_t;

typedef enum
{
    CMD_MP3_PALY = 0,  /* mp3 play */
    CMD_MP3_STOP,      /* mp3 stop */
    CMD_MP3_PAUSE,     /* mp3 pause */
    CMD_MP3_RESUME,    /* mp3 resume */
    CMD_MP3_NEXT,      /* play next in playlist */
    CMD_MP3_PREV,      /* play previous in playlist */
    CMD_MP3_MAX
} CMD_MP3_E;

typedef struct
{
    uint8_t cmd;  /* see enum CMD_MP3_E. */
    mp3_ioctl_cmd_param_t param;  /* mp3_ioctl_cmd_param_t */
    uint32_t loop;  /*loop times. 0 : play one time. 1 ~ n : play 2 ~ n+1 times. */
} mp3_ctrl_info_t;

typedef struct audio_manager_t
{
    rt_device_t audcodec_dev;
    rt_device_t audprc_dev;
    rt_event_t tx_wirte_event;
    rt_event_t rx_ind_event;
    rt_sem_t tx_complete_sem;
    rt_sem_t rx_complete_sem;
    rt_thread_t tx_thread;
    rt_thread_t rx_thread;
    rt_thread_t mp3_thread;
    rt_mq_t mp3_mq;
    mp3ctrl_handle mp3_handle;
    rt_uint32_t samplerate;
    rt_uint32_t bit_depth;
    rt_uint32_t channels;    
    rt_bool_t is_recording;     /* 非 0 时录音 */
    rt_bool_t is_playing;      /* 非 0 时播放 */
    rt_bool_t record_paused;  /* 非 0 时暂停录音 */
    rt_bool_t play_paused;  /* 非 0 时暂停录音 */
    int volume;
    char *file_path;
    int file_fd;
    rt_bool_t is_wav;       /* WAV format flag */
    char mp3_playlist[MP3_PLAYLIST_MAX][MP3_FILENAME_MAX];
    int mp3_playlist_count;
    int mp3_current_index;
    rt_bool_t mp3_paused;
    rt_bool_t mp3_shuffle;
    int current_db;          /* 最新麦克风 dBFS 值 */

}audio_manager_t;

rt_err_t audio_manager_init(void);
audio_manager_t *get_audio_info(void);
void audio_pa_open(void);
void audio_pa_close(void);

int audio_get_decibel(void);
void audio_db_monitor_stop(void);

void audio_record_start(const char *filepath);
void audio_record_pause(void);
void audio_record_resume(void);
void audio_record_stop(void);

void audio_play_start(const char *filepath);
void audio_play_pause(void);
void audio_play_resume(void);
void audio_play_stop(void);

void audio_set_volume(int vol);
int audio_get_volume(void);
void audio_volume_up(void);
void audio_volume_down(void);

void mp3_play_stop(void);
void mp3_play_pause(void);
void mp3_play_resume(void);
void mp3_file_play(const char *filepath);
void mp3_playlist_scan(const char *dir);
void mp3_play_next(void);
void mp3_play_prev(void);
void mp3_play_shuffle_toggle(void);
rt_bool_t mp3_play_shuffle_is_enabled(void);

#endif