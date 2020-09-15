#define AIRB_DEBUG

#if defined(AIRB_DEBUG)
#define AB_DBG(format, args...) printf(format, args)
#else
#define AB_DBG(format, args...)
#endif

#define AB_FUNC_ENT() AB_DBG("[AB] Enter %s\r\n", __FUNCTION__)
#define AB_FUNC_LEV() AB_DBG("[AB] Leave %s\r\n", __FUNCTION__)
