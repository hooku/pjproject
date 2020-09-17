#define AIRB_DEBUG

#if defined(AIRB_DEBUG)
#define AB_DBG(...) printf(__VA_ARGS__)
#else
#define AB_DBG(...)
#endif

#define AB_FUNC_ENT() AB_DBG("[AB] Enter %s\r\n", __FUNCTION__)
#define AB_FUNC_LEV() AB_DBG("[AB] Leave %s\r\n", __FUNCTION__)

extern void airb_ca_cb(char *FramePtr, short FrameLen);
