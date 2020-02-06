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
        uint8_t val[4];
        memcpy(&val, data + in_index, 4);
        const uint8_t i_0 =   val[0] >> 2;
        const uint8_t i_1 = ((val[0] << 4) | (val[1] >> 4)) & 0x3f;
        const uint8_t i_2 = ((val[1] << 2) | (val[2] >> 6)) & 0x3f;
        const uint8_t i_3 =   val[2] & 0x3f;
        uint8_t out[4] = {
                tbl[i_0], tbl[i_1], tbl[i_2], tbl[i_3]
        };
        memcpy(buf + out_index, &out, 4);
        in_index  += 3;
        out_index += 4;
    }
    // Handle the last 1-3 bytes
    uint32_t bits  = 0;
    unsigned shift = 6;
    for(;in_index < len; in_index++) {
        uint8_t byte = data[in_index];
        bits  |= byte >> (8 - shift);
        buf[out_index] = tbl[bits];
        bits >>= 6;
        // add the remaining bits.
        bits  |= (0xff >> shift) & byte;
        // adjust shift
        shift -= 2;
        bits <<= shift;
        out_index++;
    }
    unsigned x = (~out_index) & 0x3;
    buf[out_index + (x >> 1)] = '=';
    buf[out_index + x]        = '=';
    buf[out_index] = tbl[bits];
}