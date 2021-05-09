#define DEBUG_LEVEL 3
#if defined(DEBUG_LEVEL) && DEBUG_LEVEL > 0
 #define DEBUG(fmt, args...) fprintf(stderr, "%s:%d:%s(): " fmt, \
    __FILE__, __LINE__, __func__, ##args)
#else
 #define DEBUG(fmt, args...) /* Don't do anything in release builds */
#endif
