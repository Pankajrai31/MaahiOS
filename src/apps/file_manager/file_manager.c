/**
 * File Manager - MaahiOS File Browser Application
 * 
 * Uses libmaahi for all UI and filesystem operations.
 * Full navigation support: double-click folders, go up, path display
 */

#include "../../system/libraries/maahi.h"

/* Navigation stack for going back up */
#define MAX_PATH_DEPTH 8
static unsigned int g_path_lba[MAX_PATH_DEPTH];
static unsigned int g_path_size[MAX_PATH_DEPTH];
static char g_path_names[MAX_PATH_DEPTH][32];
static int g_path_depth = 0;

/* Current directory info */
static unsigned int g_current_lba = 0;
static unsigned int g_current_size = 0;
static char g_current_path[128] = "/";

/* File entries cache */
static MaahiFileEntry g_entries[16];
static int g_entry_count = 0;

/* UI element IDs */
static int g_window_id = -1;
static int g_path_label_id = -1;
static int g_up_button_id = -1;
static int g_refresh_button_id = -1;
static int g_list_id = -1;
static int g_status_label_id = -1;

/* ============== Helper Functions ============== */

static int my_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void my_strcat(char *dst, const char *src) {
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

/* Build file list string for display */
static void build_file_list(char *buffer, int max_len) {
    int pos = 0;
    for (int i = 0; i < g_entry_count && pos < max_len - 2; i++) {
        /* Add directory/file marker */
        if (g_entries[i].is_directory) {
            buffer[pos++] = '[';
            buffer[pos++] = 'D';
            buffer[pos++] = ']';
            buffer[pos++] = ' ';
        } else {
            buffer[pos++] = '[';
            buffer[pos++] = 'F';
            buffer[pos++] = ']';
            buffer[pos++] = ' ';
        }
        
        /* Copy filename */
        const char *name = g_entries[i].name;
        while (*name && pos < max_len - 3) {
            buffer[pos++] = *name++;
        }
        
        /* Add newline between entries */
        if (i < g_entry_count - 1) {
            buffer[pos++] = '\n';
        }
    }
    buffer[pos] = '\0';
}

/* Load current directory contents */
static void load_directory(void) {
    maahi_print("[FM] Loading directory...\n");
    
    g_entry_count = maahi_list_dir(g_current_lba, g_current_size, g_entries, 16);
    
    if (g_entry_count < 0) {
        maahi_print("[FM] Failed to load directory\n");
        g_entry_count = 0;
    } else {
        maahi_print("[FM] Directory loaded OK\n");
    }
}

/* Refresh UI after navigation */
static void refresh_ui(void) {
    static char new_list_buffer[512];
    build_file_list(new_list_buffer, sizeof(new_list_buffer));
    
    /* Update list control text */
    if (g_list_id >= 0) {
        maahi_update_text(g_list_id, new_list_buffer);
    }
    
    /* Update path label */
    if (g_path_label_id >= 0) {
        maahi_update_text(g_path_label_id, g_current_path);
    }
    
    maahi_print("[FM] UI refreshed\n");
}

/* Navigate into a subdirectory */
static void navigate_into(int entry_index) {
    if (entry_index < 0 || entry_index >= g_entry_count) return;
    if (!g_entries[entry_index].is_directory) return;
    
    /* Push current location to stack */
    if (g_path_depth < MAX_PATH_DEPTH) {
        g_path_lba[g_path_depth] = g_current_lba;
        g_path_size[g_path_depth] = g_current_size;
        
        /* Copy last path component */
        int len = my_strlen(g_entries[entry_index].name);
        if (len > 31) len = 31;
        for (int i = 0; i < len; i++) {
            g_path_names[g_path_depth][i] = g_entries[entry_index].name[i];
        }
        g_path_names[g_path_depth][len] = '\0';
        g_path_depth++;
    }
    
    /* Update current directory */
    g_current_lba = g_entries[entry_index].lba;
    g_current_size = g_entries[entry_index].size;
    
    /* Update path string */
    my_strcat(g_current_path, g_entries[entry_index].name);
    my_strcat(g_current_path, "/");
    
    /* Load new directory */
    load_directory();
}

/* Navigate up to parent directory */
static void navigate_up(void) {
    if (g_path_depth <= 0) return;
    
    g_path_depth--;
    g_current_lba = g_path_lba[g_path_depth];
    g_current_size = g_path_size[g_path_depth];
    
    /* Rebuild path string */
    g_current_path[0] = '/';
    g_current_path[1] = '\0';
    for (int i = 0; i < g_path_depth; i++) {
        my_strcat(g_current_path, g_path_names[i]);
        my_strcat(g_current_path, "/");
    }
    
    /* Load parent directory */
    load_directory();
}

/**
 * File Manager main entry point
 */
void file_manager_main_c() {
    maahi_print("[FILE_MANAGER] Starting with libmaahi...\n");
    
    /* Get root directory info */
    maahi_get_root_info(&g_current_lba, &g_current_size);
    maahi_print("[FM] Root directory obtained\n");
    
    /* Load root directory */
    load_directory();
    
    /* Create main window */
    g_window_id = maahi_create_window(150, 80, 520, 380, "File Manager");
    if (g_window_id < 0) {
        maahi_print("[FILE_MANAGER] Failed to create window\n");
        while(1);
    }
    
    /* Set folder icon for window */
    maahi_set_window_icon(g_window_id, "folder_1");
    
    /* Path label at top */
    g_path_label_id = maahi_create_label(g_window_id, 20, 10, g_current_path);
    
    /* Navigation buttons */
    g_up_button_id = maahi_create_button(g_window_id, 20, 35, 60, 28, "Up");
    g_refresh_button_id = maahi_create_button(g_window_id, 90, 35, 80, 28, "Refresh");
    
    /* Build file list string */
    static char file_list_buffer[512];
    build_file_list(file_list_buffer, sizeof(file_list_buffer));
    
    /* Create file list */
    g_list_id = maahi_create_list(g_window_id, 20, 70, 470, 250, file_list_buffer);
    
    /* Status bar */
    static char status_buf[64];
    int cnt = g_entry_count;
    status_buf[0] = '0' + (cnt / 10) % 10;
    status_buf[1] = '0' + cnt % 10;
    status_buf[2] = ' ';
    status_buf[3] = 'i'; status_buf[4] = 't'; status_buf[5] = 'e';
    status_buf[6] = 'm'; status_buf[7] = 's';
    status_buf[8] = '\0';
    g_status_label_id = maahi_create_label(g_window_id, 20, 330, status_buf);
    
    maahi_print("[FILE_MANAGER] UI created, entering event loop\n");
    
    /* Event loop */
    MaahiEvent event;
    int idle_count = 0;
    
    while (1) {
        int has_event = maahi_poll_event(&event);
        
        if (has_event) {
            idle_count = 0;
            
            if (event.type == MAAHI_EVENT_CLICK) {
                if (event.control_id == g_up_button_id) {
                    maahi_print("[FM] Up button clicked\n");
                    navigate_up();
                    refresh_ui();
                } else if (event.control_id == g_refresh_button_id) {
                    maahi_print("[FM] Refresh clicked\n");
                    load_directory();
                    refresh_ui();
                }
            } else if (event.type == MAAHI_EVENT_DBLCLICK) {
                /* Double-clicked on list item */
                int selected_index = event.data;
                if (selected_index >= 0 && selected_index < g_entry_count) {
                    if (g_entries[selected_index].is_directory) {
                        maahi_print("[FM] Entering folder\n");
                        navigate_into(selected_index);
                        refresh_ui();
                    } else {
                        maahi_print("[FM] Selected file\n");
                    }
                }
            }
        } else {
            /* No event - add small delay */
            idle_count++;
            if (idle_count > 10) {
                for (volatile int i = 0; i < 5000; i++);
                idle_count = 0;
            }
        }
        
        /* Yield CPU to other processes */
        maahi_yield();
    }
}
