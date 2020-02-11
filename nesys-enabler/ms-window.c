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
#include <windows.h>
#include "ms-window.h"

// Links to -lgdi32

#define SHOW_MALLOC_ERR()  (display_custom_error("malloc() failed to allocate memory!"))
#define SHOW_REALLOC_ERR() (display_custom_error("realloc() failed to allocate memory!"))

typedef enum {
    UNKNOWN   = 0,
	TEXT      = 1,
    GROUP_BOX = 2,
    CHECK_BOX = 3
} msCtrlType;


typedef struct childCtrl_s {
    HWND       hWnd;
    msCtrlType type;
    void (*call_bk)(msWindow* win, msCtrl ctrl, msCtrlEvent event);
    uint16_t   height;
    uint16_t   width;
} childCtrl;

struct ms_window_s {
	HWND       hWnd;
    uint8_t*   name;
    childCtrl* children;
    int32_t    child_cnt;
    int32_t    child_cap;
};

// Static functions
static void display_last_error() {
    char buff[512] = { 0 };
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buff, 511, NULL
    );
    MessageBoxA(NULL, buff, "ERROR", MB_ICONERROR | MB_OK);
}

static void display_custom_error(const char* msg) {
    MessageBoxA(NULL, msg, "ERROR", MB_ICONERROR | MB_OK);
}

static unsigned get_height(RECT r) {
    return r.bottom - r.top;
}

static unsigned get_width(RECT r) {
    return r.right - r.left;
}

static bool expand_children_if_full(msWindow* win) {
    if(win->child_cnt < win->child_cap) {
        return true;
    }
    size_t new_cap = win->child_cap + 16;
    childCtrl* tmp = realloc(win->children, sizeof(childCtrl) * new_cap);
    if(tmp == NULL) {
        return false;
    }
    win->child_cap = new_cap;
    win->children  = tmp;
    return true;
}

static void resize_window(HWND hWnd, unsigned width, unsigned height) {
    SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
}

static msCtrl add_child(msWindow* win, HWND child, msCtrlType type, uint16_t base_width, uint16_t base_height) {
    if(expand_children_if_full(win) == false) {
        return -1;
    }
    msCtrl id                 = win->child_cnt;
    win->children[id].hWnd    = child;
    win->children[id].type    = type;
    win->children[id].width   = base_width;
    win->children[id].height  = base_height;
    win->children[id].call_bk = NULL;
    win->child_cnt++;
    return id;
}

static void size_to_text(HWND hWnd, const char* text, int x_bias, int y_bias) {
    HFONT font = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
    HDC hdc = GetDC(hWnd); // get the device context
    if(hdc == NULL) {
        return; // Err kinda hard to anything without a device context
    }
    if(font == NULL) { // No font just assume the default.
        font = GetStockObject(DEFAULT_GUI_FONT);
    }
    SelectObject(hdc, font); // Make sure the DC has our font.
    // GetTextExtentPoint32()
    RECT   r;
    size_t len = strlen(text);
    int height = DrawText(hdc, text, len, &r, DT_CALCRECT);
    int width = r.right - r.left;
    resize_window(hWnd, width + x_bias, height + y_bias);
    ReleaseDC(hWnd, hdc); // free the DC
}

// Non Static functions
LRESULT handle_cmd(msWindow* win, HWND hWnd, WORD notify, WORD id) {
    if(id < 1) {
        return 1;
    }
    msCtrl ctrl = id - 1;
    if(win->children[ctrl].hWnd == NULL) {
        return 1;
    }
    if(win->children[ctrl].call_bk == NULL) {
        return 1;
    }
    char buff[256] = { 0 };
    GetClassName(win->children[ctrl].hWnd, buff,255);
    // Handle Button Class
    if(strcmp("Button", buff) == 0) {
        msCtrlEvent event = NOTIFIED;
        switch(notify) {
            case BN_DBLCLK:
                event = DOUBLE_CLK;
                break;
            case BN_CLICKED:
                event = SINGLE_CLK;
                break;
            default:
                return 1;
        }
        win->children[ctrl].call_bk(win, ctrl, event);
        return 0;
    }
    return 1;
}

LRESULT CALLBACK window_events(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HBRUSH     win_bg_brush        = NULL;
static HWND       timer_window        = NULL;
static const char window_class_name[] = "windows_60528c542f"; // Class name for default window
static WNDCLASS   window_class = {
    .lpfnWndProc   = window_events,
    .lpszClassName = window_class_name,
    .cbWndExtra    = sizeof(LONG_PTR), // Reserve space for a pointer in this window.
    .style         = CS_VREDRAW | CS_HREDRAW | CS_OWNDC
};
// Event Handler
LRESULT CALLBACK window_events(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if(hWnd == NULL || hWnd == timer_window) {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    msWindow* win = (msWindow*)GetWindowLongPtr(hWnd, 0);
    switch(msg) {
        case WM_DESTROY:
            // App is only using one windows for now so this is fine.
            PostQuitMessage(0);
            break;
        case WM_CTLCOLORSTATIC:
            return (LRESULT)win_bg_brush;
        case WM_COMMAND:
            if(handle_cmd(win, (HWND)lParam, HIWORD(wParam), LOWORD(wParam)) == 0) {
                return 0;
            }
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

bool init_ms_window() {
    HMODULE main = GetModuleHandle(NULL);
    win_bg_brush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    if(win_bg_brush == NULL) {
        display_last_error();
        return false;
    }
    // Initilize default window class details
    window_class.hCursor       = LoadCursor(NULL, IDC_ARROW); // We don't want a throbber.
	window_class.hInstance     = GetModuleHandle(NULL);// Just use current thread.
	//win->class.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    window_class.hbrBackground = win_bg_brush;
    // Add icon currently hardcoded to 101
    window_class.hIcon         = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));
    if(RegisterClass(&window_class) == 0) { // Register our window class
        display_last_error();
		DeleteObject(win_bg_brush);
        return false;
	}
    // A bit of a hack since GetMessage does not have a time out, but I can
    // attach a timer event to this window causing GetMessage() to return
    // for that time event. Using get message means lower CPU usage compared
    // to PeekMessage().
    timer_window = CreateWindowA(
        window_class_name, "NONE", 0, 0, 0, 40, 40, 0, 0, main, NULL
    );
    if(timer_window == NULL) {
        display_last_error();
		DeleteObject(win_bg_brush);
        UnregisterClass(window_class_name, main);
        return false;
    }
    return true;
}

void cleanup_ms_window() {
    DestroyWindow(timer_window);
    DeleteObject(win_bg_brush);
    UnregisterClass(window_class_name, GetModuleHandle(NULL));
}

void annihilate_window(msWindow* win) {
    for(size_t i = 0; i < win->child_cnt; i++) {
        HWND child = win->children[i].hWnd;
        if(child != NULL) {
            DestroyWindow(child);
        }
    }
    DestroyWindow(win->hWnd);
    free(win->name);
    free(win->children);
	free(win);
}

// Handles all the boiler plate for window creation
msWindow* construct_window(const char *name, unsigned x, unsigned y, bool scale) {
    msWindow* win = malloc(sizeof(msWindow));
	if(win == NULL) {
        SHOW_MALLOC_ERR();
		goto exit;
    }
    memset(win, 0, sizeof(msWindow));
    win->child_cnt = 0;
    win->child_cap = 16;
    win->children  = malloc(sizeof(childCtrl) * 16);
    if(win->children == NULL) {
        SHOW_MALLOC_ERR();
        goto free_window;
    }
    size_t name_len = strlen(name);
    win->name       = malloc(name_len + 1);
    if(win->name == NULL) {
        SHOW_MALLOC_ERR();
        goto free_child;
    }
    // Copy name over.
    memset(win->name, 0, name_len + 1);
    memcpy(win->name, name, name_len);
    // Now Lets get onto creating a window centered in the screen
    RECT rect           = { 0, 0, x, y };
	DWORD style         =  WS_SYSMENU | WS_MINIMIZEBOX | WS_CAPTION | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
	int calc_x          = 0;
	int calc_y          = 0;
	int pos_x           = 0;
	int pos_y           = 0;
    style |= (scale) ? WS_OVERLAPPEDWINDOW : 0;
    // Calculate the window area excluding border and caption ect...
    AdjustWindowRect(&rect, style, false);
    // get width and height
    calc_x = rect.right - rect.left;
	calc_y = rect.bottom - rect.top;
	// calclate the posistion centered
	pos_x  = (GetSystemMetrics(SM_CXSCREEN) - calc_x);
	pos_x  = (pos_x < 0) ? 0 : pos_x/2;
	pos_y  = (GetSystemMetrics(SM_CYSCREEN) - calc_y);
	pos_y  = (pos_y < 0) ? 0 : pos_y/2;
    // Make the window
    win->hWnd = CreateWindow(
		window_class_name, (char*)win->name,
		style,
		pos_x,  pos_y,
		calc_x, calc_y,
		0, 0,
		GetModuleHandle(NULL),
		NULL
	);
    // failed to make the class
    if(!win->hWnd) {
        display_last_error();
		goto free_name;
    }
    // Add window struct pointer to the window, so we cann access it form the
    // event loop.
	SetWindowLongPtr(win->hWnd, 0, (LONG_PTR)win);
    return win;
    // Clean up code.
    free_name:
        free(win->name);
    free_child:
        free(win->children);
	free_window:
		free(win);
	exit:
		return NULL;
}

// Functions for controls

void ctrl_set_callback(msWindow* win, msCtrl ctrl, void (*call_bk)(msWindow* win, msCtrl ctrl, msCtrlEvent event)) {
    win->children[ctrl].call_bk = call_bk;
}

void ctrl_disable(msWindow* win, msCtrl ctrl) {
    HWND hWnd = win->children[ctrl].hWnd;
    EnableWindow(hWnd, FALSE);
}

void ctrl_enable(msWindow* win, msCtrl ctrl) {
    HWND hWnd = win->children[ctrl].hWnd;
    EnableWindow(hWnd, TRUE);
}

void ctrl_center_vert(msWindow* win, msCtrl ctrl) {
    HWND hWnd = win->children[ctrl].hWnd;
    HWND par = GetParent(hWnd);
    if(par == NULL) {
        return;
    }
    RECT p_rec;
    RECT c_rec;
    GetWindowRect(par,  &p_rec);
    GetWindowRect(hWnd, &c_rec);
    MapWindowPoints(HWND_DESKTOP, par, (LPPOINT)&c_rec, 2);
	// calclate the posistion centered
    unsigned pos_x = 0, w = get_width(c_rec), h = get_height(c_rec);
	pos_x  = (get_width(p_rec) - w);
	pos_x  = (pos_x < 0) ? 0 : pos_x / 2;
    MoveWindow(hWnd, pos_x, c_rec.top, w, h, TRUE);
}

void ctrl_set_text(msWindow* win, msCtrl ctrl, const char* text, bool fit) {
    HWND hWnd  = win->children[ctrl].hWnd;
    uint16_t h = win->children[ctrl].height;
    uint16_t w = win->children[ctrl].width;
    if(hWnd == NULL) {
        return;
    }
    size_t buf_sz = strlen(text) + 3;
    char buf[buf_sz];
    memset(buf, 0, buf_sz);
    GetWindowTextA(hWnd, buf, buf_sz);
    // Old string and current are the same don't update anything.
    if(strcmp(text, buf) == 0) {
        return;
    }
    // Reize then set text this stops reduces the number of repaints needed.
    if(fit) {
        size_to_text(hWnd, text, w, h);
    }
    SetWindowTextA(hWnd, text);
}

void ctrl_move(msWindow* win, msCtrl ctrl, unsigned x, unsigned y) {
    HWND hWnd = win->children[ctrl].hWnd;
    RECT c_rec;
    GetWindowRect(hWnd, &c_rec);
    MoveWindow(hWnd, x, y, get_width(c_rec), get_height(c_rec), TRUE);
}

void ctrl_resize(msWindow* win, msCtrl ctrl, unsigned width, unsigned height) {
    if(win->children[ctrl].hWnd == NULL) {
        return;
    }
    resize_window(win->children[ctrl].hWnd, width, height);
}

unsigned ctrl_get_width(msWindow* win, msCtrl ctrl) {
    HWND hWnd = win->children[ctrl].hWnd;
    RECT c_rec;
    GetWindowRect(hWnd, &c_rec);
    return get_width(c_rec);
}

unsigned ctrl_get_height(msWindow* win, msCtrl ctrl) {
    HWND hWnd = win->children[ctrl].hWnd;
    RECT c_rec;
    GetWindowRect(hWnd, &c_rec);
    return get_height(c_rec);
}

msCtrl ctrl_create_groupbox(msWindow* win, const char* text, unsigned x, unsigned y, textAlign align) {
    DWORD style = WS_VISIBLE | WS_CHILD | BS_GROUPBOX;
    style |= (align == LEFT) ? BS_LEFT : (align == CENTER) ? BS_CENTER : BS_RIGHT;
    HWND hWnd = CreateWindowA(
        "BUTTON", text, style,
        x, y, 100, 100, win->hWnd, 0,
        GetModuleHandle(NULL), NULL
    );
    if(hWnd == NULL) {
        return -1;
    }
    PostMessage(hWnd, WM_SETFONT, (LPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
    msCtrl id = add_child(win, hWnd, GROUP_BOX, 0, 0);
    if(id < 0) {
        DestroyWindow(hWnd);
    }
    return id;
}

msCtrl ctrl_create_text(msWindow* win, const char* text, unsigned x, unsigned y, textAlign align) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_GROUP;
    style |= (align == LEFT) ? SS_LEFT : (align == CENTER) ? SS_CENTER : SS_RIGHT;
    HWND text_ctrl = CreateWindowA(
        "STATIC", text, style, x, y, 10, 10,
        win->hWnd, 0, GetModuleHandle(NULL), NULL
    );
    if(text_ctrl == NULL) {
        return -1;
    }
    PostMessage(text_ctrl, WM_SETFONT, (LPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
    //SendMessage(text_ctrl, WM_SETFONT, (LPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
    size_to_text(text_ctrl, text, 0, 0);
    msCtrl id = add_child(win, text_ctrl, TEXT, 0, 0);
    if(id < 0) {
        DestroyWindow(text_ctrl);
    }
    return id;
}

// Check Box stuff

msCtrl ctrl_create_checkbox(msWindow* win, const char* text, unsigned x, unsigned y) {
    // GetSystemMetrics is does include the padding/gap between the box and
    // the text which from testing apears to be 4 pixels
    uint16_t base_sz = GetSystemMetrics(SM_CXMENUCHECK); // Get system check box size
    HWND hWnd = CreateWindowA(
        "BUTTON", text,
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
        x, y, base_sz, base_sz, win->hWnd, (HMENU)(intptr_t)(win->child_cnt + 1),
        GetModuleHandle(NULL), NULL
    );
    if(hWnd == NULL) {
        return -1;
    }
    PostMessage(hWnd, WM_SETFONT, (LPARAM)GetStockObject(DEFAULT_GUI_FONT), true);
    size_to_text(hWnd, text, base_sz + 4, 0);
    msCtrl id = add_child(win, hWnd, CHECK_BOX, base_sz + 4, 0);
    if(id < 0) {
        DestroyWindow(hWnd);
    }
    return id;
    //CheckDlgButton(hwnd, 1, BST_CHECKED);
}

int ctrl_checkbox_get_state(msWindow* win, msCtrl ctrl) {
    if(win->children[ctrl].type != CHECK_BOX) {
        return -2;
    }
    HWND hWnd = win->children[ctrl].hWnd;
    LRESULT val = SendMessage(hWnd, BM_GETCHECK, 0, 0);
    if(val == BST_CHECKED) {
        return 1;
    }
    if(val == BST_INDETERMINATE) {
        return -1;
    }
    return 0;
}

void ctrl_checkbox_set_state(msWindow* win, msCtrl ctrl, int state) {
    if(win->children[ctrl].type != CHECK_BOX) {
        return;
    }
    HWND hWnd = win->children[ctrl].hWnd;
    switch(state) {
        case -1:
            SendMessage(hWnd, BM_SETCHECK, BST_INDETERMINATE, 0);
            break;
        case 1:
            SendMessage(hWnd, BM_SETCHECK, BST_CHECKED, 0);
            break;
        default:
            SendMessage(hWnd, BM_SETCHECK, BST_UNCHECKED, 0);
            break;
    }
}

winResult poll_windows() {
	MSG msg;
	BOOL ret = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
    if(
        ret == 0 || // No Messages
        (msg.hwnd == timer_window && msg.message == WM_TIMER) // Just eat these
    ) { // No messages
        return WIN_CONTINE;
    }
	if(msg.message == WM_QUIT) {
		return WIN_EXIT;
	}
	TranslateMessage(&msg);
	DispatchMessageW(&msg);
	return WIN_CONTINE;
}

winResult wait_windows() {
    MSG msg;
    BOOL ret = GetMessageW(&msg, NULL, 0, 0);
    // I would not call a 3 state return a BOOL, but okay microsoft.
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessage#return-value
    if(ret < 0) {
        return WIN_ERROR_GENERAL;
    }
    if(msg.message == WM_QUIT) {
		return WIN_EXIT;
	}
    if(msg.hwnd == timer_window && msg.message == WM_TIMER) {
        return WIN_TIMEOUT;
    }
    TranslateMessage(&msg);
	DispatchMessageW(&msg);
    return WIN_CONTINE;
}

void set_wait_timeout(int timeout) {
    static const UINT_PTR id = 0x82131a7f;
    if(timeout < 0) {
        KillTimer(timer_window, id);
    } else {
        SetTimer(timer_window, id, timeout, NULL);
    }
}