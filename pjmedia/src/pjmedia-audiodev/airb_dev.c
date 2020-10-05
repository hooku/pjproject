/* $Id$ */
#include <Typedef.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pjmedia/rtp.h>

#include <sys/syscall.h>

#include "airb/vp_interface.h"
#include "airb_dev.h"

#if PJMEDIA_AUDIO_DEV_HAS_AIRB_AUDIO

#define THIS_FILE "airb_dev.c"
#define BITS_PER_SAMPLE 16
#define RTP_VERSION 2
#define RTP_HDR_LEN sizeof(pjmedia_rtp_hdr)

/*
 * the airbridge dsp driver use the following format:
 * 
 *   Payload type: ITU-T G.711 PCMA(A-law)
 *   64 kbit/s bitrate (8 kHz sampling frequency × 8 bits per sample)
 * 
 *   RTP packet size: 180B UDP, 160B payload = 160 samples
 *   RTP packet interval: 20ms, 50 packet/s
 *   Samples/s = 160 Samples * 50 packet/s = 8000 Samples/s
 * 
 *   The ring tone is generated by pjsip in pcm format
 *
 */

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

    pj_thread_t *airb_thread; /* playback thread */

    char *pb_buf;           /* playback buffer ptr */
    char *pb_buf2;          /* playback buffer2 ptr */
    unsigned pb_buf_size;   /* playback buffer size */
    unsigned int pb_frames; /* playback samples_per_frame */

    char *ca_buf;           /* capture buffer ptr */
    unsigned ca_buf_size;   /* capture buffer size */
    unsigned int ca_frames; /* capture samples_per_frame */

    pj_uint16_t tseq;  /**< sequence number */
    pj_uint32_t tts;   /**< timestamp */
    pj_uint32_t tssrc; /**< synchronization source identifier */

    pj_uint32_t media_format_id; /* pjmedia format: should be LPCM/G.711 */

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

static int airb_threadfunc(void *arg);

static pj_status_t airb_open_pb(struct airb_audio_stream *stream,
                                const pjmedia_aud_param *param);
static pj_status_t airb_open_ca(struct airb_audio_stream *stream,
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
    struct airb_audio_factory *abf = (struct airb_audio_factory *)f;
    struct airb_audio_dev_info *abdi;
    pjmedia_format ext_fmt;

    AB_FUNC_ENT();

    /* Initialize input and output devices here */
    abf->dev_count = 1;
    abf->dev_info = (struct airb_audio_dev_info *)pj_pool_calloc(abf->pool,
                                                                 abf->dev_count,
                                                                 sizeof(struct airb_audio_dev_info));
    abdi = &abf->dev_info[0];
    pj_bzero(abdi, sizeof(*abdi));
    strcpy(abdi->info.name, "airb device");
    strcpy(abdi->info.driver, "airb");
    abdi->info.input_count = 1;
    abdi->info.output_count = 1;
    abdi->info.default_samples_per_sec = 8000;

    /* Set the device capabilities here */
    /* EXT_FORMAT = formats other than PCM
     * Because AC483x is configured as G.711 mode
     */
    abdi->info.caps |= (PJMEDIA_AUD_DEV_CAP_EXT_FORMAT |
                        PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING);

#if PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU
    AB_DBG("[AB] passthrough pcmu\r\n");
    pjmedia_format_init_audio(&ext_fmt, PJMEDIA_FORMAT_PCMU,
                              8000, 1, 16, 20, 64000, 64000);

    abdi->info.ext_fmt[0] = ext_fmt;
    abdi->info.ext_fmt_cnt = 1;
#endif

    vp_Init_DSP();

    PJ_LOG(4, (THIS_FILE, "airb audio initialized"));

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t airb_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct airb_audio_factory *abf = (struct airb_audio_factory *)f;

    AB_FUNC_ENT();

    vp_Deinit_DSP();

    pj_pool_safe_release(&abf->pool);

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
    struct airb_audio_factory *abf = (struct airb_audio_factory *)f;

    AB_FUNC_ENT();

    PJ_ASSERT_RETURN(index < abf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &abf->dev_info[index].info, sizeof(*info));

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t airb_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param)
{
    struct airb_audio_factory *abf = (struct airb_audio_factory *)f;
    struct airb_audio_dev_info *di = &abf->dev_info[index];

    AB_FUNC_ENT();

    PJ_ASSERT_RETURN(index < abf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_bzero(param, sizeof(*param));
    param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param->rec_id = index;
    param->play_id = index;

    /* Set the mandatory settings here */
    /* The values here are just some examples */
    param->clock_rate = di->info.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = di->info.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = BITS_PER_SAMPLE; /* TODO */

    param->flags = PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE;
    param->output_route = PJMEDIA_AUD_DEV_ROUTE_EARPIECE;

    //param->ext_fmt.id = PJMEDIA_FORMAT_PCMU;

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
    struct airb_audio_factory *abf = (struct airb_audio_factory *)f;
    pj_pool_t *pool;
    struct airb_audio_stream *strm;

    AB_FUNC_ENT();

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(abf->pf, "airb_audio-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    /* Can only support 16bits per sample */
    PJ_ASSERT_RETURN(param->bits_per_sample == BITS_PER_SAMPLE, PJ_EINVAL);

    /* Supported clock rates:
     * - for both PCM & non-PCM format(G.711): 8kHz  
     */
    PJ_ASSERT_RETURN(param->clock_rate == 8000, PJ_EINVAL);

    /* Supported channels number:
     * - for both PCM & non-PCM format(G.711): mono
     */
    PJ_ASSERT_RETURN(param->channel_count == 1, PJ_EINVAL);

    strm = PJ_POOL_ZALLOC_T(pool, struct airb_audio_stream);
    switch (param->ext_fmt.id)
    {
    case PJMEDIA_FORMAT_L16:
        AB_DBG("[AB] extfmt LPCM\r\n");
        break;
    case PJMEDIA_FORMAT_PCMU:
        AB_DBG("[AB] extfmt PCMU\r\n");
        /* TODO: have to save g_strm here because call back need */
        g_strm = strm;
        break;
    case PJMEDIA_FORMAT_PCMA:
        /* not supported */
        AB_DBG("[AB] extfmt PCMA\r\n");
        break;
    default:
        /* not supported */
        AB_DBG("[AB] extfmt unknown\r\n");
        break;
    }

    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    strm->ca_cb = rec_cb;
    strm->pb_cb = play_cb;

    pj_assert((param->ext_fmt.id == PJMEDIA_FORMAT_L16) || (param->ext_fmt.id == PJMEDIA_FORMAT_PCMU));
    strm->media_format_id = param->ext_fmt.id;

    strm->user_data = user_data;

    printf("pb_addr = %x\r\n", play_cb);

    /* Create player stream here */
    if (param->dir & PJMEDIA_DIR_PLAYBACK)
    {
        airb_open_pb(strm, param);
    }

    /* Create capture stream here */
    if (param->dir & PJMEDIA_DIR_CAPTURE)
    {
        airb_open_ca(strm, param);
    }

    /* Apply the remaining settings */
    /* Below is an example if you want to set the output volume */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING)
    {
        airb_stream_set_cap(&strm->base,
                            PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                            &param->output_vol);
    }

    strm->tseq = 0;
    strm->tts = 0;
    strm->tssrc = param->play_id;

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

    /* Update the volume setting */
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

    stream->pb_buf = (char *)pj_pool_zalloc(stream->pool, stream->pb_buf_size * 2);
    AB_DBG("[AB] pb_buf sz=%d\r\n", stream->pb_buf_size);
    stream->pb_buf2 = (char *)pj_pool_zalloc(stream->pool, stream->pb_buf_size * 2);
    AB_DBG("[AB] pb_buf2 sz=%d\r\n", stream->pb_buf_size);

    stream->ca_buf = (char *)pj_pool_zalloc(stream->pool, stream->ca_buf_size * 2);
    AB_DBG("[AB] ca_buf sz=%d\r\n", stream->ca_buf_size);

    stream->quit = 0;

    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK)
    {
        status = pj_thread_create(stream->pool,
                                  "airb_thread",
                                  airb_threadfunc,
                                  stream,
                                  0, //ZERO,
                                  0,
                                  &stream->airb_thread);
        if (status != PJ_SUCCESS)
            return status;
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

    if (stream->airb_thread)
    {
        AB_DBG("%s(%u): Waiting for airb_thread to stop.",
               __FUNCTION__, (unsigned)syscall(SYS_gettid));
        pj_thread_join(stream->airb_thread);
        AB_DBG("%s(%u): airb_thread stopped.",
               __FUNCTION__, (unsigned)syscall(SYS_gettid));
        pj_thread_destroy(stream->airb_thread);
        stream->airb_thread = NULL;
    }

    /* all pool items should be released when stream destory */

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
    g_strm = NULL;

    pj_pool_release(stream->pool);

    AB_FUNC_LEV();

    return PJ_SUCCESS;
}

static void airb_dump_data(unsigned int *buf, unsigned int buf_len)
{
    unsigned int i = 0;

#if 1
    VP_DBG("%x %x %x %x %d\n", *buf, *(buf + 1), *(buf + 2), *(buf + 4), buf_len);
#endif

#if 1
    for (i = 0; i < 20; i++)
    {
        if (buf[i] != 0)
        {
            AB_DBG("d=%x, %d\n", buf[i], buf_len);
            break;
        }
    }
#endif

#if 1
    for (i = 0; i < (buf_len / sizeof(unsigned int)); i += 4)
    {
        AB_DBG("[AB] %x %x %x %x\r\n", buf[i], buf[i + 1], buf[i + 2], buf[i + 3]);
    }
#endif
}

static void build_default_rtphdr(pjmedia_rtp_hdr *p_hdr)
{
    p_hdr->v = RTP_VERSION; /**< packet type/version 	   */
    p_hdr->p = 0;           /**< padding flag		   */
    p_hdr->x = 0;           /**< extension flag		   */
    p_hdr->cc = 0;          /**< CSRC count			   */
    p_hdr->m = 0;           /**< marker bit			   */
    p_hdr->pt = 0;          /**< payload type		   */
    p_hdr->seq = 0;         /**< sequence number		    */
    p_hdr->ts = 0;          /**< timestamp			    */
    p_hdr->ssrc = 0;        /**< synchronization source	    */
}

/* playback pcm: internet -> dsp */
static void airb_pb_cb_pcm(struct airb_audio_stream *stream)
{
    /*
     * convert to G.711 RTP to let DSP eat
     *   1. convert from LPCM to G.711
     *   2. add rtp header
     */

    pj_timestamp tstamp;
    pjmedia_frame frame;
    char *buf = stream->pb_buf;
    int size = stream->pb_buf_size;
    void *user_data = stream->user_data;
    int result;

    AB_FUNC_ENT();

    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame.buf = buf;
    frame.size = size;
    frame.timestamp.u64 = tstamp.u64;
    frame.bit_info = 0;

    /* get voice from pjsip */
    //printf("pb_addr2 = %x\r\n", stream->pb_cb);
    result = stream->pb_cb(user_data, &frame);
    pj_assert(frame.type == PJMEDIA_FRAME_TYPE_AUDIO ||
              frame.type == PJMEDIA_FRAME_TYPE_NONE);

    /* convert to G.711 use pjsip api */

    /* play with dsp */

    AB_FUNC_LEV();
}

/* playback non-pcm(G.711): internet -> dsp */
static void airb_pb_cb(struct airb_audio_stream *stream)
{
    pjmedia_frame_ext *frame = stream->pb_buf;
    pjmedia_frame_ext_subframe *sf;
    void *user_data = stream->user_data;
    int result;
    pjmedia_rtp_hdr *rtp_hdr = stream->pb_buf2;
    char *buf = stream->pb_buf2 + sizeof(pjmedia_rtp_hdr);

    pj_uint32_t samples_ready = 0, samples_req = 160;

    AB_FUNC_ENT();

    build_default_rtphdr(rtp_hdr);
    rtp_hdr->seq = pj_htons(stream->tseq);
    rtp_hdr->ts = pj_htonl(stream->tts);
    rtp_hdr->ssrc = pj_htonl(stream->tssrc);

    /* Call parent stream callback to get samples to play. */
    while (samples_ready < samples_req)
    {
        if (frame->samples_cnt == 0)
        {
            //printf("sample=0\r\n");
            frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
            /* get voice from pjsip */
            result = stream->pb_cb(user_data, frame);
            pj_assert(frame->base.type == PJMEDIA_FRAME_TYPE_EXTENDED ||
                      frame->base.type == PJMEDIA_FRAME_TYPE_NONE);
        }

        if (frame->base.type == PJMEDIA_FRAME_TYPE_EXTENDED)
        {
            unsigned samples_cnt;

            //printf("ext!%d\r\n", samples_ready);

            sf = pjmedia_frame_ext_get_subframe(frame, 0);
            samples_cnt = frame->samples_cnt / frame->subframe_cnt;
            if ((sf->data) && (sf->bitlen))
            {
                pj_memcpy(buf, sf->data, (sf->bitlen >> 3));
                buf += (sf->bitlen >> 3);
                samples_ready += samples_cnt;
            }
            else
            {
            }
            pjmedia_frame_ext_pop_subframes(frame, 1);
        }
        else
        {
            /* PJMEDIA_FRAME_TYPE_NONE */
            samples_ready = samples_req;
            frame->samples_cnt = 0;
            frame->subframe_cnt = 0;
        }
    }

    if (samples_ready)
    {
        stream->tseq++;
        stream->tts += 160;

        vp_RTPDecode(rtp_hdr, 180);
    }

    AB_FUNC_LEV();
}

/* capture non-pcm(G.711): dsp -> internet */
void airb_ca_cb(char *FramePtr, short FrameLen)
{
    int result;

    pjmedia_frame_ext *frame;
    void *user_data;
    pj_timestamp tstamp;

    if (g_strm)
    {
        frame = g_strm->ca_buf;

        /* thid = (pthread_t *)pj_thread_get_os_handle(pj_thread_this()); */
        /* TODO: should tid to figure out stream in current thread */
        /* struct airb_audio_stream *stream = (struct airb_audio_stream *)arg; */

        user_data = g_strm->user_data;

        pjmedia_frame_ext_append_subframe(frame, FramePtr + 12, 160 * 8, 160);

        frame->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
        result = g_strm->ca_cb(user_data, frame);

        frame->samples_cnt = 0;
        frame->subframe_cnt = 0;

        if (result != PJ_SUCCESS)
        {
            AB_DBG("[AB] ca_cb fail\r\n");
        }
    }
}

static int airb_threadfunc(void *arg)
{
    struct airb_audio_stream *stream = (struct airb_audio_stream *)arg;

    AB_FUNC_ENT();

    if (stream->media_format_id == PJMEDIA_FORMAT_PCMU)
    {
        while (!stream->quit)
        {
            /* 
             * handle cap voice polling
             * which will call airb_ca_cb();
             */
            vp_PollingVoiceStream();

            airb_pb_cb(stream);

            /*
             * delay a while for CPU to take a breathe
             */
            //usleep(AIRB_THREAD_POLL_INTERVAL);
        }
    }
    else
    {
        while (!stream->quit)
        {
            /* PJMEDIA_FORMAT_L16 */
            airb_pb_cb_pcm(stream);

            /*
             * nothing need to capture in pcm format
             * if vp_PollingVoiceStream not called,
             * vp driver should simply discard all data from DSP
             */
        }
    }

    AB_FUNC_LEV();

    return PJ_SUCCESS;
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

    AB_FUNC_LEV();
}

static pj_status_t airb_open_ca(struct airb_audio_stream *stream,
                                const pjmedia_aud_param *param)
{
    AB_FUNC_ENT();

    stream->ca_frames = param->samples_per_frame / param->channel_count;
    stream->ca_buf_size = stream->ca_frames * param->channel_count *
                          (param->bits_per_sample / 8);

    AB_FUNC_LEV();
}

#endif /* PJMEDIA_AUDIO_DEV_HAS_AIRB_AUDIO */
