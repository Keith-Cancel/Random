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
#include "shared.h"
#include <windows.h>
#include <string.h>

 struct shared_data_s {
    HANDLE   mutex;
    uint8_t* status;
    uint8_t* profile;
    int32_t  drives;
};

static bool update_string(HANDLE mutex, const uint8_t* src, uint8_t** dest) {
    size_t len       = strlen((char*)src);
    uint8_t* src_cpy = malloc(len + 1);
    uint8_t* dst_old = NULL;
    if(src_cpy == NULL) {
        return false;
    }
    memcpy(src_cpy, src, len);
    src_cpy[len] = '\0';
    if(WaitForSingleObject(mutex, 100) != WAIT_OBJECT_0) {
        free(src_cpy);
        return false;
    }
    dst_old = *dest;
    *dest   = src_cpy;
    ReleaseMutex(mutex);
    free(dst_old);
    return true;
}

static uint8_t* get_string(HANDLE mutex, uint8_t** str) {
    uint8_t* tmp = NULL;
    if(WaitForSingleObject(mutex, 100) != WAIT_OBJECT_0) {
        return NULL;
    }
    tmp  = *str;
    *str = NULL;
    ReleaseMutex(mutex);
    return tmp;
}

int32_t get_cur_drives(shared_data* data) {
    int32_t tmp = 0;
    if(WaitForSingleObject(data->mutex, 100) != WAIT_OBJECT_0) {
        return -1;
    }
    tmp = data->drives;
    ReleaseMutex(data->mutex);
    return tmp;
}

uint8_t* get_profile_id(shared_data* data) {
    return get_string(data->mutex, &(data->profile));
}

uint8_t* get_status_text(shared_data* data) {
    return get_string(data->mutex, &(data->status));
}

bool set_cur_drives(shared_data* data, uint32_t drives) {
    if(WaitForSingleObject(data->mutex, 100) != WAIT_OBJECT_0) {
        return false;
    }
    data->drives = drives;
    ReleaseMutex(data->mutex);
    return true;
}

bool set_profile_id(shared_data* data, const uint8_t* txt) {
    return update_string(data->mutex, txt, &(data->profile));
}

bool set_status_text(shared_data* data, const uint8_t* txt) {
    return update_string(data->mutex, txt, &(data->status));
}

shared_data* create_shared_data() {
    HANDLE mtx = CreateMutexW(NULL, FALSE, NULL);
    if(mtx == NULL) {
        return NULL;
    }
    shared_data* tmp = malloc(sizeof(shared_data));
    if(tmp == NULL) {
        CloseHandle(mtx);
        return NULL;
    }
    tmp->mutex   = mtx;
    tmp->profile = NULL;
    tmp->status  = NULL;
    tmp->drives  = 0;
    return tmp;
}

void free_shared_data(shared_data* data) {
    CloseHandle(data->mutex);
    free(data);
}