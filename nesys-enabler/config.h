#ifndef CONFIG_H
#define CONFIG_H


#include <stdint.h>
#include <stdbool.h>

// Not really checking error conditions atm
// just assuming these work.
uint32_t get_drives();
void     save_drives(uint32_t drives);

void     get_access_code(char* code, size_t capacity);
void     save_access_code(const char* code);

void     get_server_ip(char* ip, size_t capacity);
void     save_server_ip(char* ip);

void     get_exe_name(char* name, size_t capacity);
void     set_exe_name(const char* code);

uint16_t get_port();
void     save_port(uint16_t port);

void     write_defaults_to_disk();

#endif /* CONFIG_H */
