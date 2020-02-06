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
#include <string.h>

size_t to_base64_no_length_check(const uint8_t* data, size_t len, uint8_t* buf);

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

size_t to_base64(const uint8_t* data, size_t len, uint8_t* buf, size_t buf_len) {
    if(buf_len <= get_base64_length(len)) {
        return SIZE_MAX;
    }
    if(len == 0) {
        return 0;
    }
    return to_base64_no_length_check(data, len, buf);
}

// Handles lengths [1-len] assumes buffer is large enough
size_t to_base64_no_length_check(const uint8_t* data, size_t len, uint8_t* buf) {
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
    while((in_index + 4) <= len) {
        uint32_t val;
        memcpy(&val, data + in_index, 4);
        val = is_litte_endian() ? byte_swap32(val) : val;
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
    return out_index + 4;
}

static bool is_valid(uint8_t val) {
    return ((val >= 'a') & (val <= 'z')) |
           ((val >= 'A') & (val <= 'Z')) |
           ((val >= '/') & (val <= '9')) |
           (val == '+') | (val == '=');
}

static unsigned get_val(uint8_t val) {
    static const uint8_t tbl[] = {
        0x3e, 0x00, 0x00, 0x00, 0x3f, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
        0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33
    };
    return tbl[val - '+'];
    /*val -= 'A';
    val -= (val >> 5) * 6;
    val -= (val >> 7) * 145;
    val -=
    return val;*/
}

size_t from_base64(const uint8_t* data, size_t len, uint8_t* buf, size_t buf_len) {
    if(len > 2) { // adjust needed len when there is padding
        len -= data[len - 1] == '=';
        len -= data[len - 1] == '=';
    }
    size_t needed_len = (len * 3) >> 2;
    if(buf_len < needed_len) { // buffer too small
        return SIZE_MAX;
    }
    for(size_t i = 0; i < len; i += 1) {
        if(!is_valid(data[i])) {
            return SIZE_MAX;
        }
    }
    size_t out_i  = 0;
    unsigned bits = 0;
    for(size_t i = 0; i < len; i += 1) {
        const unsigned extract = ((i >> 1) | i) & 1;
        const unsigned shift   = (i & 0x3) << 1;
        const unsigned value   = get_val(data[i]);
        bits <<= shift;
        bits  |= value >> (shift ^ 6);
        buf[out_i] = bits;
        bits     >>= (extract << 3);
        bits       = value;
        out_i += extract;
    }
    return out_i;
}