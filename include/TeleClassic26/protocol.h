#ifndef TELECLASSIC26_SESSION_H
#define TELECLASSIC26_SESSION_H

#include <plibsys.h>

#define TC_PROTOCOL_MAX_STR_LEN 64

pboolean tc_protocol_kick(PSocket* session, const char msg[]);

#endif /* TELECLASSIC26_SESSION_H */