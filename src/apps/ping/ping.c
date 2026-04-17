/**
 * ping.mex - MaahiOS Ping Utility
 *
 * Console non-interactive .mex application.
 * Sends ICMP echo requests to test network connectivity.
 *
 * Usage:
 *   ping                   Ping gateway (10.0.2.2)
 *   ping <ip>              Ping specified IP (e.g. "10.0.2.2")
 *   ping help              Show help
 *
 * Layer 1 (App). Ring 3.
 * Uses: libconsole, libnet, liblog
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libconsole/libconsole.h"
#include "../../system/libraries/libnet/libnet.h"
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

static int str_len(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

/*=============================================================================
 * IP ADDRESS PARSER
 * Parses "A.B.C.D" → host-byte-order uint32
 *===========================================================================*/

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static uint32_t parse_ip(const char *str) {
    uint32_t octets[4] = {0, 0, 0, 0};
    int octet_idx = 0;
    int i = 0;

    while (str[i] && octet_idx < 4) {
        if (is_digit(str[i])) {
            octets[octet_idx] = octets[octet_idx] * 10 + (str[i] - '0');
            if (octets[octet_idx] > 255) return 0; /* Invalid */
        } else if (str[i] == '.') {
            octet_idx++;
        } else {
            return 0; /* Invalid character */
        }
        i++;
    }

    if (octet_idx != 3) return 0; /* Need exactly 3 dots */

    return LIBNET_IP(octets[0], octets[1], octets[2], octets[3]);
}

/*=============================================================================
 * NUMBER TO STRING
 *===========================================================================*/

static void uint_to_str(uint32_t val, char *buf, int buf_size) {
    if (buf_size < 2) { buf[0] = '\0'; return; }

    char tmp[12];
    int pos = 0;

    if (val == 0) {
        buf[0] = '0'; buf[1] = '\0';
        return;
    }

    while (val > 0 && pos < 11) {
        tmp[pos++] = '0' + (char)(val % 10);
        val /= 10;
    }

    /* Reverse */
    int out = 0;
    for (int i = pos - 1; i >= 0 && out < buf_size - 1; i--) {
        buf[out++] = tmp[i];
    }
    buf[out] = '\0';
}

static void ip_to_str(uint32_t ip, char *buf, int buf_size) {
    char o1[4], o2[4], o3[4], o4[4];
    uint_to_str((ip >> 24) & 0xFF, o1, 4);
    uint_to_str((ip >> 16) & 0xFF, o2, 4);
    uint_to_str((ip >> 8) & 0xFF, o3, 4);
    uint_to_str(ip & 0xFF, o4, 4);

    int i = 0;
    for (int j = 0; o1[j] && i < buf_size - 1; j++) buf[i++] = o1[j];
    if (i < buf_size - 1) buf[i++] = '.';
    for (int j = 0; o2[j] && i < buf_size - 1; j++) buf[i++] = o2[j];
    if (i < buf_size - 1) buf[i++] = '.';
    for (int j = 0; o3[j] && i < buf_size - 1; j++) buf[i++] = o3[j];
    if (i < buf_size - 1) buf[i++] = '.';
    for (int j = 0; o4[j] && i < buf_size - 1; j++) buf[i++] = o4[j];
    buf[i] = '\0';
}

/*=============================================================================
 * PRINT HELPERS
 *===========================================================================*/

static void print_config(void) {
    libnet_config_t config;
    if (libnet_get_config(&config) != 0) {
        console_print("  Unable to read network configuration.\n");
        return;
    }

    char ip_str[16];

    console_print("  Network Interface:\n");

    ip_to_str(config.ip_addr, ip_str, 16);
    console_print("    IP Address : ");
    console_print(ip_str);
    console_print("\n");

    ip_to_str(config.netmask, ip_str, 16);
    console_print("    Netmask    : ");
    console_print(ip_str);
    console_print("\n");

    ip_to_str(config.gateway, ip_str, 16);
    console_print("    Gateway    : ");
    console_print(ip_str);
    console_print("\n");

    ip_to_str(config.dns, ip_str, 16);
    console_print("    DNS        : ");
    console_print(ip_str);
    console_print("\n");

    console_print("    MAC        : ");
    char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        char mac_byte[3];
        mac_byte[0] = hex[(config.mac[i] >> 4) & 0xF];
        mac_byte[1] = hex[config.mac[i] & 0xF];
        mac_byte[2] = '\0';
        console_print(mac_byte);
        if (i < 5) console_print(":");
    }
    console_print("\n");

    console_print("    Link       : ");
    console_print(config.link_up ? "UP" : "DOWN");
    console_print("\n");
    console_print("    Status     : ");
    console_print(config.initialized ? "Initialized" : "Not initialized");
    console_print("\n");
}

/*=============================================================================
 * DO PING
 *===========================================================================*/

static void do_ping(uint32_t target_ip, int count) {
    char ip_str[16];
    ip_to_str(target_ip, ip_str, 16);

    console_print("Pinging ");
    console_print(ip_str);
    console_print(" with 32 bytes of data:\n\n");

    int success_count = 0;
    uint32_t min_rtt = 0xFFFFFFFF;
    uint32_t max_rtt = 0;
    uint32_t total_rtt = 0;

    for (int i = 0; i < count; i++) {
        libnet_ping_result_t result;
        int ret = libnet_ping(target_ip, 3000, &result);

        if (ret == 0 && result.success) {
            char rtt_str[12];
            char ttl_str[12];
            char seq_str[12];
            uint_to_str(result.rtt_ms, rtt_str, 12);
            uint_to_str(result.ttl, ttl_str, 12);
            uint_to_str(result.seq, seq_str, 12);

            console_print("  Reply from ");
            console_print(ip_str);
            console_print(": bytes=32 seq=");
            console_print(seq_str);
            console_print(" time=");
            console_print(rtt_str);
            console_print("ms TTL=");
            console_print(ttl_str);
            console_print("\n");

            success_count++;
            total_rtt += result.rtt_ms;
            if (result.rtt_ms < min_rtt) min_rtt = result.rtt_ms;
            if (result.rtt_ms > max_rtt) max_rtt = result.rtt_ms;
        } else {
            console_print("  Request timed out.\n");
        }

        /* Wait 1 second between pings */
        if (i < count - 1) {
            syscall1(SYS_SLEEP, 50);  /* ~1 second at 50 Hz */
        }
    }

    /* Summary */
    console_print("\nPing statistics for ");
    console_print(ip_str);
    console_print(":\n");

    char num_str[12];
    console_print("    Packets: Sent = ");
    uint_to_str(count, num_str, 12);
    console_print(num_str);
    console_print(", Received = ");
    uint_to_str(success_count, num_str, 12);
    console_print(num_str);
    console_print(", Lost = ");
    uint_to_str(count - success_count, num_str, 12);
    console_print(num_str);

    if (count > 0) {
        int loss_pct = ((count - success_count) * 100) / count;
        console_print(" (");
        uint_to_str(loss_pct, num_str, 12);
        console_print(num_str);
        console_print("% loss)");
    }
    console_print("\n");

    if (success_count > 0) {
        uint32_t avg_rtt = total_rtt / (uint32_t)success_count;
        console_print("    Approximate round trip times:\n");
        console_print("      Minimum = ");
        uint_to_str(min_rtt, num_str, 12);
        console_print(num_str);
        console_print("ms, Maximum = ");
        uint_to_str(max_rtt, num_str, 12);
        console_print(num_str);
        console_print("ms, Average = ");
        uint_to_str(avg_rtt, num_str, 12);
        console_print(num_str);
        console_print("ms\n");
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    liblog_init();

    if (console_init() != 0) {
        liblog(LOG_ERROR, "PING", "Failed to init console output");
        return;
    }

    /* Check if networking is available */
    if (!libnet_is_available()) {
        console_print("Error: Network is not available.\n");
        console_print("Make sure QEMU is started with -netdev user,id=net0 "
                       "-device e1000,netdev=net0\n");
        return;
    }

    /* Parse arguments */
    char args[64];
    args[0] = '\0';
    console_get_args(args, 64);

    /* Default: ping gateway */
    uint32_t target_ip = LIBNET_IP(10, 0, 2, 2);
    int ping_count = 4;

    if (args[0] == '\0') {
        /* No args → show config + ping gateway */
        console_print("MaahiOS Ping Utility\n\n");
        print_config();
        console_print("\n");
        do_ping(target_ip, ping_count);
        return;
    }

    if (str_equal(args, "help")) {
        console_print("Ping - Network Connectivity Test\n\n");
        console_print("Usage:\n");
        console_print("  ping              Ping gateway (10.0.2.2)\n");
        console_print("  ping <ip>         Ping specified IP address\n");
        console_print("  ping help         Show this help\n");
        console_print("\nExamples:\n");
        console_print("  C:\\> ping\n");
        console_print("  C:\\> ping 10.0.2.2\n");
        console_print("  C:\\> ping 10.0.2.3\n");
        return;
    }

    /* Try to parse as IP address */
    target_ip = parse_ip(args);
    if (target_ip == 0) {
        console_print("Invalid IP address: ");
        console_print(args);
        console_print("\nUse dotted decimal format: A.B.C.D\n");
        return;
    }

    do_ping(target_ip, ping_count);
}
