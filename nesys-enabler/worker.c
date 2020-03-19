

#include <winsock2.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "client.h"
#include "id-files.h"
#include "process.h"
#include "worker.h"

struct worker_data_s {
    CRITICAL_SECTION crit;
    uint8_t* status;
    uint8_t* profile;
    uint8_t* name;
    uint8_t* ip;
    uint8_t* key;
    size_t   key_len;
    int32_t  drives;
    uint16_t port;
    bool     stop;
};

struct worker_s {
    HANDLE       thread;
    worker_data* data;
};

DWORD WINAPI worker_thread_func(void *arg);

static bool set_stop(worker_data* data) {
    EnterCriticalSection(&(data->crit));
    data->stop = true;
    LeaveCriticalSection(&(data->crit));
    return true;
}

static bool get_stop(worker_data* data) {
    EnterCriticalSection(&(data->crit));
    bool tmp = data->stop;
    LeaveCriticalSection(&(data->crit));
    return tmp;
}

worker_thread* worker_create(worker_data* data) {
    if(data == NULL) {
        return NULL;
    }
    worker_thread* worker = malloc(sizeof(worker_thread));
    if(worker == NULL) {
        return NULL;
    }
    worker->data   = data;
    worker->thread = CreateThread(NULL, 0, worker_thread_func, data, CREATE_SUSPENDED, NULL);
    if(worker->thread == NULL) {
        free(worker);
        return NULL;
    }
    return worker;
}

void worker_start(worker_thread* worker) {
    ResumeThread(worker->thread);
}

void worker_cleanup(worker_thread* worker) {
    set_stop(worker->data);
    WaitForSingleObject(worker->thread, INFINITE);
    CloseHandle(worker->thread);
    free(worker);
}

static uint32_t removable_only(uint32_t drives) {
    uint32_t v = 0;
    for(int i = 0; i < 26; i++) {
        v <<= 1;
        char drive[] = "Z:\\";
        drive[0] -= i;
        v |= (GetDriveTypeA(drive) == DRIVE_REMOVABLE);
    }
    return v & drives;
}

static int64_t find_profile_id_on_drives(uint32_t drives) {
    for(int i = 0; i < 26; i++) {
        int save = (drives >> i) & 0x1;
        if(save == 0) {
            continue; // Not selected.
        }
        char drive[] = "A:\\";
        char path[]  = "A:\\nesyskey.txt";
        drive[0] += i;
        path[0]  += i;
        if(GetDriveTypeA(drive) != DRIVE_REMOVABLE) {
            continue;
        }
        int64_t id = get_id(path);
        if(id > -1) {
            return id;
        }
    }
    return -1;
}

static void save_profile_id_on_drives(int32_t drives, uint64_t id) {
    for(int i = 0; i < 26; i++) {
        int save = (drives >> i) & 0x1;
        if(save == 0) {
            continue; // Not selected.
        }
        char drive[] = "A:\\";
        char path[]  = "A:\\nesyskey.txt";
        drive[0] += i;
        path[0]  += i;
        if(GetDriveTypeA(drive) != DRIVE_REMOVABLE) {
            continue;
        }
        save_id(path, id);
    }
}

static requestInfo* get_req_info(worker_data* data) {
    uint8_t* ip       = get_IPv4_address(data);
    uint16_t port     = get_cur_port(data);
    uint8_t* key      = NULL;
    requestInfo* info = NULL;
    size_t key_len    = get_network_key(data, &key);

    if(ip == NULL || strlen((char*)ip) < 1 || port < 1) {
        set_status_text(data, (uint8_t*)"Please set a server IP & Port address in the config file.");
        goto done;
    }

    if(key_len < 1 || key == NULL) {
        set_status_text(data, (uint8_t*)"Please set the access key in the config file.");
        goto done;
    }
    info = request_info_create(key, key_len, ip, port);
    if(info == NULL) {
        set_status_text(data, (uint8_t*)"Unable to allocate the request info object.");
    }
    done:
    free(ip);
    free(key);
    return info;
}

static void set_request_fail_message(worker_data* data, requestState st) {
    switch(st) {
        case FAILURE:
            set_status_text(data, (uint8_t*)"Network failure!");
            break;
        case TIME_OUT:
            set_status_text(data, (uint8_t*)"The server took too long to respond timed out!");
            break;
        case AUTH_FAIL:
            set_status_text(data, (uint8_t*)"The Server Could not authenticate the request!");
            break;
        case AUTH_FAIL_SERVER:
            set_status_text(data, (uint8_t*)"Server failed to authenticate a request. Set the key in the config file.");
            break;
        case SUCCESS:
            break;
        default:
            set_status_text(data, (uint8_t*)"Unknown error when connecting to server!");
            break;
    }
    Sleep(2500);
}

static bool get_profile_id_from_drives(worker_data* data, requestInfo* info, uint8_t* buf, size_t cap, uint32_t drives) {
    int64_t profile_id = find_profile_id_on_drives(drives);

    if(profile_id >= 0) {
        set_status_text(data, (uint8_t*)"Got Profile ID from drives.");
        snprintf((char*)buf, cap - 1, "BG04%012"PRIX64"", profile_id);
        save_profile_id_on_drives(drives, profile_id);
        return true;
    }
    // No profile on drives so get a new on from the server
    set_status_text(data, (uint8_t*)"Requesting new Profile ID from server.");
    requestState ret = get_new_profile_id(info, buf, cap);
    if(ret != SUCCESS) {
        set_request_fail_message(data, ret);
        return false;
    }

    set_status_text(data, (uint8_t*)"Saving Profile ID to drives.");
    profile_id = strtoll((char*)buf + 4, NULL, 16);
    save_profile_id_on_drives(drives, profile_id);
    return true;
}

#define SLEEP_AMT     35
#define MAIN_OFFSET   0x3F25E0

#define VIEW_OFF      118
#define ACTIVE_OFF    140
#define TUNE_TYPE_OFF 186
#define TUNED_OFF     220
#define CAR_NAME_OFF  260

#define DRESSUP_START 185
#define DRESSUP_END   222

static bool process_active(HANDLE proc) {
    DWORD status;
    BOOL ret = GetExitCodeProcess(proc, &status);
    return (ret != 0) && (status == STILL_ACTIVE);
}

static int is_profile_active(HANDLE proc, uintptr_t base, bool* active) {
    LPVOID ptr = (LPVOID)base;
    ptr += MAIN_OFFSET + ACTIVE_OFF;
    uint8_t val = 0;
    if(ReadProcessMemory(proc, ptr, &val, 1, NULL) == 0) {
        return -1;
    }
    *active = val;
    return 0;
}

static int is_tuned(HANDLE proc, uintptr_t base, bool* tuned) {
    LPVOID ptr = (LPVOID)base;
    ptr += MAIN_OFFSET + TUNED_OFF;
    uint8_t val = 0;
    if(ReadProcessMemory(proc, ptr, &val, 1, NULL) == 0) {
        return -1;
    }
    *tuned = val;
    return 0;
}

static int get_car_name(HANDLE proc, uintptr_t base, uint8_t* buf, size_t buf_len) {
    LPVOID ptr = (LPVOID)base;
    ptr += MAIN_OFFSET + CAR_NAME_OFF;
    buf_len = buf_len > 5 ? 5 : buf_len;
    if(ReadProcessMemory(proc, ptr, buf, buf_len, NULL) == 0) {
        return -1;
    }
    return 0;
}

static int read_car_profile(HANDLE proc, uintptr_t base, uint8_t* mem) {
    LPVOID ptr = (LPVOID)base;
    ptr += MAIN_OFFSET;
    if(ReadProcessMemory(proc, ptr, mem, 304, NULL) == 0) {
        return -1;
    }
    mem[TUNED_OFF] = 0;
    return 0;
}

static int write_dress_up(HANDLE proc, uintptr_t base, const uint8_t* mem) {
    LPVOID ptr = (LPVOID)base;
    ptr += MAIN_OFFSET + DRESSUP_START;
    size_t len = DRESSUP_END - DRESSUP_START;
    if(WriteProcessMemory(proc, ptr, mem + MAIN_OFFSET + DRESSUP_START, len, NULL)) {
        return -1;
    }
    return 0;
}

static int update_game(HANDLE proc, const uint8_t* profile, requestInfo* info, worker_data* data) {
    uintptr_t base_addr = get_process_base_address(proc);
    if(base_addr == 0) {
        if(process_active(proc)) {
            set_status_text(data, (uint8_t*)"Can't get base address. (Maybe permissions?)");
            return -1;
        }
        return 0;
    }

    set_status_text(data, (uint8_t*)"Waiting for an active profile.");
    printf("Here 1\n");
    bool active = false;
    do {
        if(get_stop(data)) {
            return 0;
        }
        Sleep(SLEEP_AMT);
        if(is_profile_active(proc, base_addr, &active)) {
            if(process_active(proc)) {
                set_status_text(data, (uint8_t*)"Can't read process memory. (Maybe permissions?)");
                return -1;
            }
            return 0;
        }
    } while(!active);
    printf("Here 2\n");
    uint8_t car_name[6] = { 0 };
    uint8_t buffer[609] = { 0 };
    uint8_t* last = buffer;
    uint8_t* curr = buffer + 304;
    if(get_car_name(proc, base_addr, car_name, 5)) {
        if(process_active(proc)) {
            set_status_text(data, (uint8_t*)"Can't read process memory. (Maybe permissions?)");
            return -1;
        }
        return 0;
    }
    printf("Car Name: %s Name Length: %d\n", car_name, strlen(car_name));
    printf("Here 3\n");
    // Get the current profile from the server.
    size_t written;
    requestState ret = get_car_profile(info, profile, car_name, last, 304, &written);
    while(ret != SUCCESS) {
        set_request_fail_message(data, ret);
        if(ret == AUTH_FAIL) {
            return -1;
        }
        if(get_stop(data)) {
            return 0;
        }
    }
    printf("Here 4\n");
    set_status_text(data, (uint8_t*)"Received profile info from the server!");
    active = false;
    is_profile_active(proc, base_addr, &active); // Check active status again
    if(!active) {
        return 0;
    }
    printf("Here 5\n");
    // if server did not return any thing use the games current data
    if(written == 0) {
        printf("Nothing form server Using game memory\n");
        if(read_car_profile(proc, base_addr, last)) {
            return 0; // Could not read car profile
        }
        // Give server the current game state
        ret = save_car_profile(info, profile, car_name, last, 304);
        // Not gonna worry if this fails
    }
    printf("Current Mem:");
    for(int i = 0; i < 304; i++) {
        if((i % 16) == 0) {
            printf("\n");
        }
        printf("%02x ", last[i]);
    }

    printf("Here 6\n");

    active = false;
    is_profile_active(proc, base_addr, &active); // Check active status again
    if(!active) {
        return 0;
    }
    printf("Here 7\n");
    // Write the game data.
    if(WriteProcessMemory(proc, (LPVOID)base_addr + MAIN_OFFSET, last, 304, NULL) == 0) {
        return 0;
    }
    printf("Here 8\n");

    bool tuned = false;
    set_status_text(data, (uint8_t*)"Waiting to write dress up data!");
    do {
        if(get_stop(data)) {
            printf("Stop requested?\n");
            return 0;
        }
        Sleep(SLEEP_AMT);
        printf("Trying to get is_tuned\n");
        tuned = false;
        if(is_tuned(proc, base_addr, &tuned)) {
            if(process_active(proc)) {
                set_status_text(data, (uint8_t*)"Can't read process memory. (Maybe permissions?)");
                return -1;
            }
            return 0;
        }
        get_car_name(proc, base_addr, car_name, 5);
        is_profile_active(proc, base_addr, &active);
        printf("Is Tuned: %d Active: %d Name: %s\n", tuned, active, car_name);
    } while(!tuned);
    printf("Here 9\n");
    // Now we can update the dress up data.
    if(write_dress_up(proc, base_addr, last)) {
        return 0;
    }
    set_status_text(data, (uint8_t*)"Wrote dressup data.");
    // Now we start sending the server any changes
    do {
        if(get_stop(data)) {
            return 0;
        }
        Sleep(300);
        active = false;
        is_profile_active(proc, base_addr, &active);
        if(active && read_car_profile(proc, base_addr, curr) == 0) {
            if(memcmp(last, curr, 304) != 0) {
                uint8_t* tmp = last;
                last = curr;
                curr = tmp;
                set_status_text(data, (uint8_t*)"Sending changes to server.");
                ret = save_car_profile(info, profile, car_name, last, 304);
                if(ret == SUCCESS) {
                    set_status_text(data, (uint8_t*)"Successfully, sent changes!");
                } else {
                    set_status_text(data, (uint8_t*)"Failed to send changes!");
                }
            } else {
                set_status_text(data, (uint8_t*)"No changes to send to server.");
            }
        }
        get_car_name(proc, base_addr, car_name, 5);
        printf("Active: %d Name: %s\n", active, car_name);
    } while(active);

    return 0;
}

DWORD WINAPI worker_thread_func(void *arg) {
    worker_data* data = (worker_data*)arg;
    uint8_t*     exe  = get_game_name(data);
    requestInfo* info = get_req_info(data);
    HANDLE       game = NULL;
    init_client();
    if(exe == NULL || strlen((char*)exe) < 1) {
        set_status_text(data, (uint8_t*)"Please set an executable name in the config file.");
        goto done;
    }
    if(info == NULL) {
        goto done;
    }
    // Ping server to check Auth!
    unsigned ping_time;
    set_status_text(data, (uint8_t*)"Pinging the server!");
    requestState ping_ret = ping_server(info, &ping_time);
    set_status_text(data, (uint8_t*)"Finished pinging server!");
    if(ping_ret == AUTH_FAIL) { // Auth failed so just stop
        set_request_fail_message(data, ping_ret);
        goto done;
    }
    for(;;) {
        if(game != NULL) {
            CloseHandle(game);
            game = NULL;
        }
        uint8_t buffer[64] = { 0 };
        uint8_t profile[20] = { 0 };
        set_profile_id(data, profile); // Empty profile ID text
        if(get_stop(data)) {
            break;
        }
        // Get the drives
        uint32_t drives = removable_only(get_cur_drives(data));
        if(drives < 1) {
            set_status_text(data, (uint8_t*)"Please select a removable drive.");
            Sleep(SLEEP_AMT);
            continue;
        }
        if(get_profile_id_from_drives(data, info, profile, 20, drives) == false) {
            Sleep(SLEEP_AMT);
            continue;
        }
        set_profile_id(data, profile);
        // Got a profile now lets get a handle to the game.
        snprintf((char*)buffer, 63, "Looking for game process (%s)", exe);
        while(game == NULL) {
            if(get_stop(data)) {
                goto done; // Use goto to break out both loops
            }
            set_status_text(data, (uint8_t*)buffer);
            game = get_process_handle((char*)exe);
            if(game == NULL) {
                Sleep(500);
            }
        }

        if(update_game(game, profile, info, data)) {
            goto done;
        }

        Sleep(SLEEP_AMT);
    }
    done:
    if(game != NULL) {
        CloseHandle(game);
    }
    free(exe);
    request_info_free(info);
    cleanup_client();
    return 0;
}