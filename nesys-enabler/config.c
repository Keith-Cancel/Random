
#include "config.h"

#include <stdio.h>
#include <windows.h>

#define FILE ".\\NesysConfig.ini"
#define APP  "NesysConfig"


void get_access_code(char* code, size_t capacity) {
    GetPrivateProfileStringA(APP, "AccessCode", NULL, code, capacity, FILE);
}

void save_access_code(const char* code) {
    WritePrivateProfileStringA(APP, "AccessCode", code, FILE);
}

void get_server_ip(char* ip, size_t capacity) {
    GetPrivateProfileStringA(APP, "serverIP", "192.168.1.205", ip, capacity, FILE);
}

void save_server_ip(char* ip) {
    WritePrivateProfileStringA(APP, "serverIP", ip, FILE);
}

void get_exe_name(char* name, size_t capacity) {
    GetPrivateProfileStringA(APP, "exeName", "game1.exe", name, capacity, FILE);
}

void set_exe_name(const char* code) {
    WritePrivateProfileStringA(APP, "exeName", code, FILE);
}


uint32_t get_drives() {
    uint32_t v = GetPrivateProfileInt(APP, "drives", 0, FILE) & 0x3ffffff;
    return v;
}

void save_drives(uint32_t drives) {
    char buf[11] = { 0 };
    snprintf(buf, 10, "%u", drives & 0x3ffffff);
    WritePrivateProfileStringA(APP, "drives", buf, FILE);
}

uint16_t get_port() {
    uint16_t v = GetPrivateProfileInt(APP, "port", 8764, FILE);
    return v;
}

void save_port(uint16_t port) {
    char buf[11] = { 0 };
    snprintf(buf, 10, "%u", port);
    WritePrivateProfileStringA(APP, "port", buf, FILE);
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