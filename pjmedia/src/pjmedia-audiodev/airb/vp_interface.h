#define VP_DEFAULT_SAMPLE_PER_FRAME
#define VP_DEFAULT_CID 0U

#define VP_DEBUG

#if defined(VP_DEBUG)
#define VP_DBG(...) printf(__VA_ARGS__)
#else
#define VP_DBG(...)
#endif

#define VP_FUNC_ENT() VP_DBG("[VP] Enter %s\r\n", __FUNCTION__)
#define VP_FUNC_LEV() VP_DBG("[VP] Leave %s\r\n", __FUNCTION__)

extern void vp_Init();
extern void vp_Deinit();
extern void vp_TestVoiceStream();
extern void vp_PollingVoiceStream();
extern void vp_RTPDecode(char *FramePtr, int FrameLen);

extern void airb_ca_cb(char *FramePtr, short FrameLen);
/* TODO: Use func ptr */
