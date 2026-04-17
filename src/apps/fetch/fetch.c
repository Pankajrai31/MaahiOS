/**
 * fetch.mex - MaahiOS Network Fetch Test
 *
 * Console non-interactive .mex application.
 * Tests the full networking stack: sockets, DNS, HTTP.
 *
 * Usage:
 *   fetch                  Run all network tests
 *   fetch dns <hostname>   Resolve hostname via DNS
 *   fetch http <url>       Fetch URL via HTTP GET
 *
 * Layer 1 (App). Ring 3.
 * Uses: libconsole, libnet, libhttp, liblog
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libconsole/libconsole.h"
#include "../../system/libraries/libnet/libnet.h"
#include "../../system/libraries/libhttp/libhttp.h"
#include "../../system/libraries/core/syscall_helpers.h"

/*=============================================================================
 * STRING HELPERS
 *===========================================================================*/

static int str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void uint_to_str(uint32_t val, char *buf, int buf_size) {
    if (buf_size < 2) { buf[0] = '\0'; return; }
    char tmp[12];
    int pos = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (val > 0 && pos < 11) {
        tmp[pos++] = '0' + (char)(val % 10);
        val /= 10;
    }
    int out = 0;
    for (int i = pos - 1; i >= 0 && out < buf_size - 1; i--)
        buf[out++] = tmp[i];
    buf[out] = '\0';
}

static void int_to_str(int val, char *buf, int buf_size) {
    if (val < 0) {
        buf[0] = '-';
        uint_to_str((uint32_t)(-val), buf + 1, buf_size - 1);
    } else {
        uint_to_str((uint32_t)val, buf, buf_size);
    }
}

static void ip_to_str(uint32_t ip, char *buf, int buf_size) {
    char o[4][4];
    uint_to_str((ip >> 24) & 0xFF, o[0], 4);
    uint_to_str((ip >> 16) & 0xFF, o[1], 4);
    uint_to_str((ip >> 8) & 0xFF, o[2], 4);
    uint_to_str(ip & 0xFF, o[3], 4);
    int i = 0;
    for (int a = 0; a < 4; a++) {
        for (int j = 0; o[a][j] && i < buf_size - 1; j++) buf[i++] = o[a][j];
        if (a < 3 && i < buf_size - 1) buf[i++] = '.';
    }
    buf[i] = '\0';
}

/*=============================================================================
 * PRINT HELPERS
 *===========================================================================*/

static void print(const char *s) { console_print(s); }
static void println(const char *s) { console_print(s); console_print("\n"); }
static void print_num(uint32_t val) {
    char buf[12]; uint_to_str(val, buf, 12); print(buf);
}
static void print_int(int val) {
    char buf[16]; int_to_str(val, buf, 16); print(buf);
}
static void print_ip(uint32_t ip) {
    char buf[16]; ip_to_str(ip, buf, 16); print(buf);
}

/*=============================================================================
 * TEST 1: SOCKET CREATE/CLOSE
 *===========================================================================*/

static int test_socket_create(void) {
    print("  [1] Socket create/close ... ");

    int tcp_sock = libnet_socket_create(LIBNET_SOCK_TCP);
    if (tcp_sock < 0) {
        print("FAIL (TCP create returned ");
        print_int(tcp_sock);
        println(")");
        return -1;
    }

    int udp_sock = libnet_socket_create(LIBNET_SOCK_UDP);
    if (udp_sock < 0) {
        print("FAIL (UDP create returned ");
        print_int(udp_sock);
        println(")");
        libnet_close(tcp_sock);
        return -1;
    }

    int ret1 = libnet_close(tcp_sock);
    int ret2 = libnet_close(udp_sock);

    if (ret1 != 0 || ret2 != 0) {
        print("FAIL (close returned ");
        print_int(ret1);
        print(", ");
        print_int(ret2);
        println(")");
        return -1;
    }

    println("PASS (TCP + UDP)");
    return 0;
}

/*=============================================================================
 * TEST 2: DNS RESOLVE
 *===========================================================================*/

static int test_dns_resolve(const char *hostname, uint32_t *ip_out) {
    print("  [2] DNS resolve '");
    print(hostname);
    print("' ... ");

    uint32_t ip = 0;
    int ret = libnet_dns_resolve(hostname, &ip);
    if (ret != 0) {
        print("FAIL (returned ");
        print_int(ret);
        println(")");
        return -1;
    }

    if (ip == 0) {
        println("FAIL (IP = 0.0.0.0)");
        return -1;
    }

    print("PASS -> ");
    print_ip(ip);
    println("");

    if (ip_out) *ip_out = ip;
    return 0;
}

/*=============================================================================
 * TEST 3: TCP CONNECT
 *===========================================================================*/

static int test_tcp_connect(uint32_t ip, uint16_t port) {
    print("  [3] TCP connect to ");
    print_ip(ip);
    print(":");
    print_num(port);
    print(" ... ");

    int sock = libnet_socket_create(LIBNET_SOCK_TCP);
    if (sock < 0) {
        print("FAIL (create: ");
        print_int(sock);
        println(")");
        return -1;
    }

    int ret = libnet_connect(sock, ip, port);
    if (ret != 0) {
        print("FAIL (connect: ");
        print_int(ret);
        println(")");
        libnet_close(sock);
        return -1;
    }

    println("PASS (connected!)");

    /* Clean up */
    libnet_close(sock);
    return 0;
}

/*=============================================================================
 * TEST 4: HTTP GET
 *===========================================================================*/

#define HTTP_BUF_SIZE 4096
static char g_http_buf[HTTP_BUF_SIZE];

static int test_http_get(uint32_t ip, uint16_t port, const char *path,
                         const char *host_hdr) {
    print("  [4] HTTP GET ");
    if (host_hdr) { print(host_hdr); }
    print(path);
    print(" ... ");

    libhttp_response_t resp;
    int body_len = libhttp_get_ip(ip, port, path, host_hdr,
                                   g_http_buf, HTTP_BUF_SIZE - 1, &resp);

    if (body_len < 0) {
        print("FAIL (returned ");
        print_int(body_len);
        println(")");
        return -1;
    }

    print("PASS (status=");
    print_num((uint32_t)resp.status_code);
    print(", body=");
    print_num((uint32_t)body_len);
    print(" bytes, type=");
    print(resp.content_type[0] ? resp.content_type : "?");
    println(")");

    /* Print first 512 chars of body */
    if (body_len > 0) {
        println("");
        println("--- Response body (first 512 bytes) ---");
        int show = body_len;
        if (show > 512) show = 512;
        g_http_buf[show] = '\0';
        println(g_http_buf);
        println("--- End ---");
    }

    return 0;
}

/*=============================================================================
 * TEST 5: HTTP GET WITH DNS (full URL path)
 *===========================================================================*/

static int test_http_full(const char *url) {
    print("  [5] HTTP GET ");
    print(url);
    print(" ... ");

    libhttp_response_t resp;
    int body_len = libhttp_get(url, g_http_buf, HTTP_BUF_SIZE - 1, &resp);

    if (body_len < 0) {
        print("FAIL (returned ");
        print_int(body_len);
        println(")");
        return -1;
    }

    print("PASS (status=");
    print_num((uint32_t)resp.status_code);
    print(", body=");
    print_num((uint32_t)body_len);
    println(" bytes)");

    if (body_len > 0) {
        println("");
        println("--- Response body (first 512 bytes) ---");
        int show = body_len;
        if (show > 512) show = 512;
        g_http_buf[show] = '\0';
        println(g_http_buf);
        println("--- End ---");
    }

    return 0;
}

/*=============================================================================
 * RUN ALL TESTS
 *===========================================================================*/

static void run_all_tests(void) {
    println("MaahiOS Network Stack Test Suite");
    println("================================");
    println("");

    /* Show network config */
    libnet_config_t config;
    if (libnet_get_config(&config) == 0) {
        print("  IP:      "); print_ip(config.ip_addr); println("");
        print("  Gateway: "); print_ip(config.gateway); println("");
        print("  DNS:     "); print_ip(config.dns); println("");
        print("  Link:    "); println(config.link_up ? "UP" : "DOWN");
        println("");
    }

    int pass = 0, fail = 0;

    /* Test 1: Socket create/close */
    if (test_socket_create() == 0) pass++; else fail++;

    /* Small delay between tests */
    syscall1(SYS_SLEEP, 5);

    /* Test 2: DNS resolve — use QEMU SLiRP's built-in DNS
     * QEMU's user-mode networking responds to DNS queries sent to 10.0.2.3.
     * It resolves real hostnames via the host's DNS.
     * We try "example.com" which should resolve to 93.184.216.34 */
    uint32_t resolved_ip = 0;
    if (test_dns_resolve("example.com", &resolved_ip) == 0) pass++; else fail++;

    syscall1(SYS_SLEEP, 5);

    /* Test 3: TCP connect — try to connect to resolved IP on port 80
     * QEMU SLiRP forwards TCP to the real internet. */
    if (resolved_ip != 0) {
        if (test_tcp_connect(resolved_ip, 80) == 0) pass++; else fail++;
    } else {
        /* Fallback: try connecting to the gateway (SLiRP host) */
        print("  [3] Skipping TCP (no resolved IP), trying gateway:80 ... ");
        if (test_tcp_connect(LIBNET_IP(10, 0, 2, 2), 80) == 0) pass++; else fail++;
    }

    syscall1(SYS_SLEEP, 5);

    /* Test 4: HTTP GET to resolved IP */
    if (resolved_ip != 0) {
        if (test_http_get(resolved_ip, 80, "/", "example.com") == 0)
            pass++; else fail++;
    } else {
        println("  [4] SKIP (no resolved IP)");
    }

    syscall1(SYS_SLEEP, 5);

    /* Test 5: Full HTTP GET with DNS (URL-based) */
    if (test_http_full("http://example.com/") == 0) pass++; else fail++;

    /* Summary */
    println("");
    println("================================");
    print("Results: ");
    print_num(pass);
    print(" passed, ");
    print_num(fail);
    println(" failed");
    println("================================");
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    liblog_init();

    if (console_init() != 0) {
        liblog(LOG_ERROR, "FETCH", "Failed to init console output");
        return;
    }

    if (!libnet_is_available()) {
        println("Error: Network is not available.");
        println("Make sure QEMU is started with -netdev user,id=net0 -device e1000,netdev=net0");
        return;
    }

    /* Parse arguments */
    char args[256];
    args[0] = '\0';
    console_get_args(args, 256);

    if (args[0] == '\0') {
        /* No args → run all tests */
        run_all_tests();
        return;
    }

    if (str_equal(args, "help")) {
        println("fetch - Network Stack Test Utility");
        println("");
        println("Usage:");
        println("  fetch                 Run all network tests");
        println("  fetch dns <hostname>  Resolve hostname via DNS");
        println("  fetch http <url>      Fetch URL via HTTP GET");
        println("  fetch help            Show this help");
        return;
    }

    if (str_starts_with(args, "dns ")) {
        const char *hostname = args + 4;
        while (*hostname == ' ') hostname++;
        if (*hostname) {
            uint32_t ip = 0;
            test_dns_resolve(hostname, &ip);
        } else {
            println("Usage: fetch dns <hostname>");
        }
        return;
    }

    if (str_starts_with(args, "http ")) {
        const char *url = args + 5;
        while (*url == ' ') url++;
        if (*url) {
            /* Auto-prepend http:// if missing */
            if (!str_starts_with(url, "http://")) {
                static char url_buf[300];
                int i = 0;
                const char *prefix = "http://";
                while (*prefix && i < 290) url_buf[i++] = *prefix++;
                while (*url && i < 299) url_buf[i++] = *url++;
                url_buf[i] = '\0';
                test_http_full(url_buf);
            } else {
                test_http_full(url);
            }
        } else {
            println("Usage: fetch http <url>");
        }
        return;
    }

    print("Unknown command: ");
    println(args);
    println("Type 'fetch help' for usage.");
}
