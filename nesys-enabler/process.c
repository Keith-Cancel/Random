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
#include "process.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <wchar.h>

HANDLE get_process_handle(const char* name) {
    PROCESSENTRY32 entry = { .dwSize = sizeof(PROCESSENTRY32) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(Process32First(snapshot, &entry) != TRUE) {
        CloseHandle(snapshot);
        return NULL;
    }
    HANDLE process = NULL;
    while(Process32Next(snapshot, &entry) == TRUE) {
        if(stricmp(entry.szExeFile, name) == 0) {
            process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
            break;
        }
    }
    CloseHandle(snapshot);
    return process;
}

uintptr_t get_process_base_address(HANDLE process) {
    if(process == NULL) {
        return 0;
    }
    // Get the processes name.
    WCHAR name[512] = { 0 };
    WCHAR* base     = NULL;
    DWORD  words    = GetProcessImageFileNameW(process, name, 512);
    for(int i = words - 1; i >= 0; i--) {
        if(name[i] == 0x5c) {
            base = (name + 1) + i;
            break;
        }
    }
    if(base == NULL) {
        return 0;
    }
    // figure out how many modules there are.
    DWORD needed;
    HMODULE hMod;
    if(EnumProcessModules(process, &hMod, sizeof(hMod), &needed) == 0) {
        return 0;
    }
    size_t   arr_sz = needed;
    HMODULE* mods   = malloc(arr_sz);
    if(mods == NULL) {
        return 0;
    }
    // get all the modules
    if(EnumProcessModules(process, mods, arr_sz, &needed) == 0) {
        free(mods);
        return 0;
    }
    // Check for the module of the process.
    DWORD cnt = needed / sizeof(HMODULE);
    uintptr_t ptr = 0;
    for(DWORD i = 0; i < cnt; i++) {
        WCHAR buf[512] = { 0 };
        if(GetModuleBaseNameW(process, mods[i], buf, 512) == 0) {
            continue;
        }
        if(wcscmp(base, buf) != 0) {
            continue;
        }
        // Not Needed HMOD is the base address as well, okay that makes this easier.
        // https://docs.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-moduleinfo#remarks
        /*MODULEINFO info = { 0 };
        if(GetModuleInformation(process, mods[i], &info, sizeof(info)) == 0) {
            break;
        }*/
        ptr = (uintptr_t)mods[i];
        break;
    }
    free(mods);
    return ptr;
}