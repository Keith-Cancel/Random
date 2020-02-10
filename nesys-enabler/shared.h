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
#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <stdint.h>

typedef struct shared_data_s shared_data;

shared_data* create_shared_data();
void         free_shared_data(shared_data* data);

int32_t  get_cur_drives(shared_data* data);
uint8_t* get_profile_id(shared_data* data);
uint8_t* get_status_text(shared_data* data);

bool set_cur_drives(shared_data* data, uint32_t drives);
bool set_profile_id(shared_data* data, const uint8_t* txt);
bool set_status_text(shared_data* data, const uint8_t* txt);

#endif /* SHARED_H */