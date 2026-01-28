/*
 * MaahiOS Error Codes
 * Standardized error codes for syscalls and kernel functions
 */

#ifndef ERRNO_H
#define ERRNO_H

/* Success */
#define E_OK         0

/* Standard error codes (negative values) */
#define E_NOMEM     -1   /* Out of memory */
#define E_INVAL     -2   /* Invalid argument */
#define E_NOENT     -3   /* No such entry/file/resource */
#define E_FAULT     -4   /* Bad address / memory fault */
#define E_NOSPC     -5   /* No space left (controls, windows, etc.) */
#define E_BUSY      -6   /* Resource busy */
#define E_PERM      -7   /* Permission denied */
#define E_RANGE     -8   /* Value out of range */
#define E_IO        -9   /* I/O error */
#define E_AGAIN    -10   /* Try again / resource temporarily unavailable */
#define E_EXIST    -11   /* Resource already exists */
#define E_NODEV    -12   /* No such device */
#define E_NOTDIR   -13   /* Not a directory */
#define E_ISDIR    -14   /* Is a directory */
#define E_NOPROC   -15   /* No such process */
#define E_TIMEOUT  -16   /* Operation timed out */
#define E_OVERFLOW -17   /* Value too large */
#define E_NOIMPL   -18   /* Not implemented */

/* Error code to string conversion (for debugging) */
static inline const char* errno_str(int err) {
    switch (err) {
        case E_OK:       return "OK";
        case E_NOMEM:    return "Out of memory";
        case E_INVAL:    return "Invalid argument";
        case E_NOENT:    return "No such entry";
        case E_FAULT:    return "Memory fault";
        case E_NOSPC:    return "No space left";
        case E_BUSY:     return "Resource busy";
        case E_PERM:     return "Permission denied";
        case E_RANGE:    return "Out of range";
        case E_IO:       return "I/O error";
        case E_AGAIN:    return "Try again";
        case E_EXIST:    return "Already exists";
        case E_NODEV:    return "No such device";
        case E_NOTDIR:   return "Not a directory";
        case E_ISDIR:    return "Is a directory";
        case E_NOPROC:   return "No such process";
        case E_TIMEOUT:  return "Timeout";
        case E_OVERFLOW: return "Overflow";
        case E_NOIMPL:   return "Not implemented";
        default:         return "Unknown error";
    }
}

/* Macro to check if result is an error */
#define IS_ERROR(x)  ((x) < 0)

/* Macro to check if result is success */
#define IS_OK(x)     ((x) >= 0)

#endif /* ERRNO_H */
