#ifndef TELECLASSIC26_UTILS_H
#define TELECLASSIC26_UTILS_H

#include <plibsys.h>
#include <assert.h>

#define TC_THREADS_UUID_LEN 16

#define TC_ASSERT(condition, MSG) assert(condition)

// Compares two strings
pint tc_string_compare(pconstpointer str1, pconstpointer str2, ppointer data);

#endif /* TELECLASSIC26_UTILS_H */
