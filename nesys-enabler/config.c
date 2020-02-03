
#include "config.h"

#include <stdio.h>
#include <windows.h>

#define file ".\\NesysConfig.ini"
#define app  "NesysConfig"


void get_access_code(char* code, size_t capacity) {
    GetPrivateProfileStringA(app, "AccessCode", NULL, code, capacity, file);
}

void save_access_code(const char* code) {
    WritePrivateProfileStringA(app, "AccessCode", code, file);
}

void get_server_ip(char* ip, size_t capacity) {
    GetPrivateProfileStringA(app, "serverIP", "192.168.1.205", ip, capacity, file);
}

void save_server_ip(char* ip) {
    WritePrivateProfileStringA(app, "serverIP", ip, file);
}

void get_exe_name(char* name, size_t capacity) {
    GetPrivateProfileStringA(app, "exeName", "game1.exe", name, capacity, file);
}

void set_exe_name(const char* code) {
    WritePrivateProfileStringA(app, "exeName", code, file);
}


uint32_t get_drives() {
    uint32_t v = GetPrivateProfileInt(app, "drives", 0, file) & 0x3ffffff;
    return v;
}

void save_drives(uint32_t drives) {
    char buf[11] = { 0 };
    snprintf(buf, 10, "%u", drives & 0x3ffffff);
    WritePrivateProfileStringA(app, "drives", buf, file);
}

uint16_t get_port() {
    uint16_t v = GetPrivateProfileInt(app, "port", 8764, file);
    return v;
}

void save_port(uint16_t port) {
    char buf[11] = { 0 };
    snprintf(buf, 10, "%u", port);
    WritePrivateProfileStringA(app, "port", buf, file);
}

void write_defaults_to_disk() {
    char buffer[256] = { 0 };

    get_access_code(buffer, 255);
    save_access_code(buffer);

    get_server_ip(buffer, 255);
    save_server_ip(buffer);

    get_exe_name(buffer, 255);
    set_exe_name(buffer);

    save_drives(get_drives());
    save_port(get_port());
}