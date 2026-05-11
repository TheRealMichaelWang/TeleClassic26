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

typedef struct send_buffer {
    puint8* data;
    psize size;
} send_buffer_t;

// Compares two strings
pint tc_string_compare(pconstpointer str1, pconstpointer str2, ppointer data);

// Gzips a byte array
// - input: the byte array to gzip
// - input_size: the size of the input byte array
// - output: the send buffer to store the gziped byte array
// returns TRUE on success, FALSE on failure
// NOTE: input_size is written as a big endian 32-bit integer at the beginning of compression stream
pboolean tc_gzip_byte_array(puint8* input, psize input_size, send_buffer_t* output);

// Deflates a byte array
// - input: the byte array to deflate
// - input_size: the size of the input byte array
// - output: the send buffer to store the deflated byte array
// returns TRUE on success, FALSE on failure
// NOTE: input_size is not written at the beginning of the compression stream
pboolean tc_deflate_byte_array(puint8* input, psize input_size, send_buffer_t* output);

#endif /* TELECLASSIC26_UTILS_H */
