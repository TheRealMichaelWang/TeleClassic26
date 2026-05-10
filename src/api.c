#include <TeleClassic26/networking/api.h>

pboolean tc_api_send_message(tc_session_t* session, tc_message_type_t message_type, const pchar message[]) {
    if (tc_session_get_extension_version(session, TC_CPE_MESSAGE_TYPES_EXTENSION_INDEX) >= 0) {
        return tc_send_message(session->client_socket, (pchar)message_type, message);
    }
    return tc_send_message(session->client_socket, 0, message);
}