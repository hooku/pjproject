#define AIRB_DEBUG 1
#define AIRB_LOG_LEVEL 1

#define AIRB_THREAD_POLL_INTERVAL   5000    /* us */

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
