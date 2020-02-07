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
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

const char header[] = { 'B', 'G', '0', '4' };

int64_t get_id(const char* file_path) {
    int64_t id = -1;
    FILE*   fp = fopen(file_path, "r");

    if(fp == NULL) {
        return -1;
    }
    for(int i = 0; i < 4; i++) {
        if(fgetc(fp) != header[i]) {
            goto close;
        }
    }
    char str[13] = { 0 };
    for(int i = 0; i < 12; i++) {
        int hex = fgetc(fp);
        if(isxdigit(hex) == 0) {
            goto close;
        }
        str[i] = hex;
    }
    id = strtoll(str, NULL, 16);
    close:
        fclose(fp);
        return id;
}

int save_id(const char* file_path, uint64_t id) {
    FILE*   fp = fopen(file_path, "w+");
    if(fp == NULL) {
        return -1;
    }
    for(int i = 0; i < 4; i++) {
        if(fputc(header[i], fp) == EOF) {
            fclose(fp);
            return -1;
        }
    }
    char buffer[13] = { 0 };
    snprintf(buffer, 13, "%012"PRIX64"", id);
    for(int i = 0; i < 12; i++) {
        if(fputc(buffer[i], fp) == EOF) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}


