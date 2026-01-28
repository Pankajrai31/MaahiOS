/**
 * I/O Syscalls
 * Handles disk operations and filesystem (ISO9660) operations
 */

#include "syscall_common.h"

/**
 * Handle I/O-related syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_io(unsigned int syscall_num,
                                unsigned int arg1,
                                unsigned int arg2,
                                unsigned int arg3,
                                unsigned int arg4_esi,
                                unsigned int arg5,
                                unsigned int arg6) {
    unsigned int return_value = 0;
    
    switch(syscall_num) {
        // ==================== DISK OPERATIONS ====================
        
        case SYSCALL_DISK_GET_COUNT: {
            // Get number of disks
            return_value = disk_subsystem_get_count();
            if (return_value == 0) {
                serial_print("[SYSCALL 56] WARNING: disk_subsystem_get_count() returned 0\n");
            } else if (return_value == 1) {
                serial_print("[SYSCALL 56] disk_subsystem_get_count() returned 1\n");
            } else {
                serial_print("[SYSCALL 56] disk_subsystem_get_count() returned other value\n");
            }
            break;
        }
        
        case SYSCALL_DISK_GET_INFO: {
            // Get disk information
            // arg1 = index, arg2 = buffer pointer
            uint8_t index = (uint8_t)arg1;
            void *buffer = (void*)arg2;
            
            serial_print("[SYSCALL 57] index=");
            serial_hex(index);
            serial_print(" buffer=");
            serial_hex((unsigned char)((arg2 >> 24) & 0xFF));
            serial_hex((unsigned char)((arg2 >> 16) & 0xFF));
            serial_hex((unsigned char)((arg2 >> 8) & 0xFF));
            serial_hex((unsigned char)(arg2 & 0xFF));
            serial_print("\n");
            
            void *disk_info = disk_subsystem_get_disk(index);
            
            serial_print("[SYSCALL 57] disk_info=");
            if (disk_info) {
                serial_print("valid");
            } else {
                serial_print("NULL");
            }
            serial_print("\n");
            
            if (disk_info && buffer) {
                // Copy disk_info_t structure to user buffer (76 bytes)
                uint8_t *src = (uint8_t*)disk_info;
                uint8_t *dst = (uint8_t*)buffer;
                for (int i = 0; i < 76; i++) {
                    dst[i] = src[i];
                }
                return_value = 0;  // Success
                serial_print("[SYSCALL 57] copy OK\n");
            } else {
                return_value = -1;  // Error
                serial_print("[SYSCALL 57] FAILED\n");
            }
            break;
        }
        
        case SYSCALL_DISK_READ_SECTOR: {
            // Read disk sector
            // arg1 = disk_index, arg2 = lba, arg3 = buffer
            uint8_t disk_index = (uint8_t)arg1;
            uint32_t lba = (uint32_t)arg2;
            void *buffer = (void*)arg3;
            return_value = disk_subsystem_read_sector(disk_index, lba, buffer);
            break;
        }
        
        // ==================== ISO9660 FILESYSTEM ====================
        
        case 74: {  // SYSCALL_ISO_GET_FILE_COUNT
            // Get number of files in ISO root
            return_value = iso9660_get_file_count();
            break;
        }
        
        case 75: {  // SYSCALL_ISO_LIST_FILES
            // List files in ISO root
            // arg1 = pointer to buffer, arg2 = max entries
            void *buffer = (void*)arg1;
            int max_entries = (int)arg2;
            return_value = iso9660_list_root(buffer, max_entries);
            break;
        }
        
        case 77: {  // SYSCALL_ISO_LIST_BOOT
            // List files in /boot directory
            // arg1 = pointer to buffer, arg2 = max entries
            void *buffer = (void*)arg1;
            int max_entries = (int)arg2;
            return_value = iso9660_list_boot(buffer, max_entries);
            break;
        }
        
        case SYSCALL_ISO_LIST_DIR: {
            // List files in directory by LBA
            // arg1 = directory LBA, arg2 = directory size, arg3 = buffer, arg4_esi = max entries
            uint32_t dir_lba = (uint32_t)arg1;
            uint32_t dir_size = (uint32_t)arg2;
            void *buffer = (void*)arg3;
            int max_entries = (int)arg4_esi;
            return_value = iso9660_list_directory(dir_lba, dir_size, buffer, max_entries);
            break;
        }
        
        case SYSCALL_ISO_GET_ROOT_INFO: {
            // Get root directory LBA and size
            // arg1 = pointer to LBA, arg2 = pointer to size
            uint32_t *lba_ptr = (uint32_t*)arg1;
            uint32_t *size_ptr = (uint32_t*)arg2;
            if (lba_ptr) *lba_ptr = iso9660_get_root_lba();
            if (size_ptr) *size_ptr = iso9660_get_root_size();
            return_value = 0;
            break;
        }
        
        case SYSCALL_ISO_READ_FILE: {
            // Read file data from ISO
            // arg1 = file LBA, arg2 = file size, arg3 = buffer, arg4_esi = max size
            uint32_t file_lba = (uint32_t)arg1;
            uint32_t file_size = (uint32_t)arg2;
            void *buffer = (void*)arg3;
            uint32_t max_size = (uint32_t)arg4_esi;
            return_value = iso9660_read_file(file_lba, file_size, buffer, max_size);
            break;
        }
        
        case SYSCALL_ISO_FIND_READ_FILE: {
            // Find and read file by name in directory
            // arg1 = dir LBA, arg2 = dir size, arg3 = filename, arg4_esi = buffer, arg5 = max size
            uint32_t dir_lba = (uint32_t)arg1;
            uint32_t dir_size = (uint32_t)arg2;
            const char *filename = (const char*)arg3;
            void *buffer = (void*)arg4_esi;
            uint32_t max_size = (uint32_t)arg5;
            return_value = iso9660_find_and_read_file(dir_lba, dir_size, filename, buffer, max_size);
            break;
        }
        
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return return_value;
}
