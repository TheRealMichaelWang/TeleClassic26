#ifndef TELECLASSIC26_UTILS_H
#define TELECLASSIC26_UTILS_H

#include <TeleClassic26/networking/protocol.h>
#include <plibsys.h>
#include <assert.h>

#define TC_THREADS_UUID_LEN 16

#define TC_ASSERT(condition, MSG) assert(condition)
#define TC_LOG_SESSION(log_macro, session, FORMAT_STR, ...)                  \
    log_macro(                                                               \
        FORMAT_STR " (session: %.*s@%s)", ##__VA_ARGS__,                     \
        TC_PROTOCOL_MAX_STR_LEN,                                             \
        (session)->username,                                                 \
        (session)->authenticated_service                                     \
            ? (session)->authenticated_service->hostname                     \
            : "N/A"                                                          \
    )

// Compares two strings
pint tc_string_compare(pconstpointer str1, pconstpointer str2, ppointer data);

#endif /* TELECLASSIC26_UTILS_H */
