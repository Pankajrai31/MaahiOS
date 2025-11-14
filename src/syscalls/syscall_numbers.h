#ifndef SYSCALL_NUMBERS_H
#define SYSCALL_NUMBERS_H

// Syscall number definitions
// These are the identifiers used in EAX to specify which syscall to invoke

#define SYSCALL_PUTCHAR     1   // putchar(char c) - Print single character
#define SYSCALL_PUTS        2   // puts(const char* str) - Print null-terminated string
#define SYSCALL_PUTINT      3   // putint(int num) - Print integer
#define SYSCALL_EXIT        4   // exit(int code) - Terminate program
#define SYSCALL_WRITE       5   // write(int fd, char* buf, int size) - Write to file descriptor
#define SYSCALL_ALLOC_PAGE  6   // alloc_page() - Allocate a physical page (4KB)
#define SYSCALL_FREE_PAGE   7   // free_page(void* addr) - Free a physical page
#define SYSCALL_CLEAR       8   // clear() - Clear screen

#endif // SYSCALL_NUMBERS_H
