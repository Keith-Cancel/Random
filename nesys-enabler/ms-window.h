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
#ifndef MS_WINDOW_H
#define MS_WINDOW_H

#include <stdint.h>
#include <stdbool.h>

typedef struct ms_window_s msWindow;
typedef int16_t msCtrl;

typedef enum winResult {
    WIN_ERROR_GENERAL = -1,
    WIN_CONTINE       = 0,
    WIN_TIMEOUT       = 1,
	WIN_EXIT          = 2
} winResult;

typedef enum msCtrlEvent {
    NOTIFIED   = 0,
    SINGLE_CLK = 1,
    DOUBLE_CLK = 2
} msCtrlEvent;

typedef enum textAlign {
    LEFT   = 0,
    CENTER = 1,
    RIGHT  = 2
} textAlign;


void      annihilate_window(msWindow* win);
msWindow* construct_window(const char *name, unsigned x, unsigned y, bool scale);
winResult poll_windows();
void      set_wait_timeout(int timeout);
winResult wait_windows();
bool      init_ms_window();
void      cleanup_ms_window();

void     ctrl_enable(msWindow* win, msCtrl ctrl);
void     ctrl_disable(msWindow* win, msCtrl ctrl);
void     ctrl_set_callback(msWindow* win, msCtrl ctrl, void (*call_bk)(msWindow* win, msCtrl ctrl, msCtrlEvent event));
void     ctrl_center_vert(msWindow* win, msCtrl ctrl);
void     ctrl_set_text(msWindow* win, msCtrl ctrl, const char* text, bool fit);
void     ctrl_move(msWindow* win, msCtrl ctrl, unsigned x, unsigned y);
void     ctrl_resize(msWindow* win, msCtrl ctrl, unsigned width, unsigned height);
unsigned ctrl_get_height(msWindow* win, msCtrl ctrl);
unsigned ctrl_get_width(msWindow* win, msCtrl ctrl);

msCtrl ctrl_create_groupbox(msWindow* win, const char* text, unsigned x, unsigned y, textAlign align);
msCtrl ctrl_create_text(msWindow* win, const char* text, unsigned x, unsigned y, textAlign align);

msCtrl ctrl_create_checkbox(msWindow* win, const char* text, unsigned x, unsigned y);
int    ctrl_checkbox_get_state(msWindow* win, msCtrl ctrl);
void   ctrl_checkbox_set_state(msWindow* win, msCtrl ctrl, int state);

#endif /* MS_WINDOW_H */

