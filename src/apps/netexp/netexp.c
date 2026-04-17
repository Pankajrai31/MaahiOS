/**
 * netexp.mex - MaahiOS Network Explorer (Design System v2)
 *
 * Description:
 *   Windowed GUI app showing network information and packet activity.
 *   Uses the new tab control to split into two views:
 *
 *   Tab 1 "Overview":
 *     - IP address, netmask, gateway, DNS
 *     - MAC address, link status
 *     - Live packet counters (TX/RX, ARP, ICMP)
 *
 *   Tab 2 "Packets":
 *     - Scrollable table of recent packet log entries
 *     - Direction (TX/RX), protocol, summary, size
 *
 *   Layout:
 *     ┌─ Titlebar ────────────────────────────────────────────┐
 *     │ [Refresh]  toolbar (chrome)                           │
 *     ├──────┬─────────┐                                      │
 *     │ Over │ Packets │  tab strip                           │
 *     ├──────┴─────────┴──────────────────────────────────────┤
 *     │  (active tab content)                                 │
 *     ├───────────────────────────────────────────────────────┤
 *     │  TX: 12  RX: 8  │ Link: Up                 statusbar │
 *     └───────────────────────────────────────────────────────┘
 *
 * Uses: libwindow (window + tabs + table + toolbar + statusbar + label),
 *       libnet (network config, stats, packet log)
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "../../system/libraries/libwindow/libwindow.h"
#include "../../system/libraries/libnet/libnet.h"
#include "../../system/libraries/libgui/libgui.h"
#include "../../system/libraries/libfs/libfs.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define WIN_W           560
#define WIN_H           420
#define REFRESH_TICKS   150     /* Refresh every ~3 seconds */

/*=============================================================================
 * GLOBALS
 *===========================================================================*/

static window_t    *g_win          = (window_t *)0;
static toolbar_t   *g_toolbar      = (toolbar_t *)0;
static tabs_t      *g_tabs         = (tabs_t *)0;
static statusbar_t *g_statusbar    = (statusbar_t *)0;

/* Tab indices */
static int g_tab_overview = -1;
static int g_tab_packets  = -1;

/* Overview tab controls */
static label_t *g_lbl_ip       = (label_t *)0;
static label_t *g_lbl_netmask  = (label_t *)0;
static label_t *g_lbl_gateway  = (label_t *)0;
static label_t *g_lbl_dns      = (label_t *)0;
static label_t *g_lbl_mac      = (label_t *)0;
static label_t *g_lbl_link     = (label_t *)0;
static label_t *g_lbl_status   = (label_t *)0;

/* Stats labels */
static label_t *g_lbl_tx       = (label_t *)0;
static label_t *g_lbl_rx       = (label_t *)0;
static label_t *g_lbl_tx_err   = (label_t *)0;
static label_t *g_lbl_rx_err   = (label_t *)0;
static label_t *g_lbl_arp      = (label_t *)0;
static label_t *g_lbl_icmp     = (label_t *)0;

/* Packets tab controls */
static table_t *g_pkt_table    = (table_t *)0;

/* Statusbar panel indices */
static int g_panel_txrx  = -1;
static int g_panel_link  = -1;

/* Tick counter */
static int g_tick_counter = 0;

/*=============================================================================
 * STRING HELPERS
 *===========================================================================*/

static void int_to_str(int val, char *buf, int buflen) {
    if (buflen < 2) { buf[0] = '\0'; return; }
    if (val < 0) { buf[0] = '-'; int_to_str(-val, buf + 1, buflen - 1); return; }
    char tmp[12];
    int i = 0;
    if (val == 0) { tmp[i++] = '0'; }
    else { while (val > 0 && i < 11) { tmp[i++] = '0' + (val % 10); val /= 10; } }
    int j;
    for (j = 0; j < i && j < buflen - 1; j++) buf[j] = tmp[i - 1 - j];
    buf[j] = '\0';
}

static void str_copy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static void str_append(char *dst, const char *src, int max) {
    int len = 0;
    while (dst[len]) len++;
    int i = 0;
    while (len < max - 1 && src[i]) { dst[len++] = src[i++]; }
    dst[len] = '\0';
}

static void str_append_int(char *dst, int val, int max) {
    char tmp[12];
    int_to_str(val, tmp, sizeof(tmp));
    str_append(dst, tmp, max);
}

static void ip_to_str(uint32_t ip, char *buf, int buflen) {
    buf[0] = '\0';
    str_append_int(buf, (ip >> 24) & 0xFF, buflen);
    str_append(buf, ".", buflen);
    str_append_int(buf, (ip >> 16) & 0xFF, buflen);
    str_append(buf, ".", buflen);
    str_append_int(buf, (ip >> 8) & 0xFF, buflen);
    str_append(buf, ".", buflen);
    str_append_int(buf, ip & 0xFF, buflen);
}

static void mac_to_str(const uint8_t *mac, char *buf, int buflen) {
    static const char hex[] = "0123456789ABCDEF";
    if (buflen < 18) { buf[0] = '\0'; return; }
    int pos = 0;
    for (int i = 0; i < 6; i++) {
        buf[pos++] = hex[(mac[i] >> 4) & 0xF];
        buf[pos++] = hex[mac[i] & 0xF];
        if (i < 5) buf[pos++] = ':';
    }
    buf[pos] = '\0';
}

/*=============================================================================
 * UPDATE: Overview tab
 *===========================================================================*/

static void update_overview(void) {
    libnet_config_t config;
    libnet_stats_t stats;
    char buf[64];

    if (libnet_get_config(&config) == 0) {
        /* IP */
        str_copy(buf, "IP Address:   ", sizeof(buf));
        ip_to_str(config.ip_addr, buf + 14, sizeof(buf) - 14);
        if (g_lbl_ip) label_set_text(g_lbl_ip, buf);

        /* Netmask */
        str_copy(buf, "Netmask:      ", sizeof(buf));
        ip_to_str(config.netmask, buf + 14, sizeof(buf) - 14);
        if (g_lbl_netmask) label_set_text(g_lbl_netmask, buf);

        /* Gateway */
        str_copy(buf, "Gateway:      ", sizeof(buf));
        ip_to_str(config.gateway, buf + 14, sizeof(buf) - 14);
        if (g_lbl_gateway) label_set_text(g_lbl_gateway, buf);

        /* DNS */
        str_copy(buf, "DNS:          ", sizeof(buf));
        ip_to_str(config.dns, buf + 14, sizeof(buf) - 14);
        if (g_lbl_dns) label_set_text(g_lbl_dns, buf);

        /* MAC */
        str_copy(buf, "MAC:          ", sizeof(buf));
        mac_to_str(config.mac, buf + 14, sizeof(buf) - 14);
        if (g_lbl_mac) label_set_text(g_lbl_mac, buf);

        /* Link */
        if (g_lbl_link) {
            label_set_text(g_lbl_link,
                config.link_up ? "Link Status:  UP" : "Link Status:  DOWN");
        }

        /* Status */
        if (g_lbl_status) {
            label_set_text(g_lbl_status,
                config.initialized ? "NIC Status:   Initialized" : "NIC Status:   Not Ready");
        }
    }

    if (libnet_get_stats(&stats) == 0) {
        /* TX packets */
        str_copy(buf, "TX Packets:   ", sizeof(buf));
        str_append_int(buf, (int)stats.tx_packets, sizeof(buf));
        if (g_lbl_tx) label_set_text(g_lbl_tx, buf);

        /* RX packets */
        str_copy(buf, "RX Packets:   ", sizeof(buf));
        str_append_int(buf, (int)stats.rx_packets, sizeof(buf));
        if (g_lbl_rx) label_set_text(g_lbl_rx, buf);

        /* TX errors */
        str_copy(buf, "TX Errors:    ", sizeof(buf));
        str_append_int(buf, (int)stats.tx_errors, sizeof(buf));
        if (g_lbl_tx_err) label_set_text(g_lbl_tx_err, buf);

        /* RX errors */
        str_copy(buf, "RX Errors:    ", sizeof(buf));
        str_append_int(buf, (int)stats.rx_errors, sizeof(buf));
        if (g_lbl_rx_err) label_set_text(g_lbl_rx_err, buf);

        /* ARP */
        str_copy(buf, "ARP Sent/Rcvd:", sizeof(buf));
        str_append_int(buf, (int)stats.arp_sent, sizeof(buf));
        str_append(buf, " / ", sizeof(buf));
        str_append_int(buf, (int)stats.arp_received, sizeof(buf));
        if (g_lbl_arp) label_set_text(g_lbl_arp, buf);

        /* ICMP */
        str_copy(buf, "ICMP Sent/Rcv:", sizeof(buf));
        str_append_int(buf, (int)stats.icmp_sent, sizeof(buf));
        str_append(buf, " / ", sizeof(buf));
        str_append_int(buf, (int)stats.icmp_received, sizeof(buf));
        if (g_lbl_icmp) label_set_text(g_lbl_icmp, buf);

        /* Statusbar */
        str_copy(buf, "TX: ", sizeof(buf));
        str_append_int(buf, (int)stats.tx_packets, sizeof(buf));
        str_append(buf, "  RX: ", sizeof(buf));
        str_append_int(buf, (int)stats.rx_packets, sizeof(buf));
        if (g_panel_txrx >= 0) {
            statusbar_set_text(g_statusbar, g_panel_txrx, buf);
        }
    }
}

/*=============================================================================
 * UPDATE: Packets tab
 *===========================================================================*/

static libnet_pkt_log_t g_pkt_log;

static void update_packets(void) {
    if (libnet_get_pkt_log(&g_pkt_log) != 0) return;
    if (!g_pkt_table) return;

    int n = (int)g_pkt_log.returned;
    if (n > 64) n = 64;

    /* Show newest first: reverse order */
    table_set_row_count(g_pkt_table, n);
    for (int i = 0; i < n; i++) {
        int log_idx = n - 1 - i;  /* Newest at top */
        const libnet_pkt_log_entry_t *e = &g_pkt_log.entries[log_idx];
        char buf[32];

        /* Col 0: Direction */
        table_set_cell(g_pkt_table, i, 0,
                       e->direction == LIBNET_PKT_DIR_TX ? "TX" : "RX");

        /* Col 1: Protocol */
        switch (e->protocol) {
            case LIBNET_PKT_PROTO_ARP:  table_set_cell(g_pkt_table, i, 1, "ARP");  break;
            case LIBNET_PKT_PROTO_ICMP: table_set_cell(g_pkt_table, i, 1, "ICMP"); break;
            case LIBNET_PKT_PROTO_IPV4: table_set_cell(g_pkt_table, i, 1, "IPv4"); break;
            default:                    table_set_cell(g_pkt_table, i, 1, "Other"); break;
        }

        /* Col 2: Size */
        int_to_str((int)e->length, buf, sizeof(buf));
        table_set_cell(g_pkt_table, i, 2, buf);

        /* Col 3: Summary */
        table_set_cell(g_pkt_table, i, 3, e->summary);
    }

    g_pkt_table->base.dirty = 1;
}

/*=============================================================================
 * TOOLBAR CALLBACKS
 *===========================================================================*/

static void on_refresh_click(void *userdata) {
    (void)userdata;
    update_overview();
    update_packets();
    if (g_win) window_invalidate(g_win);
}

/*=============================================================================
 * TICK CALLBACK (auto-refresh)
 *===========================================================================*/

static void on_tick(window_t *win, void *userdata) {
    (void)userdata;
    g_tick_counter++;
    if (g_tick_counter >= REFRESH_TICKS) {
        g_tick_counter = 0;
        update_overview();
        update_packets();
        window_invalidate(win);
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    /* Center window on screen */
    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    int win_x = (scr_w - WIN_W) / 2;
    int win_y = (scr_h - WIN_H) / 2;

    window_t *win = window_create("Network Explorer", win_x, win_y, WIN_W, WIN_H);
    if (!win) return;
    g_win = win;

    /* Load titlebar icon */
    {
        static uint8_t icon_buf[4096];
        int sz = libfs_read_file("C:/icons/", "DEFAULT.BMP", icon_buf, sizeof(icon_buf));
        if (sz > 0) window_set_icon(win, icon_buf, sz);
    }

    /* ---- Toolbar ---- */
    g_toolbar = toolbar_create(0, 0, WIN_W);
    if (g_toolbar) {
        toolbar_add_button(g_toolbar, "Refresh", on_refresh_click, (void *)0);
        window_add_control(win, &g_toolbar->base);
    }

    int toolbar_h = g_toolbar ? g_toolbar->base.height : 0;

    /* ---- Statusbar ---- */
    g_statusbar = statusbar_create(0, 0, WIN_W);
    if (g_statusbar) {
        g_panel_txrx = statusbar_add_panel(g_statusbar, "TX: 0  RX: 0", 200);
        g_panel_link = statusbar_add_panel(g_statusbar, "Link: --", 120);
        window_add_control(win, &g_statusbar->base);
    }

    int statusbar_h = g_statusbar ? g_statusbar->base.height : 0;

    /* ---- Tab control ---- */
    int tabs_y = toolbar_h;
    int tabs_h = WIN_H - THEME_TITLEBAR_HEIGHT - toolbar_h - statusbar_h;
    if (tabs_h < 100) tabs_h = 100;

    g_tabs = tabs_create(0, tabs_y, WIN_W, tabs_h);
    if (g_tabs) {
        g_tab_overview = tabs_add_tab(g_tabs, "Overview");
        g_tab_packets  = tabs_add_tab(g_tabs, "Packets");

        /* ---- Overview tab children ---- */
        /* Section: Network Interface */
        label_t *hdr1 = label_create(8, 4, "== Network Interface ==", THEME_ACCENT);
        if (hdr1) tabs_add_child(g_tabs, g_tab_overview, &hdr1->base);

        g_lbl_ip      = label_create(8, 24, "IP Address:   ...", THEME_TEXT);
        g_lbl_netmask = label_create(8, 42, "Netmask:      ...", THEME_TEXT);
        g_lbl_gateway = label_create(8, 60, "Gateway:      ...", THEME_TEXT);
        g_lbl_dns     = label_create(8, 78, "DNS:          ...", THEME_TEXT);
        g_lbl_mac     = label_create(8, 96, "MAC:          ...", THEME_TEXT);
        g_lbl_link    = label_create(8, 114, "Link Status:  ...", THEME_TEXT);
        g_lbl_status  = label_create(8, 132, "NIC Status:   ...", THEME_TEXT);

        if (g_lbl_ip)      tabs_add_child(g_tabs, g_tab_overview, &g_lbl_ip->base);
        if (g_lbl_netmask) tabs_add_child(g_tabs, g_tab_overview, &g_lbl_netmask->base);
        if (g_lbl_gateway) tabs_add_child(g_tabs, g_tab_overview, &g_lbl_gateway->base);
        if (g_lbl_dns)     tabs_add_child(g_tabs, g_tab_overview, &g_lbl_dns->base);
        if (g_lbl_mac)     tabs_add_child(g_tabs, g_tab_overview, &g_lbl_mac->base);
        if (g_lbl_link)    tabs_add_child(g_tabs, g_tab_overview, &g_lbl_link->base);
        if (g_lbl_status)  tabs_add_child(g_tabs, g_tab_overview, &g_lbl_status->base);

        /* Section: Traffic Statistics */
        label_t *hdr2 = label_create(8, 158, "== Traffic Statistics ==", THEME_ACCENT);
        if (hdr2) tabs_add_child(g_tabs, g_tab_overview, &hdr2->base);

        g_lbl_tx     = label_create(8, 178, "TX Packets:   0", THEME_TEXT);
        g_lbl_rx     = label_create(8, 196, "RX Packets:   0", THEME_TEXT);
        g_lbl_tx_err = label_create(8, 214, "TX Errors:    0", THEME_TEXT);
        g_lbl_rx_err = label_create(8, 232, "RX Errors:    0", THEME_TEXT);
        g_lbl_arp    = label_create(8, 250, "ARP Sent/Rcvd:0 / 0", THEME_TEXT);
        g_lbl_icmp   = label_create(8, 268, "ICMP Sent/Rcv:0 / 0", THEME_TEXT);

        if (g_lbl_tx)     tabs_add_child(g_tabs, g_tab_overview, &g_lbl_tx->base);
        if (g_lbl_rx)     tabs_add_child(g_tabs, g_tab_overview, &g_lbl_rx->base);
        if (g_lbl_tx_err) tabs_add_child(g_tabs, g_tab_overview, &g_lbl_tx_err->base);
        if (g_lbl_rx_err) tabs_add_child(g_tabs, g_tab_overview, &g_lbl_rx_err->base);
        if (g_lbl_arp)    tabs_add_child(g_tabs, g_tab_overview, &g_lbl_arp->base);
        if (g_lbl_icmp)   tabs_add_child(g_tabs, g_tab_overview, &g_lbl_icmp->base);

        /* ---- Packets tab children ---- */
        int tbl_w = WIN_W - 12;
        int tbl_h = tabs_h - TABS_HEADER_H - TABS_BORDER * 2 - 12;
        g_pkt_table = table_create(4, 4, tbl_w, tbl_h);
        if (g_pkt_table) {
            table_add_column(g_pkt_table, "Dir", 36, TABLE_ALIGN_CENTER);
            table_add_column(g_pkt_table, "Proto", 52, TABLE_ALIGN_LEFT);
            table_add_column(g_pkt_table, "Size", 48, TABLE_ALIGN_RIGHT);
            table_add_column(g_pkt_table, "Summary", tbl_w - 36 - 52 - 48 - 30,
                             TABLE_ALIGN_LEFT);
            tabs_add_child(g_tabs, g_tab_packets, &g_pkt_table->base);
        }

        window_add_control(win, &g_tabs->base);
    }

    /* Set tick handler for auto-refresh */
    window_set_on_tick(win, on_tick, (void *)0);

    /* Initial data load */
    update_overview();
    update_packets();

    /* Update link status in statusbar */
    {
        libnet_config_t cfg;
        if (libnet_get_config(&cfg) == 0) {
            if (g_panel_link >= 0) {
                statusbar_set_text(g_statusbar, g_panel_link,
                    cfg.link_up ? "Link: Up" : "Link: Down");
            }
        }
    }

    /* Run the event loop */
    window_run(win);

    /* Cleanup */
    window_destroy(win);
}
