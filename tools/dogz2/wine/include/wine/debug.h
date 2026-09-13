/* Logging-only adapter for building the unmodified Wine 6 codec with MinGW.
 * No codec arithmetic or ACM behavior is replaced by this header. */
#define WINE_DEFAULT_DEBUG_CHANNEL(channel)
#define TRACE(...) ((void)0)
#define WARN(...) ((void)0)
#define FIXME(...) ((void)0)
#define ERR(...) ((void)0)
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif
