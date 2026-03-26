#include <TeleClassic26/utils.h>
#include <TeleClassic26/networking/protocol.h>
#include <string.h>

pint tc_string_compare(pconstpointer str1, pconstpointer str2, ppointer data) {
    return strncmp(str1, str2, TC_PROTOCOL_MAX_STR_LEN);
}