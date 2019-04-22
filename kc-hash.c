/*
MIT License

Copyright (c) 2019 Keith J. Cancel

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

#include <string.h>
#include <stdint.h>

#define RR64(x, r) ((x >> r) | (x << (64 - r)))
#define RL64(x, r) ((x << r) | (x >> (64 - r)))
#define MIX(x, y, k) (x = RL64(x, 11), x += y, x ^= k, y = RR64(y, 5), y ^= x)

// NOTE - This is not a cryptographic hash function.
// This simple hash function can hash files pretty quickly. Each operation is
// working on 8 bytes at a time. 
void kc_hash(const void *data, unsigned length,  uint64_t *upper, uint64_t *lower) {
    // Old constants. Just random numbers that seemed to work well.
    //uint64_t msb   = 0x7227fa27d388d60a;
    //uint64_t lsb   = 0x613f5204a37cb723;

    // Liked the idea from BLAKE2 Initialization vectors.
    uint64_t msb   = 0x6a09e667f3bcc908;  // frac(sqrt(2))
    uint64_t lsb   = 0xbb67ae8584caa73b;  // frac(sqrt(3))
    uint64_t chunk = 0;

    const uint64_t *data_w = (const uint64_t*)data;
    while(length >= 16) {
		msb ^= data_w[0];
        lsb ^= data_w[1];
        MIX(msb, lsb, chunk);
        chunk = RL64(chunk, 3);
        data_w += 2;
		length -= 16;
        chunk++;
	}
    // Handle remaining bytes
    uint64_t remnant[2] = {0, 0};
    memcpy(remnant, data_w, length);
    msb ^= remnant[0];
    lsb ^= remnant[1];
    // One last final mix mainly to ensure a decent propagation
    // of the remnant and recent chunks.
    for(int i = 0; i < 8; i++) {
        MIX(msb, lsb, chunk+i);
    }
    *upper = msb;
    *lower = lsb;
}