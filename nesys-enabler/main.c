#define _WIN32_WINNT 0x0600
#define _WIN32_IE    0x0600

#include <stdio.h>
#include <windows.h>

#include "config.h"
#include "ms-window.h"
#include "worker-data.h"
#include "worker.h"

msWindow* main_win = NULL;

msCtrl status      = -1;
msCtrl displayID   = -1;

// Mainly for the drives changed call back
msCtrl drives[26]  = { -1 };
uint32_t save_to   = 0;
void drives_changed(msWindow* win, msCtrl ctrl, msCtrlEvent event) {
    unsigned drive = 0;
    for(; drive < 26; drive++) {
        if(drives[drive] == ctrl) {
            break;
        }
    }
    uint32_t clear = 1 << drive;
    clear    = ~clear;
    save_to &= clear;
    clear    = (ctrl_checkbox_get_state(win, ctrl) == 1) << drive;
    save_to |= clear;
    save_drives(save_to);
}
// Enable and disable the gui as drive states change on the systam
void update_drive_checks() {
    for(int i = 0; i < 26; i++) {
        char path[] = "A:\\";
        path[0] += i;
        int state = (save_to >> i) & 0x1;
        if(state != ctrl_checkbox_get_state(main_win, drives[i])) {
            ctrl_checkbox_set_state(main_win, drives[i], state);
        }
        if(GetDriveTypeA(path) == DRIVE_REMOVABLE) {
            ctrl_enable(main_win, drives[i]);
            continue;
        }
        ctrl_disable(main_win, drives[i]);
    }
}

void main_win_failed() {
    annihilate_window(main_win);
    cleanup_ms_window();
    exit(-1);
}

void create_main_window() {
    if(init_ms_window() == false) {
        exit(-1);
    }
    main_win = construct_window("NesysProfileEnabler", 295, 175, false);
    if(main_win == NULL) {
        cleanup_ms_window();
        exit(-1);
    }
    unsigned h = 10, w = 0;
    msCtrl made_by = ctrl_create_text(main_win, "Nesys profile enabler\nby derole, anfilt", 0, h, CENTER);
    msCtrl grp     = ctrl_create_groupbox(main_win, "Fetch Profile IDs (Removeable Drives Only)", 10, 90, LEFT);
    if(made_by < 0 || grp < 0) { main_win_failed(); }

    h += ctrl_get_height(main_win, made_by) + 3;
    msCtrl lbl1    = ctrl_create_text(main_win, "Status: ", 10, h, LEFT);
    if(lbl1 < 0) { main_win_failed(); }
    w = ctrl_get_width(main_win, lbl1) + 10;
    status         = ctrl_create_text(main_win, "", w, h, LEFT);
    if(status < 0) { main_win_failed(); }

    h += ctrl_get_height(main_win, lbl1) + 2;
    msCtrl lbl2    = ctrl_create_text(main_win, "Profile ID: ", 10, h, LEFT);
    if(lbl2 < 0) { main_win_failed(); }
    w = ctrl_get_width(main_win, lbl2) + 10;
    displayID      = ctrl_create_text(main_win, "", w, h, LEFT);
    if(displayID < 0) { main_win_failed(); }

    h += ctrl_get_height(main_win, lbl2) + 2;
    char s_ip[16] = { 0 };
    char buff[32] = { 0 };
    get_server_ip(s_ip, 15);
    snprintf(buff, 31, "Server: %s:%u", s_ip, get_port());
    msCtrl server  = ctrl_create_text(main_win, buff, 10, h, LEFT);
     if(server < 0) { main_win_failed(); }

    ctrl_center_vert(main_win, made_by);
    ctrl_resize(main_win, grp, 275, 75);
    // Create and posistion check boxes
    unsigned  width   = 0;
    unsigned  height  = 0;
    for(int i = 0; i < 26; i++) {
        // Aproximate posistion
        unsigned x =  16 + (38 * (i % 7));
        unsigned y = 106 + (13 * (i / 7));
        char str[] = "A:\\";
        str[0] += i;
        drives[i] = ctrl_create_checkbox(main_win, str, x, y);
        if(drives[i] < 0) {
            main_win_failed();
        }
        unsigned w = ctrl_get_width(main_win, drives[i]);
        unsigned h = ctrl_get_height(main_win, drives[i]);
        if(w > width) {
            width = w;
        }
        if(h > height) {
            height = h;
        }
    }
    // Now that we have the largest dimension arrange in a grid.
    for(int i = 0; i < 26; i++) {
        unsigned x =  16 + (width  * (i % 7));
        unsigned y = 106 + (height * (i / 7));
        ctrl_move(main_win, drives[i], x, y);
        // Also set call back to handle changes
        ctrl_set_callback(main_win, drives[i], drives_changed);
    }
}

int main(int argc, char* argv[]) {
    int ret = 0;
    // Init stuff
    create_main_window();
    worker_data* data = create_worker_data();
    if(data == NULL) {
        MessageBoxA(NULL, "Failed to create shared data!", "ERROR", MB_ICONERROR | MB_OK);
        ret = -1;
        goto clean_window;
    }
    worker_thread* worker = worker_create(data);
    if(worker == NULL) {
        MessageBoxA(NULL, "Failed to create worker!", "ERROR", MB_ICONERROR | MB_OK);
        ret = -1;
        goto clean_worker_data;
    }
    write_defaults_to_disk();
    save_to = get_drives();

    // Give work thread config data
    set_cur_drives(data, save_to);

    char buf[64]  = { 0 };
    get_server_ip(buf, 63);
    set_IPv4_address(data,  (uint8_t*)buf);

    memset(buf, 0, 64);
    get_exe_name(buf, 63);
    set_game_name(data,  (uint8_t*)buf);

    memset(buf, 0, 64);
    get_access_code(buf, 63);
    set_network_key(data, buf, strlen(buf));
    set_network_key(data, (uint8_t*)"\000\001\002\003\004\005\006\007", 8);

    set_cur_port(data, get_port());

    worker_start(worker);
    // GUI Update loop
    set_wait_timeout(500);
    winResult res = WIN_CONTINE;
    do {
        update_drive_checks();
        set_cur_drives(data, save_to);
        uint8_t* profile = get_profile_id(data);
        if(profile != NULL) {
            ctrl_set_text(main_win, displayID, (char*)profile, true);
            free(profile);
        }
        uint8_t* status_txt = get_status_text(data);
        if(status_txt != NULL) {
            ctrl_set_text(main_win, status, (char*)status_txt, true);
            free(status_txt);
        }
        res = wait_windows();
    } while(res != WIN_EXIT);
    worker_cleanup(worker);
    clean_worker_data:
        free_worker_data(data);
    clean_window:
        annihilate_window(main_win);
        cleanup_ms_window();
    return ret;
}