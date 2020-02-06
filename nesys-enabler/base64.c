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
#include "base64.h"

#define FULL_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
// Clang builtin check
#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

#if FULL_GCC_VERSION > 402 || __has_builtin(__builtin_bswap32)
#define byte_swap32 __builtin_bswap32
#else
#define ROTL32(x, a)   (((x) << (a)) | ((x) >> (32 - (a))))
static uint32_t byte_swap32(uint32_t value) {
    return (ROTR32(value, 8) & 0xff00ff00) |
           (ROTL32(value, 8) & 0x00ff00ff);
}
#endif

static inline bool is_litte_endian(void) {
    union {
        uint32_t v;
        uint8_t  b[4];
    } chk = {0x01000000};
    return chk.b[0] != 1;
}

size_t get_base64_length(size_t length) {
    size_t x   = length * 4;
    size_t j   = x / 3 + ((x % 3) > 0);
    size_t pad = (-j) & 0x3;
    return j + pad;
}

bool to_base64(const uint8_t* data, size_t len, uint8_t* buf, size_t buf_len) {
    if(buf_len < get_base64_length(len)) {
        return false;
    }
    to_base64_no_buffer_check(data, len, buf);
    return true;
}

void to_base64_no_buffer_check(const uint8_t* data, size_t len, uint8_t* buf) {
    static const char tbl[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/'
    };
    size_t in_index  = 0;
    size_t out_index = 0;
    while(in_index <= (len - 4)) {
    static const char tbl[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
        'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
        'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
        'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
        'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
        'w', 'x', 'y', 'z', '0', '1', '2', '3',
        '4', '5', '6', '7', '8', '9', '+', '/'
    };
    size_t in_index  = 0;
    size_t out_index = 0;
    while(in_index <= (len - 4)) {
        uint32_t val;
        memcpy(&val, data + in_index, 4);
        val = is_litte_endian() ? byte_swap32(val) : val;
        // Breaking this up as consts some reason
        // makes clang spill more registers
        /*const uint8_t i_0 = (val >>  8) & 0x3f;
        const uint8_t i_1 = (val >> 14) & 0x3f;
        const uint8_t i_2 = (val >> 20) & 0x3f;
        const uint8_t i_3 = (val >> 26);
        uint32_t out;
        out  = tbl[i_0] << (is_litte_endian() ? 24 : 0);
        out |= tbl[i_1] << (is_litte_endian() ? 16 : 8);
        out |= tbl[i_2] << (is_litte_endian() ? 8 : 16);
        out |= tbl[i_3] << (is_litte_endian() ? 0 : 24);*/
        uint32_t out;
        out  = tbl[(val >>  8) & 0x3f] << (is_litte_endian() ? 24 : 0);
        out |= tbl[(val >> 14) & 0x3f] << (is_litte_endian() ? 16 : 8);
        out |= tbl[(val >> 20) & 0x3f] << (is_litte_endian() ? 8 : 16);
        out |= tbl[(val >> 26)] << (is_litte_endian() ? 0 : 24);
        memcpy(buf + out_index, &out, 4);
        in_index  += 3;
        out_index += 4;
    }
    // Handle the last 1-3 bytes without branches XD
    // Did more for fun than anything
    buf  += out_index;
    data += in_index;
    const unsigned rem   = len - in_index;
    const unsigned rem_s = rem >> 1;
    uint32_t bits = data[rem - 1] * (rem & rem_s);
    bits |= ((uint32_t)data[rem_s] * rem_s) << 8;
    bits |= ((uint32_t)data[0])             << 16;
    buf[3]        = '=';
    buf[2]        = '=';
    buf[rem]      = tbl[ bits & 0x3f];
    buf[rem_s + 1]= tbl[(bits >>  6) & 0x3f];
    buf[1]        = tbl[(bits >> 12) & 0x3f];
    buf[0]        = tbl[ bits >> 18];
}