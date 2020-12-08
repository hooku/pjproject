#define VP_DEFAULT_SAMPLE_PER_FRAME
#define VP_DEFAULT_CID 0U

#define VP_DEBUG 1
#define VP_LOG_LEVEL 1
#define VP_FAST_INIT 0
#define VP_NULL_DSP 0

#if (VP_DEBUG)
#define VP_DBG(...) printf(__VA_ARGS__)
#else /* VP_DEBUG */
#define VP_DBG(...)
#endif /* VP_DEBUG */

#define VP_FUNC_ENT()                                  \
    do                                                 \
    {                                                  \
        if (VP_LOG_LEVEL >= 2)                         \
            VP_DBG("[VP] Enter %s\r\n", __FUNCTION__); \
    } while (0)
#define VP_FUNC_LEV()                                  \
    do                                                 \
    {                                                  \
        if (VP_LOG_LEVEL >= 2)                         \
            VP_DBG("[VP] Leave %s\r\n", __FUNCTION__); \
    } while (0)

extern void vp_Init_DSP();
extern void vp_Deinit_DSP();

extern void vp_TestVoiceStream();
extern void vp_PollingVoiceStream();
extern void vp_RTPDecode(char *FramePtr, int FrameLen);

extern void airb_ca_cb(char *FramePtr, short FrameLen);
/* TODO: Use func ptr */
