#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TIME_OUT         = -4,
    AUTH_FAIL_SERVER = -3,
    AUTH_FAIL        = -2,
    FAILURE          = -1,
    SUCCESS          =  0
} requestState;

typedef struct car_s {
    uint8_t* data;
    uint16_t length;
} CarData;

typedef struct request_info_s requestInfo;

requestInfo* request_info_create(const uint8_t* key, size_t key_len, const uint8_t* IPv4, uint16_t port);
void         request_info_free(requestInfo* info);
bool         init_client();
void         cleanup_client();

requestState ping_server(requestInfo* info, unsigned* ping_time);
requestState get_new_profile_id(requestInfo* info, uint8_t* id, size_t id_cap);
requestState get_car_profile(requestInfo* info, const uint8_t* profile_id, const uint8_t* plate_txt, uint8_t* data, size_t data_cap, size_t* written);
requestState save_car_profile(requestInfo* info, const uint8_t* profile_id, const uint8_t* plate_txt, uint8_t* data, size_t data_length);

#endif /* CLIENT_H */

