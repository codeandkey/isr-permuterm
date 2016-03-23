#ifndef ISR3_DEBUG
#define ISR3_DEBUG

#ifdef ISR3_VERBOSE
#define isr3_debugf(x, ...) fprintf(stderr, "[%s] " x, __func__, ##__VA_ARGS__)
#define isr3_debug(x) fprintf(stderr, "[%s] " x, __func__)
#else
#define isr3_debugf(x, ...)
#define isr3_debug(x)
#endif

#define isr3_errf(x, ...) fprintf(stderr, "[%s] " x, __func__, ##__VA_ARGS__)
#define isr3_err(x) fprintf(stderr, "[%s] " x, __func__)

#endif
