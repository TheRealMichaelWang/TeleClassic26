#include "plibsys.h"
#include <TeleClassic26/utils.h>
#include <TeleClassic26/networking/protocol.h>
#include <zlib.h>
#include <string.h>

pint tc_string_compare(pconstpointer str1, pconstpointer str2, ppointer data) {
    return strncmp(str1, str2, TC_PROTOCOL_MAX_STR_LEN);
}

static void write_u16_be(puint8 *dst, puint16 v) {
    dst[0] = (puint8)(v >> 8);
    dst[1] = (puint8)(v);
}

static void write_u32_be(puint8 *dst, puint32 v) {
    dst[0] = (puint8)(v >> 24);
    dst[1] = (puint8)(v >> 16);
    dst[2] = (puint8)(v >> 8);
    dst[3] = (puint8)(v);
}

pboolean tc_gzip_byte_array(puint8* input, psize input_size, send_buffer_t* output) {
    z_stream zs;
    int ret;

    output->data = NULL;
    output->size = 0;

    psize new_input_size = input_size + sizeof(puint32);
    puint8* new_input = p_malloc(new_input_size);
    if (!new_input) {
        return FALSE;
    }

    write_u32_be(new_input, input_size);
    memcpy(new_input + sizeof(puint32), input, input_size);

    // initialize the z_stream
    memset(&zs, 0, sizeof(z_stream));
    ret = deflateInit2(
        &zs,
        Z_DEFAULT_COMPRESSION,
        Z_DEFLATED,
        15 + 16, // 15 for the window size, 16 for the gzip header
        8,
        Z_DEFAULT_STRATEGY
    );
    if (ret != Z_OK) {
        p_free(new_input);
        return FALSE;
    }

    psize out_upper_bound = compressBound(new_input_size);
    puint8* compressed_output = p_malloc(out_upper_bound);
    if (!compressed_output) {
        deflateEnd(&zs);
        p_free(new_input);
        return FALSE;
    }

    zs.next_in = new_input;
    zs.avail_in = new_input_size;
    zs.next_out = compressed_output;
    zs.avail_out = out_upper_bound;

    ret = deflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&zs);
        p_free(compressed_output);
        p_free(new_input);
        return FALSE;
    }

    output->data = compressed_output;
    output->size = zs.total_out;

    deflateEnd(&zs);
    p_free(new_input);

    return TRUE;
}

pboolean tc_deflate_byte_array(puint8* input, psize input_size, send_buffer_t* output) {
    z_stream zs;
    int ret;

    output->data = NULL;
    output->size = 0;

    // initialize the z_stream
    memset(&zs, 0, sizeof(z_stream));
    ret = deflateInit2(
        &zs,
        Z_DEFAULT_COMPRESSION,
        Z_DEFLATED,
        -15, // -15 for raw deflate
        8,
        Z_DEFAULT_STRATEGY
    );
    if (ret != Z_OK) {
        return FALSE;
    }

    psize out_upper_bound = compressBound(input_size);
    puint8* compressed_output = p_malloc(out_upper_bound);
    if (!compressed_output) {
        deflateEnd(&zs);
        return FALSE;
    }

    zs.next_in = input;
    zs.avail_in = input_size;
    zs.next_out = compressed_output;
    zs.avail_out = out_upper_bound;

    ret = deflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&zs);
        p_free(compressed_output);
        return FALSE;
    }

    output->data = compressed_output;
    output->size = zs.total_out;

    deflateEnd(&zs);

    return TRUE;
}