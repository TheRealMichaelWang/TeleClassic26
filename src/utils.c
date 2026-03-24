#include <TeleClassic26/utils.h>
#include <string.h>

psize tc_bounded_buffer_length(const pchar* buf, psize max_len) {
    psize len = 0;
    while (len < max_len && buf[len] != '\0') {
        len++;
    }
    return len;
}

static pint base62_digit_value(pchar c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'Z') {
        return (c - 'A') + 10;
    }
    if (c >= 'a' && c <= 'z') {
        return (c - 'a') + 36;
    }
    return -1;
}

pboolean tc_decode_base62_bytes(
    const pchar* base62,
    psize base62_len,
    puchar* out,
    psize out_len
) {
    if (P_UNLIKELY(base62 == NULL || out == NULL || out_len == 0 || base62_len == 0)) {
        return FALSE;
    }

    memset(out, 0, out_len);

    for (psize i = 0; i < base62_len; i++) {
        const pint digit = base62_digit_value(base62[i]);
        if (digit < 0) {
            return FALSE;
        }

        puint carry = (puint)digit;
        for (psize j = out_len; j > 0; j--) {
            const puint value = ((puint)out[j - 1] * 62u) + carry;
            out[j - 1] = (puchar)(value & 0xFFu);
            carry = value >> 8;
        }

        if (carry != 0) {
            return FALSE;
        }
    }

    return TRUE;
}
