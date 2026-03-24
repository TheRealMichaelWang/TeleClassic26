#ifndef TELECLASSIC26_UTILS_H
#define TELECLASSIC26_UTILS_H

#include <plibsys.h>
#include <assert.h>

#define TC_ASSERT(condition, MSG) assert(condition)

// Returns the index of '\0' up to max_len, or max_len if not found.
psize tc_bounded_buffer_length(const pchar* buf, psize max_len);

// Decodes a base-62 numeral into a fixed-size big-endian byte buffer.
// Returns FALSE on invalid characters or numeric overflow.
// - base62: the base-62 numeral to decode
// - base62_len: the length of the base-62 numeral
// - out: the buffer to write the decoded bytes to
// - out_len: the length of the output buffer
// - return: TRUE if the base-62 numeral was decoded successfully, FALSE otherwise
pboolean tc_decode_base62_bytes(
    const pchar* base62,
    psize base62_len,
    puchar* out,
    psize out_len
);

#endif /* TELECLASSIC26_UTILS_H */