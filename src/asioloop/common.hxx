#include <fmt/core.h>

#if defined(_MSC_VER)
#define win32 1
#endif

#ifdef PY_ASIO_LOOP_CPP_DEBUG

#define debug_print(format, ...)                                                                   \
                                                                                                   \
    do {                                                                                           \
        printf(__FILE__);                                                                          \
        printf(":");                                                                               \
        printf("%d", __LINE__);                                                                    \
        printf("\t%s", __FUNCTION__);                                                              \
        printf("\tDEBUG: ");                                                                       \
        fmt::println(format, ##__VA_ARGS__);                                                       \
    } while (0)

#else

#define debug_print(...)                                                                           \
    do {                                                                                           \
    } while (0)

#endif
