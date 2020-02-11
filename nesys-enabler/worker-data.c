/*
MIT License
Copyright (c) 2020 Keith J. Cancel
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "worker-data.h"

#include <windows.h>
#include <string.h>

struct worker_data_s {
    CRITICAL_SECTION crit;
    uint8_t* status;
    uint8_t* profile;
    uint8_t* name;
    uint8_t* ip;
    uint8_t* key;
    size_t   key_len;
    int32_t  drives;
    uint16_t port;
    bool     stop;
};

static bool update_string(CRITICAL_SECTION* crit, const uint8_t* src, uint8_t** dest) {
    size_t len       = strlen((char*)src);
    uint8_t* src_cpy = malloc(len + 1);
    uint8_t* dst_old = NULL;
    if(src_cpy == NULL) {
        return false;
    }
    memcpy(src_cpy, src, len);
    src_cpy[len] = '\0';
    EnterCriticalSection(crit);
    dst_old = *dest;
    *dest   = src_cpy;
    LeaveCriticalSection(crit);
    free(dst_old);
    return true;
}

static uint8_t* get_string(CRITICAL_SECTION* crit, uint8_t** str) {
    uint8_t* tmp = NULL;
    EnterCriticalSection(crit);
    tmp  = *str;
    *str = NULL;
    LeaveCriticalSection(crit);
    return tmp;
}

int32_t get_cur_drives(worker_data* data) {
    EnterCriticalSection(&(data->crit));
    int32_t tmp = data->drives;
    LeaveCriticalSection(&(data->crit));
    return tmp;
}

uint16_t get_cur_port(worker_data* data) {
    EnterCriticalSection(&(data->crit));
    uint16_t tmp = data->port;
    LeaveCriticalSection(&(data->crit));
    return tmp;
}

uint8_t* get_game_name(worker_data* data) {
    return get_string(&(data->crit), &(data->name));
}

uint8_t* get_IPv4_address(worker_data* data) {
    return get_string(&(data->crit), &(data->ip));
}

size_t get_network_key(worker_data* data, uint8_t** key) {
    EnterCriticalSection(&(data->crit));
    *key       = data->key;
    data->key  = NULL;
    size_t tmp = data->key_len;
    LeaveCriticalSection(&(data->crit));
    return tmp;
}

uint8_t* get_profile_id(worker_data* data) {
    return get_string(&(data->crit), &(data->profile));
}

uint8_t* get_status_text(worker_data* data) {
    return get_string(&(data->crit), &(data->status));
}

// Set Functions

bool set_cur_drives(worker_data* data, uint32_t drives) {
    EnterCriticalSection(&(data->crit));
    data->drives = drives;
    LeaveCriticalSection(&(data->crit));
    return true;
}

bool set_cur_port(worker_data* data, uint16_t port) {
    EnterCriticalSection(&(data->crit));
    data->port = port;
    LeaveCriticalSection(&(data->crit));
    return true;
}

bool set_game_name(worker_data* data, const uint8_t* txt) {
    return update_string(&(data->crit), txt, &(data->name));
}

bool set_IPv4_address(worker_data* data, const uint8_t* txt) {
    return update_string(&(data->crit), txt, &(data->ip));
}

bool set_network_key(worker_data* data, const uint8_t* key, size_t key_len) {
    uint8_t* key_cpy = malloc(key_len);
    uint8_t* key_old = NULL;
    if(key_cpy == NULL) {
        return false;
    }
    memcpy(key_cpy, key, key_len);
    EnterCriticalSection(&(data->crit));
    key_old       = data->key;
    data->key     = key_cpy;
    data->key_len = key_len;
    LeaveCriticalSection(&(data->crit));
    free(key_old);
    return true;
}

bool set_profile_id(worker_data* data, const uint8_t* txt) {
    return update_string(&(data->crit), txt, &(data->profile));
}

bool set_status_text(worker_data* data, const uint8_t* txt) {
    return update_string(&(data->crit), txt, &(data->status));
}

worker_data* create_worker_data() {
    CRITICAL_SECTION crit;
    if(!InitializeCriticalSectionAndSpinCount(&crit, 512)) {
        return NULL;
    }
    worker_data* tmp = malloc(sizeof(worker_data));
    if(tmp == NULL) {
        DeleteCriticalSection(&crit);
        return NULL;
    }
    memset(tmp, 0, sizeof(worker_data));
    tmp->crit = crit;
    return tmp;
}

void free_worker_data(worker_data* data) {
    DeleteCriticalSection(&(data->crit));
    free(data->status);
    free(data->profile);
    free(data->name);
    free(data->ip);
    free(data->key);
    free(data);
}