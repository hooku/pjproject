/* $Id$ */
#include <Typedef.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pjmedia-audiodev/audiodev_imp.h>

#include <sys/syscall.h>

#include "airb/vp_interface.h"
#include "airb_dev.h"

#if PJMEDIA_AUDIO_DEV_HAS_AIRB_AUDIO

#define THIS_FILE "airb_dev.c"

/* airb_audio device info */
struct airb_audio_dev_info
{
    pjmedia_aud_dev_info info;
    unsigned dev_id;
};

/* airb_audio factory */
struct airb_audio_factory
{
    pjmedia_aud_dev_factory base;
    pj_pool_t *pool;
    pj_pool_factory *pf;

    unsigned dev_count;
    struct airb_audio_dev_info *dev_info;
};

/* Sound stream. */
struct airb_audio_stream
{
    pjmedia_aud_stream base; /**< Base stream	       */
    pjmedia_aud_param param; /**< Settings	       */
    pj_pool_t *pool;         /**< Memory pool.          */

    pjmedia_aud_rec_cb ca_cb;  /**< Capture callback.     */
    pjmedia_aud_play_cb pb_cb; /**< Playback callback.    */
    void *user_data;           /**< Application data.     */

    pj_thread_t *airb_thread_ca; /* capture thread */
    pj_thread_t *airb_thread_pb; /* playback thread */

    char *pb_buf;           /* playback buffer ptr */
    unsigned pb_buf_size;   /* playback buffer size */
    unsigned int pb_frames; /* samples_per_frame */

    int quit;
};

/* Prototypes */
static pj_status_t airb_factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t airb_factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t airb_factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned airb_factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t airb_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info);
static pj_status_t airb_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param);
static pj_status_t airb_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_aud_strm);

static pj_status_t airb_stream_get_param(pjmedia_aud_stream *strm,
                                         pjmedia_aud_param *param);
static pj_status_t airb_stream_get_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       void *value);
static pj_status_t airb_stream_set_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       const void *value);
static pj_status_t airb_stream_start(pjmedia_aud_stream *strm);
static pj_status_t airb_stream_stop(pjmedia_aud_stream *strm);
static pj_status_t airb_stream_destroy(pjmedia_aud_stream *strm);

static int airb_threadfunc_pb(void *arg);
static int airb_threadfunc_ca(void *arg);

static pj_status_t airb_open_pb(struct airb_audio_stream *stream,
                                const pjmedia_aud_param *param);

/* Operations */
static pjmedia_aud_dev_factory_op factory_op = {
    &airb_factory_init,
    &airb_factory_destroy,
    &airb_factory_get_dev_count,
    &airb_factory_get_dev_info,
    &airb_factory_default_param,
    &airb_factory_create_stream,
    &airb_factory_refresh,
};

static pjmedia_aud_stream_op stream_op = {
    &airb_stream_get_param,
    &airb_stream_get_cap,
    &airb_stream_set_cap,
    &airb_stream_start,
    &airb_stream_stop,
    &airb_stream_destroy,
};

struct airb_audio_stream *g_strm = NULL;

/****************************************************************************
 * Factory operations
 */
/*
 * Init airb_audio audio driver.
 */
pjmedia_aud_dev_factory *pjmedia_airb_audio_factory(pj_pool_factory *pf)
{
    struct airb_audio_factory *f;
    pj_pool_t *pool;

    AB_FUNC_ENT();

    pool = pj_pool_create(pf, "airb audio", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct airb_audio_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    AB_FUNC_LEV();

    return &f->base;
}

/* API: init factory */
static pj_status_t airb_factory_init(pjmedia_aud_dev_factory *f)
{
    struct airb_audio_factory *nf = (struct airb_audio_factory *)f;
    struct airb_audio_dev_info *ndi;

    AB_FUNC_ENT();

    /* Initialize input and output devices here */
    nf->dev_count = 1;
    nf->dev_info = (struct airb_audio_dev_info *)
        pj_pool_calloc(nf->pool, nf->dev_count,
                       sizeof(struct airb_audio_dev_info));
    ndi = &nf->dev_info[0];
    pj_bzero(ndi, sizeof(*ndi));
    strcpy(ndi->info.name, "airb device");
    strcpy(ndi->info.driver, "airb");
    ndi->info.input_count = 1;
    ndi->info.output_count = 1;
    ndi->info.default_samples_per_sec = 8000;
    /* Set the device capabilities here */
    ndi->info.caps = 0;

    /* vp_Init(); */

    PJ_LOG(4, (THIS_FILE, "airb audio initialized"));

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t airb_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct airb_audio_factory *nf = (struct airb_audio_factory *)f;

    AB_FUNC_ENT();

    /* vp_Deinit(); */

    pj_pool_safe_release(&nf->pool);

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t airb_factory_refresh(pjmedia_aud_dev_factory *f)
{
    AB_FUNC_ENT();

    PJ_UNUSED_ARG(f);

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned airb_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct airb_audio_factory *nf = (struct airb_audio_factory *)f;

    AB_FUNC_ENT();
    AB_FUNC_LEV();

    return nf->dev_count;
}

/* API: get device info */
static pj_status_t airb_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info)
{
    struct airb_audio_factory *nf = (struct airb_audio_factory *)f;

    AB_FUNC_ENT();

    PJ_ASSERT_RETURN(index < nf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &nf->dev_info[index].info, sizeof(*info));

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t airb_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param)
{
    struct airb_audio_factory *nf = (struct airb_audio_factory *)f;
    struct airb_audio_dev_info *di = &nf->dev_info[index];

    AB_FUNC_ENT();

    PJ_ASSERT_RETURN(index < nf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_bzero(param, sizeof(*param));
    if (di->info.input_count && di->info.output_count)
    {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
    }
    else if (di->info.input_count)
    {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
    }
    else if (di->info.output_count)
    {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    }
    else
    {
        AB_FUNC_LEV();

        return PJMEDIA_EAUD_INVDEV;
    }

    /* Set the mandatory settings here */
    /* The values here are just some examples */
    param->clock_rate = di->info.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = di->info.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;

    /* Set the device capabilities here */
    param->flags = 0;

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t airb_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_aud_strm)
{
    struct airb_audio_factory *nf = (struct airb_audio_factory *)f;
    pj_pool_t *pool;
    struct airb_audio_stream *strm;

    AB_FUNC_ENT();

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(nf->pf, "airb_audio-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct airb_audio_stream);
    g_strm = strm;

    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    strm->ca_cb = rec_cb;
    strm->pb_cb = play_cb;
    strm->user_data = user_data;

    vp_Init();

    /* Create player stream here */
    if (param->dir & PJMEDIA_DIR_PLAYBACK)
    {
        airb_open_pb(strm, param);
    }

    /* Create capture stream here */
    if (param->dir & PJMEDIA_DIR_CAPTURE)
    {
    }

    /* Apply the remaining settings */
    /* Below is an example if you want to set the output volume */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING)
    {
        airb_stream_set_cap(&strm->base,
                            PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                            &param->output_vol);
    }

    /* Done */
    strm->base.op = &stream_op;
    *p_aud_strm = &strm->base;

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: Get stream info. */
static pj_status_t airb_stream_get_param(pjmedia_aud_stream *s,
                                         pjmedia_aud_param *pi)
{
    struct airb_audio_stream *strm = (struct airb_audio_stream *)s;

    AB_FUNC_ENT();

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    /* Example: Update the volume setting */
    if (airb_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                            &pi->output_vol) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t airb_stream_get_cap(pjmedia_aud_stream *s,
                                       pjmedia_aud_dev_cap cap,
                                       void *pval)
{
    struct airb_audio_stream *strm = (struct airb_audio_stream *)s;

    AB_FUNC_ENT();

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    /* Example: Get the output's volume setting */
    if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING)
    {
        /* Output volume setting */
        *(unsigned *)pval = 0; // retrieve output device's volume here

        AB_FUNC_LEV();

        return PJ_SUCCESS;
    }
    else
    {
        AB_FUNC_LEV();

        return PJMEDIA_EAUD_INVCAP;
    }
}

/* API: set capability */
static pj_status_t airb_stream_set_cap(pjmedia_aud_stream *s,
                                       pjmedia_aud_dev_cap cap,
                                       const void *pval)
{
    struct airb_audio_stream *strm = (struct airb_audio_stream *)s;

    AB_FUNC_ENT();

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    /* Example */
    if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING)
    {
        /* Output volume setting */
        // set output's volume level here

        AB_FUNC_LEV();

        return PJ_SUCCESS;
    }

    AB_FUNC_LEV();

    return PJMEDIA_EAUD_INVCAP;
}

/* API: Start stream. */
static pj_status_t airb_stream_start(pjmedia_aud_stream *strm)
{
    struct airb_audio_stream *stream = (struct airb_audio_stream *)strm;
    pj_status_t status = PJ_SUCCESS;

    AB_FUNC_ENT();

    PJ_LOG(4, (THIS_FILE, "Starting airb audio stream"));

    stream->quit = 0;
    status = pj_thread_create(stream->pool,
                              "airb_capture",
                              airb_threadfunc_ca,
                              stream,
                              0, //ZERO,
                              0,
                              &stream->airb_thread_ca);
    if (status != PJ_SUCCESS)
        return status;

    if (stream->param.dir & PJMEDIA_DIR_CAPTURE)
    {
        status = pj_thread_create(stream->pool,
                                  "airb_playback",
                                  airb_threadfunc_pb,
                                  stream,
                                  0, //ZERO,
                                  0,
                                  &stream->airb_thread_pb);
        if (status != PJ_SUCCESS)
        {
            stream->quit = PJ_TRUE;
            pj_thread_join(stream->airb_thread_pb);
            pj_thread_destroy(stream->airb_thread_pb);
            stream->airb_thread_pb = NULL;
        }
    }

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t airb_stream_stop(pjmedia_aud_stream *strm)
{
    struct airb_audio_stream *stream = (struct airb_audio_stream *)strm;

    AB_FUNC_ENT();

    PJ_LOG(4, (THIS_FILE, "Stopping airb audio stream"));

    stream->quit = 1;

    if (stream->airb_thread_pb)
    {
        AB_DBG("%s(%u): Waiting for playback to stop.",
               __FUNCTION__, (unsigned)syscall(SYS_gettid));
        pj_thread_join(stream->airb_thread_pb);
        AB_DBG("%s(%u): playback stopped.",
               __FUNCTION__, (unsigned)syscall(SYS_gettid));
        pj_thread_destroy(stream->airb_thread_pb);
        stream->airb_thread_pb = NULL;
    }

    if (stream->airb_thread_ca)
    {
        AB_DBG("%s(%u): Waiting for capture to stop.",
               __FUNCTION__, (unsigned)syscall(SYS_gettid));
        pj_thread_join(stream->airb_thread_ca);
        AB_DBG("%s(%u): capture stopped.",
               __FUNCTION__, (unsigned)syscall(SYS_gettid));
        pj_thread_destroy(stream->airb_thread_ca);
        stream->airb_thread_ca = NULL;
    }

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: Destroy stream. */
static pj_status_t airb_stream_destroy(pjmedia_aud_stream *strm)
{
    struct airb_audio_stream *stream = (struct airb_audio_stream *)strm;

    AB_FUNC_ENT();

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    airb_stream_stop(strm);

    vp_Deinit();

    pj_pool_release(stream->pool);

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

static int airb_threadfunc_ca(void *arg)
{
    struct airb_audio_stream *stream = (struct airb_audio_stream *)arg;

    AB_FUNC_ENT();

    while (!stream->quit)
    {
        /* get voice from dsp */
        vp_PollingVoiceStream();

        /* the call back function will call upperlayer automatically */
        /* the call back function airb_ca_cb is registered in init */
        /* TODO: shall use 1 thread? */
    }

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* playback: internet -> dsp */
static int airb_threadfunc_pb(void *arg)
{
    struct airb_audio_stream *stream = (struct airb_audio_stream *)arg;

    pj_timestamp tstamp;
    pjmedia_frame frame;
    char *buf = stream->pb_buf;
    int size = stream->pb_buf_size;
    void *user_data = stream->user_data;
    unsigned int nframes = stream->pb_frames;
    int result;

    AB_FUNC_ENT();

    pj_bzero(buf, size);
    tstamp.u64 = 0;

    while (!stream->quit)
    {
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = buf;
        frame.size = size;
        frame.timestamp.u64 = tstamp.u64;
        frame.bit_info = 0;

        /* get voice from pjsip */
        result = stream->pb_cb(user_data, &frame);

        if (result != PJ_SUCCESS)
        {
            AB_DBG("[AB] pb_cb fail\r\n");
        }

        if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
            pj_bzero(buf, size);

        vp_RTPDecode(frame.buf, frame.size);

        tstamp.u64 += nframes;
    }

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* capture: dsp -> internet */
void airb_ca_cb(char *FramePtr, short FrameLen)
{
    int result;

    pjmedia_frame frame;
    pj_timestamp tstamp;
    void *user_data;

    AB_FUNC_ENT();

    if (g_strm)
    {
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = (void *)FramePtr;
        frame.size = FrameLen;
        frame.timestamp.u64 = tstamp.u64;
        frame.bit_info = 0;

        /* thid = (pthread_t *)pj_thread_get_os_handle(pj_thread_this()); */
        /* TODO: should tid to figure out stream in current thread */
        /* struct airb_audio_stream *stream = (struct airb_audio_stream *)arg; */

        user_data = g_strm->user_data;
        result = g_strm->ca_cb(user_data, &frame);

        if (result != PJ_SUCCESS)
        {
            AB_DBG("[AB] ca_cb fail\r\n");
        }
    }

    AB_FUNC_LEV();
}

static pj_status_t airb_open_pb(struct airb_audio_stream *stream,
                                const pjmedia_aud_param *param)
{
    AB_FUNC_ENT();

    /*
     * 	samples_per_frame = app_config.media_cfg.audio_frame_ptime * 
     *   app_config.media_cfg.clock_rate *
     *   app_config.media_cfg.channel_count / 1000;
     */

    stream->pb_frames = param->samples_per_frame / param->channel_count;
    stream->pb_buf_size = stream->pb_frames * param->channel_count *
                          (param->bits_per_sample / 8);
    stream->pb_buf = (char *)pj_pool_alloc(stream->pool, stream->pb_buf_size);

    AB_FUNC_LEV();
}

static pj_status_t airb_open_ca(struct airb_audio_stream *stream,
                                const pjmedia_aud_param *param)
{
    AB_FUNC_ENT();
    AB_FUNC_LEV();
}

#endif /* PJMEDIA_AUDIO_DEV_HAS_AIRB_AUDIO */
