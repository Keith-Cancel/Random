#include "client.h"
#include "sha256.h"

#include <winsock2.h>
#include <windows.h>

#include <stdio.h>

#define HEADER_SZ 6

#define MSG_ID_SUCCESS        0x0000
#define MSG_ID_BAD_DECODE     0x0010
#define MSG_ID_BAD_AUTH       0x0011
#define MSG_ID_PING           0x00ff

#define MSG_ID_GET_PROFILE    0x0100
#define MSG_ID_SET_PROFILE    0x0101
#define MSG_ID_GET_CARS       0x0102
#define MSG_ID_NEW_PROFILE    0x0110
#define MSG_ID_PROFILE_EXISTS 0x0111
// 11644473600 / (100 * 10^-9)
#define EPOCH_DIFF 116444736000000000ULL

struct request_info_s {
    uint8_t* dat;
    uint8_t* key;
    size_t   dat_cap;
    size_t   key_len;
    int64_t  last_time;
    uint32_t ip;
    uint16_t port;
};

// get the unix time stamp in miliseconds
static int64_t time_ms() {
    FILETIME f_time;
    GetSystemTimeAsFileTime(&f_time);
    uint64_t time = f_time.dwLowDateTime;
    time |= ((uint64_t)f_time.dwHighDateTime) << 32;
    time -= EPOCH_DIFF;
    time /= 10000; // 10^6 ns in a ms, so 10^6/100
    return time;
}

static int recv_x_bytes(SOCKET s, const uint8_t* data, size_t amt) {
    size_t offset = 0;
    while(offset < amt) {
        int read = recv(s, (char*)(data + offset), amt - offset, 0);
        if(read == SOCKET_ERROR) {
            return SOCKET_ERROR;
        }
        if(read == 0) {
            break;
        }
        offset += read;
    }
    return offset;
}

static int send_x_bytes(SOCKET s, const uint8_t* data, size_t amt) {
    size_t offset = 0;
    while(offset < amt) {
        int sent = send(s, (char*)(data + offset), amt - offset, 0);
        if(sent == SOCKET_ERROR) {
            return SOCKET_ERROR;
        }
        offset += sent;
    }
    return offset;
}

static SOCKET socket_create_IPv4_tcp(uint32_t IPv4, uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == INVALID_SOCKET) {
        printf("Failed to create socket!\n");
        return INVALID_SOCKET;
    }
    DWORD to = 4000; // time out in ms
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&to, sizeof(DWORD));
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(DWORD));
    struct sockaddr_in addr = {
        .sin_addr.s_addr = IPv4,
        .sin_family      = AF_INET,
        .sin_port        = htons(port)
    };
    if(connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("Socket failed to connect!\n");
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

// Functions for editing message buffer

bool request_info_expand_buffer(requestInfo* info, size_t expand_by) {
    uint8_t* tmp = realloc(info->dat, info->dat_cap + expand_by + 1);
    if(tmp == NULL) {
        return false;
    }
    info->dat      = tmp;
    info->dat_cap += expand_by;
    return true;
}

static bool get_has_hmac(requestInfo* info) {
    return info->dat[2] & 1;
}

static bool get_has_timestamp(requestInfo* info) {
    return info->dat[3] & 1;
}

static uint16_t get_length(requestInfo* info) {
    uint16_t len;
    memcpy(&len, info->dat + 4, 2);
    len = ntohs(len);
    return len;
}

static uint16_t get_msg_id(requestInfo* info) {
    uint16_t id;
    memcpy(&id, info->dat, 2);
    id = ntohs(id);
    return id;
}

static int64_t get_time_stamp(requestInfo* info) {
    if(!get_has_timestamp(info)) {
        return 0;
    }
    size_t pos = HEADER_SZ + get_length(info);
    if(info->dat_cap < pos + 8) {
        return 0;
    }
    uint64_t time = 0;
    for(int i = 0; i < 8; i++) {
        time <<= 8;
        time  |= (info->dat + pos)[i];
    }
    return time;
}

static size_t get_total_size(requestInfo* info) {
    size_t len = HEADER_SZ;
    len += get_length(info);
    len += get_has_timestamp(info) * 8;
    len += get_has_hmac(info) * 32;
    return len;
}
/*
void print_long_hex_str(uint8_t* bytes, size_t len) {
    for(size_t i = 0; i < len; i++) {
        if(i > 0 && (i % 4) == 0) {
            printf(" ");
        }
        printf("%x%x", bytes[i] >> 4, bytes[i] & 0xf);
    }
    printf("\n");
}
*/
static bool is_hmac_valid(requestInfo* info) {
    if(!get_has_hmac(info)) {
        return false;
    }
    size_t msg_len = get_length(info);
    msg_len += HEADER_SZ;
    msg_len += get_has_timestamp(info) * 8;
    if(info->dat_cap < msg_len + 32) {
        return false;
    }
    uint8_t hash[32] = { 0 };
    sha256_hmac(
        info->key, info->key_len,
        info->dat, msg_len,
        hash, 32
    );
    bool valid = true;
    for(int i = 0; i < 32; i++) {
        valid &= hash[i] == (info->dat + msg_len)[i];
    }
    return valid;
}

static void set_has_hmac(requestInfo* info, bool val) {
    info->dat[2] = val & 1;
}

static void set_has_timestamp(requestInfo* info, bool val) {
    info->dat[3] = val & 1;
}

static void set_length(requestInfo* info, uint16_t len) {
    uint16_t tmp = htons(len);
    memcpy(info->dat + 4, &tmp, 2);
}

static void set_msg_id(requestInfo* info, uint16_t id) {
    uint16_t tmp = htons(id);
    memcpy(info->dat, &tmp, 2);
}

static bool append_body(requestInfo* info, const uint8_t* body, uint16_t len) {
    size_t length = get_length(info);
    size_t buf_sz = HEADER_SZ + length + len;
    if(info->dat_cap < buf_sz) {
        if(!request_info_expand_buffer(info, buf_sz - info->dat_cap)) {
            printf("Buffer failed! %d\n", len);
            return false;
        }
    }
    set_length(info, len + length);
    memcpy(info->dat + HEADER_SZ + length, body, len);
    set_has_hmac(info, false);
    set_has_timestamp(info, false);
    return true;
}

static bool set_body(requestInfo* info, const uint8_t* body, uint16_t len) {
    size_t buf_sz = HEADER_SZ + len;
    if(info->dat_cap < buf_sz) {
        if(!request_info_expand_buffer(info, buf_sz - info->dat_cap)) {
            return false;
        }
    }
    set_length(info, len);
    memcpy(info->dat + HEADER_SZ, body, len);
    set_has_hmac(info, false);
    set_has_timestamp(info, false);
    return true;
}

static bool set_time(requestInfo* info, int64_t time) {
    uint8_t bytes[8] = { 0 };
    uint64_t tmp     = time;
    size_t body_len  = get_length(info);
    size_t buf_sz    = HEADER_SZ + body_len + 8;
    if(info->dat_cap < buf_sz) {
        if(!request_info_expand_buffer(info, buf_sz - info->dat_cap)) {
            return false;
        }
    }
    for(int i = 0; i < 8; i++) {
        bytes[7 - i] = tmp & 0xff;
        tmp >>= 8;
    }
    memcpy(info->dat + HEADER_SZ + body_len, bytes, 8);
    set_has_timestamp(info, true);
    return true;
}

static bool set_time_now(requestInfo* info) {
    return set_time(info, time_ms());
}

static bool gen_hmac(requestInfo* info) {
    size_t msg_len = get_length(info);
    msg_len += HEADER_SZ;
    msg_len += get_has_timestamp(info) * 8;
    size_t buf_sz = msg_len + 32;
    if(info->dat_cap < buf_sz) {
        if(!request_info_expand_buffer(info, buf_sz - info->dat_cap)) {
            return false;
        }
    }
    set_has_hmac(info, true);
    sha256_hmac(
        info->key, info->key_len,
        info->dat, msg_len,
        info->dat + msg_len, 32
    );
    return true;
}

// Process server reponse
static requestState get_response(SOCKET s, requestInfo* info, bool need_hmac) {
    memset(info->dat, 0, info->dat_cap);
    if(recv_x_bytes(s, info->dat, HEADER_SZ) != HEADER_SZ) {
        printf("Failed to receive response header: ");
        if(WSAGetLastError() == WSAETIMEDOUT) {
            printf("Time out\n");
            return TIME_OUT;
        }
        printf("Network Failure\n");
        return FAILURE;
    }
    size_t total_size = get_total_size(info);
    // Make sure info buffer has enough room for entire message
    if(info->dat_cap < total_size) {
        if(!request_info_expand_buffer(info, total_size - info->dat_cap)) {
            printf("Failed to allocate memory!\n");
            return FAILURE;
        }
    }

    total_size -= HEADER_SZ;
    bool has_time   = get_has_timestamp(info);
    bool has_hmac   = get_has_hmac(info);
    uint16_t msg_id = get_msg_id(info);

    if(need_hmac && !has_hmac) {
        return AUTH_FAIL_SERVER;
    }

    if(recv_x_bytes(s, info->dat + HEADER_SZ, total_size) != total_size) {
        printf("Failed to receive response body: ");
        if(WSAGetLastError() == WSAETIMEDOUT) {
            printf("Time out\n");
            return TIME_OUT;
        }
        printf("Network Failure\n");
        return FAILURE;
    }
    if(has_hmac) {
        if(!is_hmac_valid(info)) {
            printf("HMAC or time is not valid!\n");
            return AUTH_FAIL_SERVER;
        }
        if(!has_time) {
            printf("HMAC requires time!\n");
        }
        int64_t t = get_time_stamp(info);
        if(t <= info->last_time) {
            printf("Time stamp too old! %lld < %lld\n", t, info->last_time);
            return AUTH_FAIL_SERVER;
        }
        info->last_time = t;
    }
    switch(msg_id) {
        case MSG_ID_SUCCESS:
            return SUCCESS;
        case MSG_ID_BAD_AUTH:
            printf("Received an Auth Failed response!\n");
            return AUTH_FAIL;
        default:
            printf("Unkown message ID: %02x\n", msg_id);
            return FAILURE;
    }
}

static requestState send_reponse(SOCKET s, requestInfo* info) {
    if(!set_time_now(info) || !gen_hmac(info)) {
        return FAILURE;
    }
    int tot = get_total_size(info);
    if(send_x_bytes(s, info->dat, tot) != tot) {
        if(WSAGetLastError() == WSAETIMEDOUT) {
            return TIME_OUT;
        }
        return FAILURE;
    }
    return SUCCESS;
}

requestState ping_server(requestInfo* info, unsigned* ping_time) {
    requestState ret = SUCCESS;
    uint64_t time = time_ms();
    SOCKET s = socket_create_IPv4_tcp(info->ip, info->port);
    if(s == INVALID_SOCKET) {
        if(WSAGetLastError() == WSAETIMEDOUT) {
            return TIME_OUT;
        }
        return FAILURE;
    }

    memset(info->dat, 0, info->dat_cap);
    set_msg_id(info, MSG_ID_PING);
    ret = send_reponse(s, info);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }

    ret = get_response(s, info, false);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }
    *ping_time = time_ms() - time;

    closesocket(s);
    return SUCCESS;
}

requestState get_new_profile_id(requestInfo* info, uint8_t* id, size_t id_cap) {
    SOCKET s = socket_create_IPv4_tcp(info->ip, info->port);
    if(s == INVALID_SOCKET) {
        if(WSAGetLastError() == WSAETIMEDOUT) {
            return TIME_OUT;
        }
        return FAILURE;
    }

    memset(info->dat, 0, info->dat_cap);
    set_msg_id(info, MSG_ID_NEW_PROFILE);
    requestState ret = send_reponse(s, info);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }

    ret = get_response(s, info, true);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }

    uint16_t len = get_length(info);
    len = len > (id_cap - 1) ? id_cap - 1 : len;
    id[len] = '\0';
    memcpy(id, info->dat + HEADER_SZ, len);

    closesocket(s);
    return SUCCESS;
}

requestState get_car_profile(requestInfo* info, const uint8_t* profile_id, const uint8_t* plate_txt, uint8_t* data, size_t data_cap, size_t* written) {
    memset(info->dat, 0, info->dat_cap);

    set_msg_id(info, MSG_ID_GET_PROFILE);

    uint8_t profile_len = strlen((const char*)profile_id);
    uint8_t plate_len   = strlen((const char*)plate_txt);

    bool appended = true;
    appended &= append_body(info, &profile_len, 1);
    appended &= append_body(info, profile_id, profile_len);

    appended &= append_body(info, &plate_len, 1);
    appended &= append_body(info, plate_txt, plate_len);

    if(appended == false) {
        return FAILURE;
    }

    SOCKET s = socket_create_IPv4_tcp(info->ip, info->port);
    if(s == INVALID_SOCKET) {
        if(WSAGetLastError() == WSAETIMEDOUT) {
            return TIME_OUT;
        }
        return FAILURE;
    }

    requestState ret = send_reponse(s, info);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }

    ret = get_response(s, info, true);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }

    uint16_t length = get_length(info);
    length = length > data_cap ? data_cap : length;

    memcpy(data, info->dat + HEADER_SZ, length);
    *written = length;

    closesocket(s);
    return SUCCESS;
}

requestState save_car_profile(requestInfo* info, const uint8_t* profile_id, const uint8_t* plate_txt, uint8_t* data, size_t data_length) {
    memset(info->dat, 0, info->dat_cap);

    set_msg_id(info, MSG_ID_SET_PROFILE);

    uint8_t profile_len = strlen((const char*)profile_id);
    uint8_t plate_len   = strlen((const char*)plate_txt);

    bool appended = true;
    appended &= append_body(info, &profile_len, 1);
    appended &= append_body(info, profile_id, profile_len);

    appended &= append_body(info, &plate_len, 1);
    appended &= append_body(info, plate_txt, plate_len);

    data_length      &= 0xffff;
    uint16_t data_len = htons(data_length);
    appended &= append_body(info, (uint8_t*)&data_len, 2);
    appended &= append_body(info, data, data_length);

    if(appended == false) {
        return FAILURE;
    }

    SOCKET s = socket_create_IPv4_tcp(info->ip, info->port);
    if(s == INVALID_SOCKET) {
        if(WSAGetLastError() == WSAETIMEDOUT) {
            return TIME_OUT;
        }
        return FAILURE;
    }

    requestState ret = send_reponse(s, info);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }

    ret = get_response(s, info, true);
    if(ret != SUCCESS) {
        closesocket(s);
        return ret;
    }
    closesocket(s);
    return SUCCESS;
}

// Buffer creation and size adjust

requestInfo* request_info_create(const uint8_t* key, size_t key_len, const uint8_t* IPv4, uint16_t port) {
    requestInfo* info = malloc(sizeof(requestInfo));
    if(info == NULL) {
        return NULL;
    }
    info->dat = malloc(512);
    if(info->dat == NULL) {
        free(info);
        return NULL;
    }
    info->key = malloc(key_len);
    if(info->key == NULL) {
        free(info->dat);
        free(info);
        return NULL;
    }
    memset(info->dat, 0, 512);
    memcpy(info->key, key, key_len);
    info->dat_cap   = 512;
    info->key_len   = key_len;
    info->port      = port;
    info->ip        = inet_addr((char*)IPv4);
    info->last_time = time_ms() - (20 * 1000);
    return info;
}

void request_info_free(requestInfo* info) {
    free(info->dat);
    free(info);
}

bool init_client() {
    WSADATA wsa;
	if (WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
        return false;
	}
    return true;
}

void cleanup_client() {
    WSACleanup();
}