#define AIRB_DEBUG 1
#define AIRB_LOG_LEVEL 1

#define AIRB_TEST 1

#define AIRB_THREAD_DUAL 1

#define AIRB_SAMPLE_PER_PKT 160

#define AIRB_THREAD_POLL_INTERVAL 1000 /* us */

#define UDP_HDR_LEN 8 /* byte */

#define RTP_VERSION 2
#define RTP_HDR_LEN sizeof(pjmedia_rtp_hdr)
#define BITS_PER_SAMPLE 16

#if (AIRB_DEBUG)
#define AB_DBG(...) printf(__VA_ARGS__)
#else /* AIRB_DEBUG */
#define AB_DBG(...)
#endif /* AIRB_DEBUG */

#define AB_FUNC_ENT()                                  \
    do                                                 \
    {                                                  \
        if (AIRB_LOG_LEVEL >= 2)                       \
            AB_DBG("[AB] Enter %s\r\n", __FUNCTION__); \
    } while (0)
#define AB_FUNC_LEV()                                  \
    do                                                 \
    {                                                  \
        if (AIRB_LOG_LEVEL >= 2)                       \
            AB_DBG("[AB] Leave %s\r\n", __FUNCTION__); \
    } while (0)

extern void airb_ca_cb(char *FramePtr, short FrameLen);
