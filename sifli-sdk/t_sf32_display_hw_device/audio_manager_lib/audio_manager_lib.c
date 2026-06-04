#include "audio_manager_lib.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "mem_section.h"
#include "ns4150b.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "math.h"
#if RT_USING_DFS
    #include "dfs_file.h"
    #include "dfs_posix.h"
    #include <dirent.h>
#endif

L2_RET_BSS_SECT_BEGIN(g_audrx_buf)
ALIGN(4) uint8_t g_audrx_buf[AUDRX_BUF_MAX] L2_RET_BSS_SECT(g_audrx_buf);
L2_RET_BSS_SECT_END
static uint8_t mic_data[AUDIO_BUF_SIZE];
static uint8_t sd_play_data[AUDIO_BUF_SIZE / 2];
uint32_t g_tmp_pos = 0;

audio_manager_t audio;

#define AUDIO_LOG_DEBUG 1
#ifdef AUDIO_LOG_DEBUG
    #define AUDIO_LOG_DEBUG(...) rt_kprintf("[AUDIO_MANAGER_LIB]" __VA_ARGS__)
#else
    #define AUDIO_LOG_DEBUG(...)
#endif

/* Funtion */
void mp3_proc_thread_entry(void *params);


static bool has_wav_ext(const char *path)
{
    if (!path)
        return false;
    const char *ext = strrchr(path, '.');
    return ext && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".WAV") == 0);
}

static void wav_header_init(wav_header_t *hdr, uint32_t rate, uint32_t ch,
                            uint32_t bits)
{
    memset(hdr, 0, sizeof(*hdr));
    memcpy(hdr->riff_id, "RIFF", 4);
    memcpy(hdr->wave_id, "WAVE", 4);
    memcpy(hdr->fmt_id, "fmt ", 4);
    memcpy(hdr->data_id, "data", 4);
    hdr->fmt_size = 16;
    hdr->audio_format = 1; /* PCM */
    hdr->num_channels = ch;
    hdr->sample_rate = rate;
    hdr->bits_per_sample = bits;
    hdr->block_align = ch * (bits / 8);
    hdr->byte_rate = rate * hdr->block_align;
}

/**
 * @brief  Audio receiving callback.
 * This callback will send event to receiving thread for further audio
 * precessing.
 * @param[in]  dev: audio device.
 * @param[in]  size: received audio data size.
 * @retval RT_EOK
 */
static rt_err_t audio_rx_ind(rt_device_t dev, rt_size_t size)
{
    rt_event_send(audio.rx_ind_event, 1);
    return RT_EOK;
}

/**
 * @brief  Tx callback.
 * When Isr arrived, need to write audio data (half dma buffer) to tx device.
 * @param[in]  dev: audio device.
 * @retval RT_EOK
 */
static rt_err_t audio_tx_done(rt_device_t dev, void *buffer)
{
    rt_event_send(audio.tx_wirte_event, 1);
    return RT_EOK;
}

/**
 * @brief send msg to mp3 proc thread.
 */
static void send_msg_to_mp3_proc(mp3_ctrl_info_t *info)
{
    rt_err_t err = rt_mq_send(audio.mp3_mq, info, sizeof(mp3_ctrl_info_t));
    RT_ASSERT(err == RT_EOK);
}

void audio_pa_open(void)
{
    sifli_ns4150b_start();
}

void audio_pa_close(void)
{
    sifli_ns4150b_stop();
}

static void config_rx(void)
{
    struct rt_audio_caps caps;
    int stream;

    /* AUDCODEC : set input as "AUDPRC_RX_FROM_CODEC" */
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_SETINPUT,
                      (void *)AUDPRC_RX_FROM_CODEC);
    caps.main_type = AUDIO_TYPE_INPUT;
    caps.sub_type = 1 << HAL_AUDCODEC_ADC_CH0;       // ADC CH0
    caps.udata.config.channels = audio.channels;     // channels
    caps.udata.config.samplerate = audio.samplerate; // sample rate
    caps.udata.config.samplefmt = audio.bit_depth;   // depth. 8 16 24 or 32
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_CONFIGURE, &caps);

    AUDIO_LOG_DEBUG(
        "codec input parameter:sub_type=%d channels %d, rate %d, bits %d",
        caps.sub_type, caps.udata.config.channels, caps.udata.config.samplerate,
        caps.udata.config.samplefmt);

    /* AUDPRC : set input as "AUDPRC_RX_FROM_CODEC" */
    rt_device_control(audio.audprc_dev, AUDIO_CTL_SETINPUT,
                      (void *)AUDPRC_RX_FROM_CODEC);
    caps.main_type = AUDIO_TYPE_INPUT;
    caps.sub_type = HAL_AUDPRC_RX_CH0 - HAL_AUDPRC_RX_CH0;
    caps.udata.config.channels = audio.channels;
    caps.udata.config.samplerate = audio.samplerate;
    caps.udata.config.samplefmt = audio.bit_depth;
    AUDIO_LOG_DEBUG(
        "mic input:rx channel %d, channels %d, rate %d, bitwidth %d", 0,
        caps.udata.config.channels, caps.udata.config.samplerate,
        caps.udata.config.samplefmt);
    rt_device_control(audio.audprc_dev, AUDIO_CTL_CONFIGURE, &caps);
}

static void config_tx(void)
{
#define mixer_sel 0x5150
    struct rt_audio_caps caps;
    struct rt_audio_sr_convert cfg;
    // uint8_t g_hardware_mix_enable = 1;
    /* mix left & right channel to mono channel output, too big volume sometime
     */
    int out_sel = 0x5050;
    // if (caps.udata.config.channels == 2 && g_hardware_mix_enable)
    // {
    //     out_sel = 0x5010; //mix left & right to speaker.  speaker pcm = left
    //     pcm + right pcm
    // }

    /* AUCODEC : set output as "AUDPRC_TX_TO_CODEC" (codec/mem/i2s). */
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_SETOUTPUT,
                      (void *)AUDPRC_TX_TO_CODEC);
    caps.main_type = AUDIO_TYPE_OUTPUT;
    caps.sub_type = 1 << HAL_AUDCODEC_DAC_CH0;
#if defined(BSP_USING_BOARD_SF32LB58_LCD_N16R64N4)
    caps.sub_type |= (1 << HAL_AUDCODEC_DAC_CH1);
#endif
    caps.udata.config.channels = audio.channels; // L,R,L,R,L,R, ......
    caps.udata.config.samplerate = audio.samplerate;
    caps.udata.config.samplefmt = audio.bit_depth; // 8 16 24 or 32
    AUDIO_LOG_DEBUG(
        "prc_codec : sub_type=%d channel %d, samplerate %d, bits %d\n",
        caps.sub_type, caps.udata.config.channels, caps.udata.config.samplerate,
        caps.udata.config.samplefmt);
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_CONFIGURE, &caps);

    /* AUDPRC : set output as "AUDPRC_TX_TO_CODEC" (codec/mem/i2s). */
    rt_device_control(audio.audprc_dev, AUDIO_CTL_SETOUTPUT,
                      (void *)AUDPRC_TX_TO_CODEC);
    cfg.channel = audio.channels;
    cfg.source_sr = audio.samplerate;
    cfg.dest_sr = audio.samplerate;
    AUDIO_LOG_DEBUG("speaker OUTPUTSRC channel=%d in_rate=%d out_rate=%d\n",
                    cfg.channel, cfg.source_sr, cfg.dest_sr);
    rt_device_control(audio.audprc_dev, AUDIO_CTL_OUTPUTSRC, (void *)(&cfg));

    /* AUDPRC : SELECTOR */
    AUDIO_LOG_DEBUG("speaker select=0x%x mixer=0x%x\n", out_sel, mixer_sel);
    caps.main_type = AUDIO_TYPE_SELECTOR;
    caps.sub_type = 0xFF;
    caps.udata.value = out_sel;
    rt_device_control(audio.audprc_dev, AUDIO_CTL_CONFIGURE, &caps);

    /* AUDPRC : MIXER */
    caps.main_type = AUDIO_TYPE_MIXER;
    caps.sub_type = 0xFF;
    caps.udata.value = out_sel;
    rt_device_control(audio.audprc_dev, AUDIO_CTL_CONFIGURE, &caps);

    /* data source format */
    caps.main_type = AUDIO_TYPE_OUTPUT;
    caps.sub_type = HAL_AUDPRC_TX_CH0;
    caps.udata.config.channels = audio.channels;
    caps.udata.config.samplerate = audio.samplerate;
    caps.udata.config.samplefmt = audio.bit_depth;
    AUDIO_LOG_DEBUG("tx[0]: sub_type %d, ch %d, samrate %d, bits %d\n",
                    caps.sub_type, caps.udata.config.channels,
                    caps.udata.config.samplerate, caps.udata.config.samplefmt);

    rt_device_control(audio.audprc_dev, AUDIO_CTL_CONFIGURE, &caps);

    /* Set volume */
    AUDIO_LOG_DEBUG("init volume=%d\n", audio.volume);
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_SETVOLUME,
                      (void *)audio.volume);
}

static void start_rx(void)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    int stream;
    stream = AUDIO_STREAM_RECORD | ((1 << HAL_AUDCODEC_ADC_CH0) << 8);
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_START, &stream);
    stream = AUDIO_STREAM_RECORD | ((1 << HAL_AUDPRC_RX_CH0) << 8);
    rt_device_set_rx_indicate(audio.audprc_dev, audio_rx_ind);
    rt_device_control(audio.audprc_dev, AUDIO_CTL_START, &stream);
}

static void stop_rx(void)
{
    int stream;
    AUDIO_LOG_DEBUG("%s\n", __func__);
    stream = AUDIO_STREAM_RECORD | ((1 << HAL_AUDCODEC_ADC_CH0) << 8);
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_STOP, &stream);
    stream = AUDIO_STREAM_RECORD | ((1 << HAL_AUDPRC_RX_CH0) << 8);
    rt_device_set_rx_indicate(audio.audprc_dev, audio_rx_ind);
    rt_device_control(audio.audprc_dev, AUDIO_CTL_STOP, &stream);
}

static void start_tx(void)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    int stream_audprc, stream_audcodec;
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_MUTE, (void *)1);
    stream_audcodec = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDCODEC_DAC_CH0) << 8);
    stream_audprc = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDPRC_TX_CH0) << 8);

    rt_device_control(audio.audcodec_dev, AUDIO_CTL_START, &stream_audcodec);
    rt_device_set_tx_complete(audio.audprc_dev, audio_tx_done);
    rt_device_control(audio.audprc_dev, AUDIO_CTL_START, &stream_audprc);

    audio_pa_open();

    rt_thread_mdelay(30);
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_MUTE, (void *)0);
}

static void stop_tx(void)
{
    int stream_audprc, stream_audcodec;
    AUDIO_LOG_DEBUG("%s\n", __func__);

    stream_audcodec = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDCODEC_DAC_CH0) << 8);
    stream_audprc = AUDIO_STREAM_REPLAY | ((1 << HAL_AUDPRC_TX_CH0) << 8);
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_STOP, &stream_audcodec);
    rt_device_control(audio.audprc_dev, AUDIO_CTL_STOP, &stream_audprc);

    audio_pa_close();
}

rt_err_t audio_manager_init(void)
{
    int err = RT_EOK;
    audio.audprc_dev = rt_device_find(AUDPRC_DEVICE_NAME);
    if (NULL == audio.audprc_dev)
    {
        AUDIO_LOG_DEBUG("Find audprc device failed.\n");
        return -RT_ERROR;
    }
    err = rt_device_open(audio.audprc_dev, RT_DEVICE_FLAG_RDWR);
    if (RT_EOK != err)
    {
        AUDIO_LOG_DEBUG("Open audprc device failed. err=%d\n", err);
        return -RT_ERROR;
    }

    audio.audcodec_dev = rt_device_find(AUDCODEC_DEVICE_NAME);
    if (NULL == audio.audcodec_dev)
    {
        AUDIO_LOG_DEBUG("Find audcodec device failed.\n");
        return -RT_ERROR;
    }
    err = rt_device_open(audio.audcodec_dev, RT_DEVICE_FLAG_WRONLY);
    if (RT_EOK != err)
    {
        AUDIO_LOG_DEBUG("Open audcodec device failed. err=%d\n", err);
        return -RT_ERROR;
    }

    audio.volume = SET_VOLUME_DEFAULT;
    audio.samplerate = DEFAULT_SAMPLERATE;
    audio.channels = DEFAULT_CHANNELS;
    audio.bit_depth = DEFAULT_BIT_DEPTH;
    audio.is_recording = false;
    audio.is_playing = false;
    audio.play_paused = false;
    audio.record_paused = false;
    audio.is_wav = false;
    audio.mp3_handle = NULL;
    audio.mp3_playlist_count = 0;
    audio.mp3_current_index = -1;
    audio.mp3_paused = false;
    audio.mp3_shuffle = false;
    audio.current_db = -100;

    // create sem and threads.
    audio.tx_wirte_event = rt_event_create("tx_wirte_event", RT_IPC_FLAG_FIFO);
    audio.rx_ind_event = rt_event_create("rx_ind_event", RT_IPC_FLAG_FIFO);
    audio.mp3_mq =
        rt_mq_create("mp3_mq", sizeof(mp3_ctrl_info_t), 60, RT_IPC_FLAG_FIFO);

    srand(rt_tick_get());

    return RT_EOK;
}

audio_manager_t *get_audio_info(void)
{
    return &audio;
}

/* 音量设置 */
void audio_set_volume(int vol)
{
    if (vol < VOLUME_MIN)
        vol = VOLUME_MIN;
    if (vol > VOLUME_MAX)
        vol = VOLUME_MAX;
    audio.volume = vol;
    // rt_device_control(audio.audcodec_dev, AUDIO_CTL_SETVOLUME, (void *)audio.volume);
    audio_server_set_private_volume(AUDIO_TYPE_BT_MUSIC, (uint8_t)audio.volume);
    audio_server_set_private_volume(AUDIO_TYPE_LOCAL_MUSIC, (uint8_t)audio.volume);
    // audio_server_set_private_volume(AUDIO_TYPE_LOCAL_RECORD, (uint8_t)audio.volume);
    audio_server_set_public_volume((uint8_t)audio.volume);
    AUDIO_LOG_DEBUG("Volume set to %d\n", audio.volume);
}

int audio_get_volume(void)
{
    return audio.volume;
}

void audio_volume_up(void)
{
    int vol = audio.volume + VOLUME_STEP;
    if (vol > VOLUME_MAX)
        vol = VOLUME_MAX;
    audio_set_volume(vol);
}

void audio_volume_down(void)
{
    int vol = audio.volume - VOLUME_STEP;
    if (vol < VOLUME_MIN)
        vol = VOLUME_MIN;
    audio_set_volume(vol);
}

static rt_bool_t db_monitor_running = false;

/* 获取麦克风分贝值 (dBFS, 回传值 = 负数, 0=最大, -100=静音) */
int audio_get_decibel(void)
{
    int16_t buf[160];
    int len, samples;
    int64_t sum_sq = 0;
    double rms;
    int db;
    rt_uint32_t evt;

    if (!audio.audprc_dev || !audio.audcodec_dev)
        return -100;

    /* 录音中直接返回缓存的最新值 */
    if (audio.is_recording)
        return audio.current_db;

    /* 首次调用：启动麦克风并保持运行 */
    if (!db_monitor_running)
    {
        config_rx();
        start_rx();

        /* 丢弃第一帧（启动时的脏数据） */
        rt_event_recv(audio.rx_ind_event, 1,
                      RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 0, &evt);
        if (rt_event_recv(audio.rx_ind_event, 1,
                          RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                          rt_tick_from_millisecond(500), &evt) == RT_EOK)
        {
            rt_device_read(audio.audprc_dev, 0, buf, sizeof(buf));
        }
        db_monitor_running = true;
    }

    /* 等待新数据就绪 */
    rt_event_recv(audio.rx_ind_event, 1,
                  RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                  RT_WAITING_FOREVER, &evt);

    /* 读取一帧数据 */
    len = rt_device_read(audio.audprc_dev, 0, buf, sizeof(buf));
    if (len <= 0)
        return audio.current_db;

    samples = len / sizeof(int16_t);
    for (int i = 0; i < samples; i++)
        sum_sq += (int64_t)buf[i] * buf[i];

    if (sum_sq == 0 || samples == 0)
        return audio.current_db;

    rms = sqrt((double)sum_sq / samples);
    db = (int)(20.0 * log10(rms / 32768.0));
    if (db < -100)
        db = -100;

    audio.current_db = db;
    return db;
}

void audio_db_monitor_stop(void)
{
    if (db_monitor_running)
    {
        stop_rx();
        db_monitor_running = false;
        audio.current_db = -100;
        AUDIO_LOG_DEBUG("dB monitor stopped\n");
    }
}

/* 录音线程 */
static void audprc_rx_entry(void *parameter)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    rt_uint32_t evt;
    rt_size_t get_mic_data_len;
    uint32_t wav_data_size = 0;
    rt_bool_t is_wav = audio.is_wav;
    g_tmp_pos = 0;
    memset(g_audrx_buf, 0, sizeof(g_audrx_buf));
    while (audio.is_recording)
    {
        rt_event_recv(audio.rx_ind_event, 1,
                      RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_FOREVER, &evt);
        while (1)
        {
            /* RX read (from mic)*/
            get_mic_data_len = rt_device_read(audio.audprc_dev, 0, mic_data,
                                              AUDIO_BUF_SIZE / 2);
            if (get_mic_data_len != (AUDIO_BUF_SIZE / 2))
            {
                AUDIO_LOG_DEBUG("Got abnormal audio size = %d\n",
                                get_mic_data_len);
            }

            /* 计算当前麦克风 dBFS 值 */
            {
                int db_samples = get_mic_data_len / sizeof(int16_t);
                int64_t sum_sq = 0;
                for (int i = 0; i < db_samples; i++)
                    sum_sq += (int64_t)((int16_t *)mic_data)[i] * ((int16_t *)mic_data)[i];
                if (sum_sq > 0 && db_samples > 0)
                {
                    double rms = sqrt((double)sum_sq / db_samples);
                    audio.current_db = (int)(20.0 * log10(rms / 32768.0));
                    if (audio.current_db < -100)
                        audio.current_db = -100;
                }
                else
                {
                    audio.current_db = -100;
                }
                AUDIO_LOG_DEBUG("Mic dBFS=%d (samples=%d)\n", audio.current_db, db_samples);
            }

            /* 暂停时丢弃数据（但必须继续读取，防止 DMA FIFO 溢出） */
            if (audio.record_paused)
                break;

            if (audio.is_recording == false)
                break;

            if (audio.file_path)
            {
                if (audio.file_fd > 0)
                {
                    int written =
                        write(audio.file_fd, mic_data, AUDIO_BUF_SIZE / 2);
                    if (written != (AUDIO_BUF_SIZE / 2))
                    {
                        struct statfs fs_stat;
                        if (statfs(audio.file_path, &fs_stat) == 0)
                        {
                            uint64_t free_bytes =
                                (uint64_t)fs_stat.f_bfree * fs_stat.f_bsize;
                            AUDIO_LOG_DEBUG(
                                "Write failed! FS free space: %llu bytes\n",
                                free_bytes);
                        }
                        else
                        {
                            AUDIO_LOG_DEBUG(
                                "Write failed! Cannot get FS info.\n");
                        }
                        audio.is_recording = false; // 空间不足，停止录音
                        break;
                    }
                    wav_data_size += written;
                }
            }
            else /* 传入是空文件路径，存入ram内存中*/
            {
                if ((g_tmp_pos + (AUDIO_BUF_SIZE / 2)) < AUDRX_BUF_MAX)
                {
                    memcpy(&g_audrx_buf[g_tmp_pos], mic_data,
                           AUDIO_BUF_SIZE / 2);
                    g_tmp_pos += (AUDIO_BUF_SIZE / 2);
                }
                else
                {
                    audio.is_recording = false;
                    break;
                }
            }
        }
    }

    AUDIO_LOG_DEBUG("Record finished.\n");
    stop_rx();
    audio.is_recording = false;
    if (audio.file_fd >= 0)
    {
        if (is_wav)
        {
            wav_header_t hdr;
            wav_header_init(&hdr, audio.samplerate, audio.channels,
                            audio.bit_depth);
            hdr.data_size = wav_data_size;
            hdr.riff_size = wav_data_size + sizeof(wav_header_t) - 8;
            lseek(audio.file_fd, 0, SEEK_SET);
            write(audio.file_fd, &hdr, sizeof(hdr));
            AUDIO_LOG_DEBUG("WAV header updated, data_size=%d\n",
                            wav_data_size);
        }
        close(audio.file_fd);
        audio.file_fd = -1;
        audio.file_path = NULL;
    }
}

void audio_record_start(const char *filepath)
{
    /* 停止 dB 监听，避免冲突 */
    audio_db_monitor_stop();

    if (filepath == NULL)
    {
        AUDIO_LOG_DEBUG("File path is NULL,rx save ram\n");
    }
    else
    {
        audio.file_path = filepath;
        AUDIO_LOG_DEBUG("Start recording to %s\n", audio.file_path);

        audio.file_fd =
            open(audio.file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (audio.file_fd < 0)
        {
            AUDIO_LOG_DEBUG("Cannot open file %s for write", audio.file_path);
            return;
        }

        // WAV format: write header before recording
        audio.is_wav = has_wav_ext(audio.file_path);
        if (audio.is_wav)
        {
            wav_header_t hdr;
            wav_header_init(&hdr, audio.samplerate, audio.channels,
                            audio.bit_depth);
            write(audio.file_fd, &hdr, sizeof(hdr));
            AUDIO_LOG_DEBUG("WAV header written\n");
        }
    }

    audio.is_recording = true;
    audio.rx_thread =
        rt_thread_create("audprc_rx", audprc_rx_entry, NULL, 2048,
                         RT_THREAD_PRIORITY_HIGH, RT_THREAD_TICK_DEFAULT);
    if (audio.rx_thread == NULL)
    {
        AUDIO_LOG_DEBUG("Create rx thread fail\n");
        close(audio.file_fd);
        audio.file_path = NULL;
        audio.file_fd = -1;
        return;
    }
    rt_thread_startup(audio.rx_thread);
    config_rx();
    start_rx();
}

void audio_record_pause(void)
{
    audio.record_paused = true;
    AUDIO_LOG_DEBUG("Recording paused\n");
}

void audio_record_resume(void)
{
    audio.record_paused = false;
    AUDIO_LOG_DEBUG("Recording resumed\n");
}

void audio_record_stop(void)
{
    if (!audio.is_recording)
        return;
    audio.is_recording = false;
    AUDIO_LOG_DEBUG("Recording stop\n");
}

/* 播放线程 */
static void audprc_tx_entry(void *parameter)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    rt_uint32_t evt;
    uint32_t wr_offset = 0;
    while (audio.is_playing)
    {
        rt_event_recv(audio.tx_wirte_event, 1,
                      RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                      RT_WAITING_FOREVER, &evt);

        /* 暂停时写入静音保持 DMA 运转 */
        if (audio.play_paused)
        {
            memset(sd_play_data, 0, AUDIO_BUF_SIZE / 2);
            rt_device_write(audio.audprc_dev, 0, sd_play_data,
                            AUDIO_BUF_SIZE / 2);
            continue;
        }

        if (audio.file_path)
        {
            if (audio.file_fd > 0)
            {
                int paly_data_len =
                    read(audio.file_fd, sd_play_data, AUDIO_BUF_SIZE / 2);
                if (paly_data_len <= 0)
                {
                    AUDIO_LOG_DEBUG("End of file or read error :%d\n",
                                    paly_data_len);
                    break;
                }

                int len = rt_device_write(audio.audprc_dev, 0, sd_play_data,
                                          AUDIO_BUF_SIZE / 2);
                if (len != (AUDIO_BUF_SIZE / 2))
                {
                    AUDIO_LOG_DEBUG("Abnormal write len :%d\n", len);
                }
            }
        }
        else /* 读取RAM的录音文件*/
        {
            int len =
                rt_device_write(audio.audprc_dev, 0, &g_audrx_buf[wr_offset],
                                AUDIO_BUF_SIZE / 2);
            if (len != (AUDIO_BUF_SIZE / 2))
            {
                AUDIO_LOG_DEBUG("Abnormal write len :%d\n", len);
            }
            wr_offset += (AUDIO_BUF_SIZE / 2);
            if (wr_offset + (AUDIO_BUF_SIZE / 2) >= AUDRX_BUF_MAX)
            {
                audio.is_playing = false;
                break;
            }
        }
    }

    AUDIO_LOG_DEBUG("Play finished.\n");
    stop_tx();
    audio.is_playing = false;
    if (audio.file_fd >= 0)
    {
        close(audio.file_fd);
        audio.file_fd = -1;
    }
}

void audio_play_start(const char *filepath)
{
    if (filepath == NULL)
    {
        AUDIO_LOG_DEBUG("File path is NULL,rx save ram\n");
    }
    else
    {
        audio.file_path = filepath;
        AUDIO_LOG_DEBUG("Start recording to %s\n", audio.file_path);

        audio.file_fd = open(audio.file_path, O_RDONLY);
        if (audio.file_fd < 0)
        {
            AUDIO_LOG_DEBUG("Cannot open file %s for write", audio.file_path);
            return;
        }

        // WAV format: skip header for playback
        if (has_wav_ext(audio.file_path))
        {
            lseek(audio.file_fd, sizeof(wav_header_t), SEEK_SET);
            AUDIO_LOG_DEBUG("Skipped WAV header for playback\n");
        }
    }

    audio.is_playing = true;
    audio.tx_thread =
        rt_thread_create("audprc_tx", audprc_tx_entry, NULL, 2048,
                         RT_THREAD_PRIORITY_HIGH, RT_THREAD_TICK_DEFAULT);
    if (audio.tx_thread == NULL)
    {
        AUDIO_LOG_DEBUG("Create audprc_tx thread fail\n");
        return;
    }
    rt_thread_startup(audio.tx_thread);

    config_tx();
    start_tx();
}

void audio_play_pause(void)
{
    audio.play_paused = true;

    /* 静音并关闭 PA */
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_MUTE, (void *)1);
    audio_pa_close();
    AUDIO_LOG_DEBUG("Playing paused\n");
}

void audio_play_resume(void)
{
    audio.play_paused = false;

    audio_pa_open();
    rt_thread_mdelay(30);
    rt_device_control(audio.audcodec_dev, AUDIO_CTL_MUTE, (void *)0);
    AUDIO_LOG_DEBUG("Playing resumed\n");
}

void audio_play_stop(void)
{
    if (!audio.is_playing)
        return;

    audio.is_playing = false;
    audio.play_paused = false;

    AUDIO_LOG_DEBUG("Playing stop\n");
}

/* MP3播放线程 */
void mp3_playlist_scan(const char *dir)
{
    DIR *d;
    struct dirent *ent;
    int count = 0;

    audio.mp3_playlist_count = 0;
    audio.mp3_current_index = -1;

    if (dir == NULL)
        dir = "/music";

    d = opendir(dir);
    if (d == NULL)
    {
        AUDIO_LOG_DEBUG("Cannot open dir %s\n", dir);
        return;
    }

    while ((ent = readdir(d)) != NULL && count < MP3_PLAYLIST_MAX)
    {
        const char *ext = strrchr(ent->d_name, '.');
        if (ext && (strcmp(ext, ".mp3") == 0 || strcmp(ext, ".MP3") == 0))
        {
            rt_snprintf(audio.mp3_playlist[count], MP3_FILENAME_MAX,
                        "%s/%s", dir, ent->d_name);
            count++;
        }
    }
    closedir(d);

    /* Bubble sort by full path/filename */
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = 0; j < count - 1 - i; j++)
        {
            if (strcmp(audio.mp3_playlist[j], audio.mp3_playlist[j + 1]) > 0)
            {
                char tmp[MP3_FILENAME_MAX];
                strncpy(tmp, audio.mp3_playlist[j], MP3_FILENAME_MAX);
                strncpy(audio.mp3_playlist[j], audio.mp3_playlist[j + 1], MP3_FILENAME_MAX);
                strncpy(audio.mp3_playlist[j + 1], tmp, MP3_FILENAME_MAX);
            }
        }
    }

    audio.mp3_playlist_count = count;
    AUDIO_LOG_DEBUG("Found %d MP3 files in %s\n", count, dir);
    for (int i = 0; i < count; i++)
    {
        AUDIO_LOG_DEBUG("  [%d] %s\n", i, audio.mp3_playlist[i]);
    }
}

static bool mp3_thread_ensure(void)
{
    if (audio.mp3_thread == NULL)
    {
        audio.mp3_thread =
            rt_thread_create("mp3_thread", mp3_proc_thread_entry, NULL, 2048,
                             RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
        if (audio.mp3_thread == NULL)
        {
            AUDIO_LOG_DEBUG("Create mp3_thread fail\n");
            return false;
        }
        if (rt_thread_startup(audio.mp3_thread) != RT_EOK)
        {
            AUDIO_LOG_DEBUG("Start mp3_thread fail\n");
            audio.mp3_thread = NULL;
            return false;
        }
    }
    return true;
}

void mp3_play_prev(void)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    if (!mp3_thread_ensure())
        return;
    mp3_ctrl_info_t info = {0};
    info.cmd = CMD_MP3_PREV;
    send_msg_to_mp3_proc(&info);
}

void mp3_play_next(void)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    if (!mp3_thread_ensure())
        return;
    mp3_ctrl_info_t info = {0};
    info.cmd = CMD_MP3_NEXT;
    send_msg_to_mp3_proc(&info);
}

void mp3_play_shuffle_toggle(void)
{
    audio.mp3_shuffle = !audio.mp3_shuffle;
    AUDIO_LOG_DEBUG("Shuffle %s\n", audio.mp3_shuffle ? "ON" : "OFF");
}

rt_bool_t mp3_play_shuffle_is_enabled(void)
{
    return audio.mp3_shuffle;
}

void mp3_play_stop(void)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    mp3_ctrl_info_t info = {0};
    info.cmd = CMD_MP3_STOP;
    send_msg_to_mp3_proc(&info);
}

void mp3_play_pause(void)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    mp3_ctrl_info_t info = {0};
    info.cmd = CMD_MP3_PAUSE;
    send_msg_to_mp3_proc(&info);
}

void mp3_play_resume(void)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    mp3_ctrl_info_t info = {0};
    info.cmd = CMD_MP3_RESUME;
    send_msg_to_mp3_proc(&info);
}

void mp3_play_start(const char *file_name, uint32_t loop)
{
    AUDIO_LOG_DEBUG("%s %s\n", __func__, file_name);
    mp3_ctrl_info_t info = {0};

    info.cmd = CMD_MP3_PALY;
    info.loop = loop;
    info.param.filename = file_name;
    info.param.len = -1;

    send_msg_to_mp3_proc(&info);
}

static int mp3_play_callback_func(audio_server_callback_cmt_t cmd,
                                  void *callback_userdata, uint32_t reserved)
{
    AUDIO_LOG_DEBUG("mp3 %s cmd %d\n", __func__, cmd);
    switch (cmd)
    {
    case as_callback_cmd_play_to_end:
        /* Auto play next song from playlist */
        if (audio.mp3_playlist_count > 0)
            mp3_play_next();
        else
            mp3_play_stop();
        break;

    default:
        break;
    }

    return 0;
}

void mp3_proc_thread_entry(void *params)
{
    AUDIO_LOG_DEBUG("%s\n", __func__);
    rt_err_t err = RT_ERROR;
    mp3_ctrl_info_t msg;

    while (1)
    {
        err = rt_mq_recv(audio.mp3_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
        RT_ASSERT(err == RT_EOK);
        AUDIO_LOG_DEBUG("mp3 msg: cmd %d\n", msg.cmd);
        switch (msg.cmd)
        {
        case CMD_MP3_PALY:
            audio.mp3_paused = false;
            audio.mp3_current_index = -1;
            if (audio.mp3_handle)
            {
                /* Close fistly if mp3 is playing. */
                mp3ctrl_close(audio.mp3_handle);
                audio_pa_close();
            }
            audio.mp3_handle = mp3ctrl_open(
                AUDIO_TYPE_LOCAL_MUSIC, /* audio type, see enum audio_type_t. */
                msg.param.filename,     /* file path */
                mp3_play_callback_func, /* play callback function. */
                NULL);
            if (audio.mp3_handle == NULL)
            {
                AUDIO_LOG_DEBUG("mp3ctrl_open failed:%s\n", msg.param.filename);
                break;
            }
            audio_pa_open();
            rt_thread_mdelay(30);
            /* Set loop times. */
            mp3ctrl_ioctl(
                audio.mp3_handle, /* handle returned by mp3ctrl_open. */
                0,                /* cmd = 0, set loop times. */
                msg.loop);        /* loop times. */
            /* To play. */
            if(mp3ctrl_play(audio.mp3_handle) != RT_EOK)
            {
                AUDIO_LOG_DEBUG("mp3ctrl_play failed:%s\n", msg.param.filename);
            }
            break;

        case CMD_MP3_STOP:
            if (audio.mp3_handle)
            {
                mp3ctrl_close(audio.mp3_handle);
                audio.mp3_handle = NULL;
            }
            audio_pa_close();
            audio.mp3_paused = false;
            break;

        case CMD_MP3_PAUSE:
            if (audio.mp3_handle && !audio.mp3_paused)
            {
                mp3ctrl_pause(audio.mp3_handle);
                audio_pa_close();
                audio.mp3_paused = true;
                AUDIO_LOG_DEBUG("MP3 paused\n");
            }
            break;

        case CMD_MP3_RESUME:
            if (audio.mp3_handle && audio.mp3_paused)
            {
                audio_pa_open();
                rt_thread_mdelay(30);
                mp3ctrl_resume(audio.mp3_handle);
                audio.mp3_paused = false;
                AUDIO_LOG_DEBUG("MP3 resumed\n");
            }
            break;

        case CMD_MP3_NEXT:
        {
            int next_idx;
            if (audio.mp3_playlist_count == 0)
            {
                AUDIO_LOG_DEBUG("Playlist is empty\n");
                break;
            }
            if (audio.mp3_shuffle)
            {
                /* Random: pick a different song (avoid repeating same song) */
                if (audio.mp3_playlist_count == 1)
                    next_idx = 0;
                else
                    do {
                        next_idx = rand() % audio.mp3_playlist_count;
                    } while (next_idx == audio.mp3_current_index);
            }
            else
            {
                if (audio.mp3_current_index < 0)
                    next_idx = 0;
                else
                    next_idx = (audio.mp3_current_index + 1) % audio.mp3_playlist_count;
            }
            audio.mp3_current_index = next_idx;
            audio.mp3_paused = false;
            AUDIO_LOG_DEBUG("Playing %s [%d/%d]: %s\n",
                            audio.mp3_shuffle ? "random" : "next",
                            next_idx + 1, audio.mp3_playlist_count,
                            audio.mp3_playlist[next_idx]);
            if (audio.mp3_handle)
            {
                mp3ctrl_close(audio.mp3_handle);
                audio_pa_close();
            }
            audio.mp3_handle = mp3ctrl_open(
                AUDIO_TYPE_LOCAL_MUSIC,
                audio.mp3_playlist[next_idx],
                mp3_play_callback_func,
                NULL);
            if (audio.mp3_handle == NULL)
            {
                AUDIO_LOG_DEBUG("mp3ctrl_open failed: %s\n",
                                audio.mp3_playlist[next_idx]);
                break;
            }
            audio_pa_open();
            rt_thread_mdelay(30);
            if (mp3ctrl_play(audio.mp3_handle) != RT_EOK)
            {
                AUDIO_LOG_DEBUG("mp3ctrl_play failed: %s\n",
                                audio.mp3_playlist[next_idx]);
            }
            break;
        }

        case CMD_MP3_PREV:
        {
            int prev_idx;
            if (audio.mp3_playlist_count == 0)
            {
                AUDIO_LOG_DEBUG("Playlist is empty\n");
                break;
            }
            if (audio.mp3_current_index < 0)
                prev_idx = 0;
            else
                prev_idx = (audio.mp3_current_index - 1 + audio.mp3_playlist_count)
                           % audio.mp3_playlist_count;
            audio.mp3_current_index = prev_idx;
            audio.mp3_paused = false;
            AUDIO_LOG_DEBUG("Playing prev [%d/%d]: %s\n",
                            prev_idx + 1, audio.mp3_playlist_count,
                            audio.mp3_playlist[prev_idx]);
            if (audio.mp3_handle)
            {
                mp3ctrl_close(audio.mp3_handle);
                audio_pa_close();
            }
            audio.mp3_handle = mp3ctrl_open(
                AUDIO_TYPE_LOCAL_MUSIC,
                audio.mp3_playlist[prev_idx],
                mp3_play_callback_func,
                NULL);
            if (audio.mp3_handle == NULL)
            {
                AUDIO_LOG_DEBUG("mp3ctrl_open failed: %s\n",
                                audio.mp3_playlist[prev_idx]);
                break;
            }
            audio_pa_open();
            rt_thread_mdelay(30);
            if (mp3ctrl_play(audio.mp3_handle) != RT_EOK)
            {
                AUDIO_LOG_DEBUG("mp3ctrl_play failed: %s\n",
                                audio.mp3_playlist[prev_idx]);
            }
            break;
        }

        default:
            break;
        }
        AUDIO_LOG_DEBUG("mp3 RECV END.\n");
    }
}

void mp3_file_play(const char *filepath)
{
    if (audio.mp3_thread == NULL)
    {
        audio.mp3_thread =
            rt_thread_create("mp3_thread", mp3_proc_thread_entry, NULL, 2048,
                             RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT);
        if (audio.mp3_thread == NULL)
        {
            AUDIO_LOG_DEBUG("Create mp3_thread thread fail\n");
            return;
        }

        rt_err_t err = rt_thread_startup(audio.mp3_thread);
        if (err != RT_EOK)
        {
            AUDIO_LOG_DEBUG("Start mp3_thread thread fail\n");
            return;
        }
    }

    if (filepath == NULL)
    {
        AUDIO_LOG_DEBUG("File path is NULL\n");
        return;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd >= 0)
    {
        close(fd);
        AUDIO_LOG_DEBUG("Start playing mp3 file %s\n", filepath);
        mp3_play_start(filepath, 0);
    }
    else
    {
        AUDIO_LOG_DEBUG("File not found or cannot be opened:%s\n", filepath);
        return;
    }
}