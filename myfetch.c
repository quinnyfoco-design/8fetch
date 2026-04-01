/*
 * myfetch.c — C port of myfetch (neofetch-like system info tool)
 *
 * Compile: gcc -O2 -o myfetch myfetch.c
 * Usage: ./myfetch [--grey] [--color:#rrggbb]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <glob.h>

#define MAX_ASCII 28
#define LINE_LEN 512
#define MAX_INFO 20
#define LEFT_WIDTH 40

static const char *RED, *SRED;
static const char *STEELBLUE, *STEELBLUE_LT;
static const char *SYELLOW, *SBLACK, *SBRIGHTBLUE, *PURPLE, *BLUE, *SBLUE;
static const char *TEAL, *TEAL_DIM, *WHITE, *ORANGE, *SORANGE, *GREEN, *SGREEN, *RESET_C;
static const char *label_color;
static const char *INFO_COLOR = "";

static char g_color_seq[32]      = "";
static char g_color_seq_bold[32] = "";

static bool parse_hex_color(const char *hex) {
    if (!hex || !*hex) return false;
    if (*hex == '#') hex++;
    if (strlen(hex) != 6) return false;
    unsigned int r, g, b;
    if (sscanf(hex, "%2x%2x%2x", &r, &g, &b) != 3) return false;
    snprintf(g_color_seq,      sizeof(g_color_seq),      "\033[38;2;%u;%u;%um",   r, g, b);
    snprintf(g_color_seq_bold, sizeof(g_color_seq_bold), "\033[1;38;2;%u;%u;%um", r, g, b);
    return true;
}

static void init_colors(bool grey, const char *color_hex) {
    if (grey) {
        SYELLOW=SBLACK=SBRIGHTBLUE=PURPLE=BLUE=SBLUE="";
        TEAL=TEAL_DIM=WHITE=ORANGE=SORANGE=GREEN=SGREEN=RESET_C="";
        RED=SRED=STEELBLUE=STEELBLUE_LT="";
    } else if (color_hex && parse_hex_color(color_hex)) {
        const char *c = g_color_seq;
        SYELLOW=SBLACK=SBRIGHTBLUE=PURPLE=BLUE=SBLUE=c;
        TEAL=TEAL_DIM=WHITE=ORANGE=SORANGE=GREEN=SGREEN=c;
        RED=SRED=STEELBLUE=STEELBLUE_LT=c;
        RESET_C  = "\033[0m";
        INFO_COLOR = c;
    } else {
        SYELLOW    = "\033[33m";
        SBLACK     = "\033[39m";
        SBRIGHTBLUE= "\033[38;5;153m";
        PURPLE     = "\033[35m";
        BLUE       = "\033[1;34m";
        SBLUE      = "\033[34m";
        TEAL       = "\033[38;5;43m";
        TEAL_DIM   = "\033[36m";
        WHITE      = "\033[1;37m";
        ORANGE     = "\033[1;38;5;208m";
        SORANGE    = "\033[38;5;208m";
        GREEN      = "\033[1;32m";
        SGREEN     = "\033[32m";
        RESET_C    = "\033[0m";
        RED        = "\033[1;31m";
        SRED       = "\033[31m";
        STEELBLUE  = "\033[38;5;68m";
        STEELBLUE_LT= "\033[38;5;111m";
    }
}

static char ascii_lines[MAX_ASCII][LINE_LEN];
static int  n_ascii = 0;

static char info_lines[MAX_INFO][LINE_LEN];
static int  n_info = 0;

static char g_os[256]       = "";
static char g_kernel[256]   = "";
static char g_device[256]   = "";
static char g_uptime[256]   = "";
static char g_terminal[128] = "";
static char g_disp1[64]     = "";
static char g_disp2[64]     = "";
static char g_cpu[256]      = "";
static char g_gpu[256]      = "";
static char g_gpu2[256]     = "";
static char g_ram[128]      = "";
static char g_shell[128]    = "";
static char g_wm[128]       = "";
static char g_de[128]       = "";
static char g_distro_id[64]   = "";
static char g_distro_base[64] = "";
static bool g_is_proxmox = false;
static bool g_is_android = false;

#define AL(...) snprintf(ascii_lines[n_ascii++], LINE_LEN, __VA_ARGS__)
#define ALN()   (ascii_lines[n_ascii++][0] = '\0')

/* run_cmd: kept for things that truly need a subprocess (hyprctl, xrandr, xprop, pveversion) */
static void run_cmd(const char *cmd, char *out, size_t sz) {
    out[0] = '\0';
    FILE *fp = popen(cmd, "r");
    if (!fp) return;
    if (!fgets(out, sz, fp)) out[0] = '\0';
    pclose(fp);
    size_t n = strlen(out);
    while (n && (out[n-1]=='\n'||out[n-1]=='\r'||out[n-1]==' ')) out[--n]='\0';
}

static void os_field(const char *key, char *out, size_t sz) {
    out[0] = '\0';
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp) return;
    char line[512];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, klen)==0 && line[klen]=='=') {
            char *v = line + klen + 1;
            size_t n = strlen(v);
            while (n && (v[n-1]=='\n'||v[n-1]=='\r'||v[n-1]=='"')) v[--n]='\0';
            if (v[0]=='"') { v++; n--; }
            strncpy(out, v, sz-1); out[sz-1]='\0';
            fclose(fp); return;
        }
    }
    fclose(fp);
}

static size_t vis_len(const char *s) {
    size_t len = 0;
    bool esc = false;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\033') { esc = true; continue; }
        if (esc) { if (isalpha(*p)) esc = false; continue; }
        if ((*p & 0xC0) != 0x80) len++;
    }
    return len;
}

/* -----------------------------------------------------------------------
 * FAST: read CPU model + MHz directly from /proc/cpuinfo
 * Replaces two slow lscpu|grep/awk subprocesses
 * ----------------------------------------------------------------------- */
static void gather_cpu(void) {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) { strcpy(g_cpu, "unknown"); return; }

    char model[200] = "";
    float max_mhz = 0.0f;
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        /* model name — grab first occurrence */
        if (!model[0] && strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ') colon++;
                size_t n = strlen(colon);
                while (n && (colon[n-1]=='\n'||colon[n-1]=='\r'||colon[n-1]==' ')) colon[--n]='\0';
                strncpy(model, colon, sizeof(model)-1);
            }
        }
        /* cpu MHz — track max across all cores */
        if (strncmp(line, "cpu MHz", 7) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                float mhz = 0.0f;
                if (sscanf(colon+1, "%f", &mhz) == 1 && mhz > max_mhz)
                    max_mhz = mhz;
            }
        }
    }
    fclose(fp);

    if (!model[0]) strcpy(model, "unknown");

    if (max_mhz > 0.0f)
        snprintf(g_cpu, sizeof(g_cpu), "%s @ %.1f GHz", model, max_mhz / 1000.0f);
    else
        snprintf(g_cpu, sizeof(g_cpu), "%s @ unknown", model);
}

/* -----------------------------------------------------------------------
 * FAST: read RAM from /proc/meminfo
 * Replaces two slow `free -h | awk` subprocesses
 * ----------------------------------------------------------------------- */
static void gather_ram(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) { strcpy(g_ram, "unknown"); return; }

    long total_kb = 0, avail_kb = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0)
            sscanf(line + 9, "%ld", &total_kb);
        else if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line + 13, "%ld", &avail_kb);
        if (total_kb && avail_kb) break;
    }
    fclose(fp);

    long used_kb = total_kb - avail_kb;

    /* Format like `free -h`: show in MiB or GiB */
    auto_format: ;
    double used  = used_kb  / 1024.0;
    double total = total_kb / 1024.0;

    if (total >= 1024.0)
        snprintf(g_ram, sizeof(g_ram), "%.1f GiB / %.1f GiB",
                 used / 1024.0, total / 1024.0);
    else
        snprintf(g_ram, sizeof(g_ram), "%.0f MiB / %.0f MiB", used, total);
}

/* -----------------------------------------------------------------------
 * FAST: read uptime from /proc/uptime
 * Replaces `uptime -p` subprocess
 * ----------------------------------------------------------------------- */
static void gather_uptime(void) {
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) { strcpy(g_uptime, "unknown"); return; }

    double secs = 0.0;
    if (fscanf(fp, "%lf", &secs) != 1) secs = 0.0;
    fclose(fp);

    long total = (long)secs;
    int days  = total / 86400;
    int hours = (total % 86400) / 3600;
    int mins  = (total % 3600)  / 60;

    if (days > 0)
        snprintf(g_uptime, sizeof(g_uptime), "%d day%s, %d hour%s, %d min%s",
                 days,  days  != 1 ? "s" : "",
                 hours, hours != 1 ? "s" : "",
                 mins,  mins  != 1 ? "s" : "");
    else if (hours > 0)
        snprintf(g_uptime, sizeof(g_uptime), "%d hour%s, %d min%s",
                 hours, hours != 1 ? "s" : "",
                 mins,  mins  != 1 ? "s" : "");
    else
        snprintf(g_uptime, sizeof(g_uptime), "%d min%s",
                 mins, mins != 1 ? "s" : "");
}

/* -----------------------------------------------------------------------
 * FAST: GPU via single lspci call (still needs lspci but ONE popen)
 * Original code opened TWO separate popen calls; now we do it in one.
 * ----------------------------------------------------------------------- */
static void gather_gpu(void) {
#define CLEAN_GPU(src, dst, dsz) do { \
    char _g[512]; snprintf(_g, sizeof(_g), "%s", (src)); \
    char *_r = strstr(_g, " (rev "); if (_r) *_r = '\0'; \
    char *_p; \
    while ((_p = strstr(_g, "Corporation ")) != NULL) \
        memmove(_p, _p+12, strlen(_p+12)+1); \
    while ((_p = strstr(_g, "Technologies ")) != NULL) \
        memmove(_p, _p+13, strlen(_p+13)+1); \
    char *_br = strchr(_g, '['); \
    if (_br) { \
        char *_cl = strchr(_br+1, ']'); \
        if (_cl) *_cl = '\0'; \
        memmove(_g, _br+1, strlen(_br+1)+1); \
    } \
    size_t _n = strlen(_g); \
    while (_n && (_g[_n-1]=='\n'||_g[_n-1]=='\r'||_g[_n-1]==' ')) \
        _g[--_n] = '\0'; \
    snprintf((dst), (dsz), "%s", _g); \
} while(0)

    char raw_a[512] = "", raw_b[512] = "";

    /* Single popen — read first two matching lines at once */
    FILE *lp = popen("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display'", "r");
    if (lp) {
        char line[512];
        if (fgets(line, sizeof(line), lp)) {
            char *af = strstr(line, ": ");
            if (af) snprintf(raw_a, sizeof(raw_a), "%s", af + 2);
        }
        if (fgets(line, sizeof(line), lp)) {
            char *af = strstr(line, ": ");
            if (af) snprintf(raw_b, sizeof(raw_b), "%s", af + 2);
        }
        pclose(lp);
    }

    char name_a[256] = "", name_b[256] = "";
    if (raw_a[0]) CLEAN_GPU(raw_a, name_a, sizeof(name_a));
    if (raw_b[0]) CLEAN_GPU(raw_b, name_b, sizeof(name_b));

    const char *dri  = getenv("DRI_PRIME");
    bool prime = dri && dri[0] != '\0' && strcmp(dri, "0") != 0;
    if (prime && name_b[0]) {
        char tmp[256];
        snprintf(tmp,    sizeof(tmp),    "%s", name_a);
        snprintf(name_a, sizeof(name_a), "%s", name_b);
        snprintf(name_b, sizeof(name_b), "%s", tmp);
    }

    if (name_a[0]) snprintf(g_gpu,  sizeof(g_gpu),  "%s", name_a);
    else           strcpy(g_gpu, "unknown");
    if (name_b[0]) snprintf(g_gpu2, sizeof(g_gpu2), "%s", name_b);
#undef CLEAN_GPU
}

static void gather(void) {

    /* Termux / Android detection */
    {
        const char *pfx = getenv("PREFIX");
        if (pfx && strstr(pfx, "com.termux")) {
            g_is_android = true;
        } else {
            FILE *bp = fopen("/system/build.prop", "r");
            if (bp) { fclose(bp); g_is_android = true; }
        }
        if (g_is_android)
            snprintf(g_os, sizeof(g_os), "Android (Termux)");
    }

    {
        char buf[256];
        run_cmd("pveversion 2>/dev/null | head -n1", buf, sizeof(buf));
        if (buf[0]) {
            char *sl = strchr(buf, '/');
            if (sl) {
                char *ver  = sl + 1;
                char *dash = strchr(ver, '-');
                if (dash) *dash = '\0';
                snprintf(g_os, sizeof(g_os), "Proxmox VE %s", ver);
                g_is_proxmox = true;
            }
        }
    }

    os_field("ID",      g_distro_id,   sizeof(g_distro_id));
    os_field("ID_LIKE", g_distro_base, sizeof(g_distro_base));

    if (!g_is_proxmox && !g_is_android) {
        char name[128] = "Linux", ver[128] = "";
        os_field("NAME",    name, sizeof(name));
        os_field("VERSION", ver,  sizeof(ver));
        if (ver[0]) snprintf(g_os, sizeof(g_os), "%s %s", name, ver);
        else        snprintf(g_os, sizeof(g_os), "%s", name);
    }

    {
        struct utsname u;
        if (uname(&u) == 0) strncpy(g_kernel, u.release, sizeof(g_kernel)-1);
        else                 strcpy(g_kernel, "unknown");
    }

    {
        FILE *fp = fopen("/sys/devices/virtual/dmi/id/product_name", "r");
        if (fp) {
            if (!fgets(g_device, sizeof(g_device), fp)) strcpy(g_device, "Unknown");
            else {
                size_t n = strlen(g_device);
                while (n && (g_device[n-1]=='\n'||g_device[n-1]=='\r')) g_device[--n]='\0';
            }
            fclose(fp);
        } else strcpy(g_device, "Unknown");
    }

    /* FAST uptime from /proc/uptime */
    gather_uptime();

    {
        const char *t = getenv("TERM");
        strncpy(g_terminal, t ? t : "unknown", sizeof(g_terminal)-1);
    }

    {
        bool got_res = false;

        if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
            FILE *hf = popen("hyprctl monitors 2>/dev/null", "r");
            if (hf) {
                char line[512];
                int nd = 0;
                while (fgets(line, sizeof(line), hf) && nd < 2) {
                    char *at = strstr(line, "\t");
                    if (!at) at = line;
                    int w, h;
                    if (sscanf(at, " %dx%d@", &w, &h) == 2) {
                        if (nd == 0) { snprintf(g_disp1, sizeof(g_disp1), "%dx%d", w, h); got_res = true; }
                        else          snprintf(g_disp2, sizeof(g_disp2), "%dx%d", w, h);
                        nd++;
                    }
                }
                pclose(hf);
            }
        }

        if (!got_res) {
            FILE *xr = popen("xrandr 2>/dev/null", "r");
            if (xr) {
                char res[2][64]    = {"",""};
                bool is_primary[2] = {false, false};
                int nd = 0;
                char line[512];
                while (fgets(line, sizeof(line), xr) && nd < 2) {
                    if (!strstr(line, " connected")) continue;
                    bool primary = strstr(line, " primary") != NULL;
                    for (char *p = line; *p; p++) {
                        int w, h, ox, oy;
                        if (sscanf(p, "%dx%d+%d+%d", &w, &h, &ox, &oy) == 4) {
                            snprintf(res[nd], sizeof(res[nd]), "%dx%d", w, h);
                            is_primary[nd] = primary;
                            nd++;
                            break;
                        }
                    }
                }
                pclose(xr);
                if (nd > 0) {
                    int pri = 0;
                    for (int k = 0; k < nd; k++) if (is_primary[k]) { pri = k; break; }
                    snprintf(g_disp1, sizeof(g_disp1), "%s", res[pri]);
                    for (int k = 0; k < nd; k++)
                        if (k != pri) { snprintf(g_disp2, sizeof(g_disp2), "%s", res[k]); break; }
                    got_res = true;
                }
            }
        }

        if (!got_res) {
            glob_t gl;
            if (glob("/sys/class/drm/card*-*/mode", 0, NULL, &gl) == 0) {
                int nd = 0;
                for (size_t gi = 0; gi < gl.gl_pathc && nd < 2; gi++) {
                    char spath[512];
                    snprintf(spath, sizeof(spath), "%s", gl.gl_pathv[gi]);
                    char *sl = strrchr(spath, '/');
                    if (sl) strcpy(sl + 1, "status");
                    char status[32] = "";
                    FILE *sf = fopen(spath, "r");
                    if (!sf) continue;
                    if (!fgets(status, sizeof(status), sf)) { fclose(sf); continue; }
                    fclose(sf);
                    size_t sn = strlen(status);
                    while (sn && (status[sn-1]=='\n'||status[sn-1]=='\r')) status[--sn]='\0';
                    if (strcmp(status, "connected") != 0) continue;
                    char res[64] = "";
                    FILE *mf = fopen(gl.gl_pathv[gi], "r");
                    if (!mf) continue;
                    if (!fgets(res, sizeof(res), mf)) { fclose(mf); continue; }
                    fclose(mf);
                    size_t rn = strlen(res);
                    while (rn && (res[rn-1]=='\n'||res[rn-1]=='\r')) res[--rn]='\0';
                    if (!res[0]) continue;
                    if (nd == 0) snprintf(g_disp1, sizeof(g_disp1), "%s", res);
                    else         snprintf(g_disp2, sizeof(g_disp2), "%s", res);
                    nd++;
                }
                globfree(&gl);
            }
        }

        if (!g_disp1[0]) strcpy(g_disp1, "unknown");
    }

    /* FAST CPU from /proc/cpuinfo */
    gather_cpu();

    /* FAST GPU — single lspci popen */
    gather_gpu();

    /* FAST RAM from /proc/meminfo */
    gather_ram();

    {
        const char *sh = getenv("SHELL");
        if (sh) {
            const char *base = strrchr(sh, '/');
            snprintf(g_shell, sizeof(g_shell), "%s", base ? base + 1 : sh);
        } else strcpy(g_shell, "unknown");
    }

    {
        if (getenv("HYPRLAND_INSTANCE_SIGNATURE"))
            snprintf(g_wm, sizeof(g_wm), "Hyprland");
        else if (getenv("SWAYSOCK"))
            snprintf(g_wm, sizeof(g_wm), "Sway");
        else if (getenv("i3SOCK"))
            snprintf(g_wm, sizeof(g_wm), "i3");
        else if (getenv("NIRI_SOCKET"))
            snprintf(g_wm, sizeof(g_wm), "Niri");

        if (!g_wm[0]) {
            const char *de = getenv("XDG_CURRENT_DESKTOP");
            if (!de || !de[0]) de = getenv("DESKTOP_SESSION");
            if (!de || !de[0]) {
                if      (getenv("GNOME_DESKTOP_SESSION_ID")) de = "GNOME";
                else if (getenv("KDE_FULL_SESSION"))         de = "KDE";
                else if (getenv("MATE_DESKTOP_SESSION_ID")) de = "MATE";
            }
            if (de && de[0])
                snprintf(g_de, sizeof(g_de), "%s", de);
            else {
                run_cmd(
                    "xprop -id "
                    "$(xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null "
                    " | awk '{print $NF}') "
                    "_NET_WM_NAME 2>/dev/null "
                    "| grep -oP '(?<=\")[^\"]+(?=\")' | head -1",
                    g_wm, sizeof(g_wm));
            }
        }
    }
}

static void choose_label_color(void) {
    if (g_is_proxmox) { label_color = ORANGE; return; }
    if (g_is_android) { label_color = GREEN;  return; }
    if (!strcmp(g_distro_id,"arch") || !strcmp(g_distro_id,"endeavouros") ||
        !strcmp(g_distro_id,"manjaro") || !strcmp(g_distro_id,"garuda"))
        { label_color = BLUE; return; }
    if (!strcmp(g_distro_id,"artix"))
        { label_color = BLUE; return; }
    if (!strcmp(g_distro_id,"ubuntu"))    { label_color = ORANGE; return; }
    if (!strcmp(g_distro_id,"linuxmint")) { label_color = GREEN;  return; }
    if (!strcmp(g_distro_id,"pop"))       { label_color = TEAL;   return; }
    if (!strcmp(g_distro_id,"cachyos"))   { label_color = TEAL;   return; }
    if (!strcmp(g_distro_id,"void"))      { label_color = GREEN;  return; }
    if (!strcmp(g_distro_id,"nixos"))     { label_color = BLUE;   return; }
    if (!strcmp(g_distro_id,"debian"))    { label_color = SRED;   return; }
    if (!strcmp(g_distro_id,"gentoo"))    { label_color = STEELBLUE; return; }
    if (strstr(g_distro_id,"opensuse"))   { label_color = GREEN;  return; }
    if (strstr(g_distro_base,"suse") || strstr(g_distro_base,"opensuse"))
        { label_color = GREEN; return; }
    if (strstr(g_distro_base,"arch"))
        { label_color = BLUE; return; }
    if (strstr(g_distro_base,"ubuntu") || strstr(g_distro_base,"debian"))
        { label_color = SRED; return; }
    label_color = PURPLE;
}

static void build_ascii_proxmox(void) {
    const char *o=ORANGE, *w=WHITE, *r=RESET_C;
    AL("    %s@@@@%s    %s@@@@%s",       w,r,w,r);
    AL("  %s++++%s  %s@@@@@@@%s  %s++++%s",  o,r,w,r,o,r);
    AL("  %s++++%s    %s@@@%s    %s++++%s",  o,r,w,r,o,r);
    AL("   %s++++ ++++%s",               o,r);
    AL("   %s++++%s%s@@@%s%s++++%s",         o,r,w,r,o,r);
    AL("  %s++++%s  %s@@@@@%s  %s++++%s",    o,r,w,r,o,r);
    AL(" %s+++%s  %s@@@@%s  %s@@@@%s  %s+++%s", o,r,w,r,w,r,o,r);
    AL("    %s@@@%s      %s@@@%s",       w,r,w,r);
    ALN();
}

static void build_ascii_android(void) {
    const char *g=SGREEN, *r=RESET_C;
    AL("          %s-o    o-%s",                         g,r);
    AL("        %s+hydNNNNdyh+%s",                       g,r);
    AL("      %s+mMMMMMMMMMMMMm+%s",                     g,r);
    AL("    %sdMMm:NMMMMMMN:mMMd%s",                     g,r);
    AL("   %shMMMMMMMMMMMMMMMMMMh%s",                     g,r);
    AL("  %s.. yyyyyyyyyyyyyyyyyyyy ..%s",                g,r);
    AL("%s.mMMm MMMMMMMMMMMMMMMMMMMM mMMm.%s",           g,r);
    AL("%s:MMMM-MMMMMMMMMMMMMMMMMMMM-MMMM:%s",           g,r);
    AL("%s:MMMM-MMMMMMMMMMMMMMMMMMMM-MMMM:%s",           g,r);
    AL("%s:MMMM-MMMMMMMMMMMMMMMMMMMM-MMMM:%s",           g,r);
    AL("%s:MMMM-MMMMMMMMMMMMMMMMMMMM-MMMM:%s",           g,r);
    AL("%s-MMMM-MMMMMMMMMMMMMMMMMMMM-MMMM-%s",           g,r);
    AL(" %s+yy+ MMMMMMMMMMMMMMMMMMMM +yy+%s",            g,r);
    AL("      %smMMMMMMMMMMMMMMMMMMm%s",                  g,r);
    AL("      %s/++MMMMh++hMMMM++/%s",                   g,r);
    AL("         %sMMMMo    oMMMM%s",                    g,r);
    AL("         %sMMMMo    oMMMM%s",                    g,r);
    AL("          %soNMm-  -mMNs%s",                     g,r);
}

static void build_ascii_artix(void) {
    const char *b=SBLUE, *r=RESET_C;
    ALN();
    AL("         %s+%s",                         b,r);
    AL("        %s=++%s",                        b,r);
    AL("       %s=+=+=%s",                       b,r);
    AL("      %s-+===+-%s",                      b,r);
    AL("     %s-++====+-%s",                     b,r);
    AL("    %s:+++-====+:%s",                    b,r);
    AL("   %s=+***+===++.%s",                    b,r);
    AL("   %s:****==++.%s",                      b,r);
    AL("  %s.+. .***=++.%s",                     b,r);
    AL(" %s+++=++-   +*++%s",                    b,r);
    AL(" %s.=++=====++++ -+.%s",                 b,r);
    AL(" %s++++=======++++++.%s",                b,r);
    AL(" %s=+++=======-+++++++=.%s",             b,r);
    AL(" %s=+++======+****+: :+=%s",             b,r);
    AL(" %s-+++====+***=.   -++==+-%s",          b,r);
    AL(" %s-+++==***: =+++++===+-%s",            b,r);
    AL("  %s:+++*+. :+**+=+.%s",                 b,r);
    AL("   %s:+-   .=+:%s",                      b,r);
    ALN();
}

static void build_ascii_arch(void) {
    const char *b=SBLUE, *r=RESET_C;
    AL("                  %s-'%s",                b,r);
    AL("                 %s.o+'%s",               b,r);
    AL("                %s'ooo/%s",               b,r);
    AL("               %s'+oooo:%s",              b,r);
    AL("              %s'+oooooo:%s",             b,r);
    AL("              %s-+oooooo+:%s",            b,r);
    AL("            %s'/:-:++oooo+:%s",           b,r);
    AL("           %s'/++++/+++++++:%s",          b,r);
    AL("          %s'/++++++++++++++:%s",         b,r);
    AL("         %s'/+++ooooooooooooo/'%s",       b,r);
    AL("        %s./ooosssso++osssssso+'%s",      b,r);
    AL("       %s.oossssso-''''/ossssss+'%s",     b,r);
    AL("      %s-osssssso.      :ssssssso.%s",    b,r);
    AL("     %s:osssssss/        osssso+++.%s",   b,r);
    AL("    %s/ossssssss/         +ssssooo/-%s",  b,r);
    AL("  %s'/ossssso+/:-          -:/+osssso+-%s", b,r);
    AL(" %s'+sso+:-'                  '.-/+oso:%s", b,r);
    AL(" %s'++:.                          '-/+/%s", b,r);
    AL("  %s.'                               '%s",  b,r);
}

static void build_ascii_ubuntu(void) {
    const char *o=SORANGE, *r=RESET_C;
    ALN();
    AL("          %s===========%s",               o,r);
    AL("      %s===================%s",           o,r);
    AL("    %s=======================%s",         o,r);
    AL("   %s================= =====%s",          o,r);
    AL("  %s========== = ======%s",               o,r);
    AL(" %s========= -= ===========%s",           o,r);
    AL(" %s======== .=======. ========%s",        o,r);
    AL(" %s======== =========== ========%s",      o,r);
    AL(" %s==== = ============= :=======%s",      o,r);
    AL(" %s=== = ========================%s",     o,r);
    AL(" %s==== = ============= :=======%s",      o,r);
    AL(" %s======== =========== ========%s",      o,r);
    AL(" %s======== .=======: ========%s",        o,r);
    AL(" %s========= -= ===========%s",           o,r);
    AL("  %s========== = ======%s",               o,r);
    AL("   %s================= =====%s",          o,r);
    AL("    %s=======================%s",         o,r);
    AL("      %s===================%s",           o,r);
    AL("          %s===========%s",               o,r);
    ALN(); ALN();
}

static void build_ascii_mint(void) {
    const char *sg=SGREEN, *r=RESET_C;
    ALN();
    AL("   %s===============%s",                  sg,r);
    AL("   %s=====================%s",            sg,r);
    AL("   %s=========================%s",        sg,r);
    AL("   %s===%s...%s=====================%s",  sg,r,sg,r);
    AL("   %s====%s...%s=====%s.....%s=%s.....%s======%s", sg,r,sg,r,sg,r,sg,r);
    AL("   %s=====%s...%s===%s...............%s=====%s", sg,r,sg,r,sg,r);
    AL("   %s======%s...%s===%s...%s===%s...%s===%s...%s======%s", sg,r,sg,r,sg,r,sg,r,sg,r);
    AL("   %s======%s...%s===%s...%s===%s...%s===%s...%s======%s", sg,r,sg,r,sg,r,sg,r,sg,r);
    AL("   %s======%s...%s===%s...%s===%s...%s===%s...%s======%s", sg,r,sg,r,sg,r,sg,r,sg,r);
    AL("   %s======%s...%s===%s...%s===%s...%s===%s...%s======%s", sg,r,sg,r,sg,r,sg,r,sg,r);
    AL("   %s======%s...%s===%s...%s===%s...%s===%s...%s======%s", sg,r,sg,r,sg,r,sg,r,sg,r);
    AL("   %s=====%s...%s===============%s...%s=====%s", sg,r,sg,r,sg,r);
    AL("   %s=====%s...................%s=====%s", sg,r,sg,r);
    AL("   %s======%s...............%s======%s",  sg,r,sg,r);
    AL("   %s=========================%s",        sg,r);
    AL("   %s=====================%s",            sg,r);
    AL("   %s===============%s",                  sg,r);
    ALN(); ALN();
}

static void build_ascii_pop(void) {
    const char *sb=SBRIGHTBLUE, *r=RESET_C;
    ALN();
    AL("   %s.++++++++++++*.  %s",                sb,r);
    AL("  %s.++++++++++++++*****.%s",             sb,r);
    AL("  %s.+++%s.........%s++*********.%s",     sb,r,sb,r);
    AL("  %s+++%s.....%s+-%s.....%s+***********.%s", sb,r,sb,r,sb,r);
    AL("  %s++++%s.....%s+++%s.....%s************%s", sb,r,sb,r,sb,r);
    AL("  %s++++++%s.....%s+**%s....%s***%s....%s******%s", sb,r,sb,r,sb,r,sb,r);
    AL("  %s.+++++++%s.....%s*=%s...%s+**%s.....%s******.%s", sb,r,sb,r,sb,r,sb,r);
    AL("  %s+++++++++%s........%s****%s....%s********%s", sb,r,sb,r,sb,r);
    AL("  %s++++++++**%s.....%s******%s...%s*********%s", sb,r,sb,r,sb,r);
    AL("  %s++++++*****%s....%s******%s..%s**********%s", sb,r,sb,r,sb,r);
    AL("  %s.+++*******%s....%s****************.%s", sb,r,sb,r);
    AL("  %s+***********%s....%s***%s..%s**********%s", sb,r,sb,r,sb,r);
    AL("  %s*****************************%s",     sb,r);
    AL("  %s*****%s.................%s*****%s",   sb,r,sb,r);
    AL("  %s.***%s.................%s***.%s",     sb,r,sb,r);
    AL("   %s.*******************.%s",            sb,r);
    AL("     %s..***********.  %s",               sb,r);
    ALN(); ALN(); ALN();
}

static void build_ascii_cachyos(void) {
    const char *t=TEAL, *d=TEAL_DIM, *r=RESET_C;
    AL("  %s**%s++++++++++++++++%s*%s",           t,d,t,r);
    AL("  %s=++++%s*%s+++++++++++++%s  %s=%s",   d,t,d,r,d,r);
    AL("  %s==+++==++%s*%s+++++++++%s  %s=--%s", d,t,d,r,d,r);
    AL("  %s===++++====++++++++%s  %s=%s",        d,r,d,r);
    AL("  %s====+++++++++++++++%s",               d,r);
    AL("  %s=====++++%s  %s+++%s",                d,r,d,r);
    AL("  %s===+++%s*%s++%s  %s=====%s",          d,t,d,r,d,r);
    AL("  %s+++++++%s*%s+%s  %s---%s",            t,t,d,r,d,r);
    AL("%s========%s*%s",                         d,t,r);
    AL("  %s===++++++%s  %s++++%s",               d,r,d,r);
    AL("  %s+%s*%s+++++++%s  %s======%s",         d,t,d,r,d,r);
    AL("  %s+++%s***%s+++=  %s----%s",            d,t,d,d,r);
    AL("  %s+++++===+++=================%s",      d,r);
    AL("  %s++++==+++++%s*%s+============%s",     d,t,d,r);
    AL("  %s+++=+++++++++++========%s",           d,r);
    AL("  %s+++++++++++++++++====%s",             d,r);
    AL("  %s+++++++++++++++++++%s",               d,r);
}

static void build_ascii_void(void) {
    AL("          -------");
    AL("        -----------");
    AL("        -----------");
    AL("      * -- ------");
    AL("     **     ----");
    AL("    ****  - ----");
    AL("  @@****@@*++=-@@#+++@@");
    AL(" @@@**@@@%%+@@=@@@@+#@@");
    AL(" @@%%%%@@@=-@@#@@@%%-#@@");
    AL(" @@#*  @@*%%@+@@@@%%#@@@");
    AL("    ****  @*=--*@@%%++=");
    AL("    ****  - ----");
    AL("    ****     --");
    AL("    ******  *  -");
    AL("   **********");
    AL("   **********");
    AL("    *******");
    ALN();
}

static void build_ascii_nixos(void) {
    const char *b=BLUE, *w=WHITE, *r=RESET_C;
    AL(" %s+++%s    %s----- ---%s",              b,r,w,r);
    AL(" %s+++*%s    %s----- ----%s",            b,r,w,r);
    AL(" %s++**%s    %s--------%s",              b,r,w,r);
    AL(" %s+++++++********%s%s---===%s  %s+%s",  b,r,w,r,b,r);
    AL(" %s++++++++*********%s%s-====%s  %s+++%s", b,r,w,r,b,r);
    AL(" %s=====%s  %s=====%s  %s+++++%s",        w,r,w,r,b,r);
    AL("  %s====%s  %s====%s%s++++%s",            w,r,w,r,b,r);
    AL(" %s--=-=======%s  %s==%s%s*+++++++%s",   w,r,w,r,b,r);
    AL("%s-------====%s  %s**********+++++%s",   w,r,b,r);
    AL(" %s--------=%s%s**%s  %s**********+%s",  w,r,b,r,b,r);
    AL("  %s----%s%s****%s  %s****%s",           w,r,b,r,b,r);
    AL("   %s----%s  %s*****%s  %s*****%s",      w,r,b,r,b,r);
    AL("  %s---%s  %s****+%s%s=========%s%s--------%s", w,r,b,r,w,r,w,r);
    AL("  %s-%s  %s*****+%s%s=======%s%s--------%s",    w,r,b,r,w,r,w,r);
    AL("      %s*++++++%s  %s=---%s",             b,r,w,r);
    AL("      %s++++  +++++%s  %s----%s",         b,r,w,r);
    AL("      %s+++   +++++%s  %s---%s",          b,r,w,r);
}

static void build_ascii_debian(void) {
    const char *rd=SRED, *br=RED, *r=RESET_C;
    AL("        %s#####  #%s",                    rd,r);
    AL("    %s##################%s",             rd,r);
    AL("   %s########  ########%s",              rd,r);
    AL("  %s#####          #####%s",             rd,r);
    AL("  %s#####          ######%s",            rd,r);
    AL("  %s####   ##       #%s",                rd,r);
    AL("  %s###  #####       ###%s",             rd,r);
    AL("  %s###    #          ##%s",             rd,r);
    AL("  %s###  ##           ###%s",            rd,r);
    AL("  %s##     #           ##%s",            rd,r);
    AL("  %s##     #           ##%s",            rd,r);
    AL("  %s##    ##           ###%s",           rd,r);
    AL("  %s###    #   ##       ##%s",           rd,r);
    AL("  %s###    #   ##      ###%s",           rd,r);
    AL("  %s##     #   %s%s%s%s#%s",            rd,r,br,"%",rd,r);
    AL("  %s###%s",                              rd,r);
    AL("   %s####%s",                            rd,r);
    AL("    %s##%s",                             rd,r);
    AL("    %s###%s",                            rd,r);
    AL("     %s##%s",                            rd,r);
    AL("     %s##%s",                            rd,r);
    AL("     %s##%s",                            rd,r);
    ALN();
}

static void build_ascii_gentoo(void) {
    const char *sb=STEELBLUE, *sl=STEELBLUE_LT, *sd=SBLUE, *r=RESET_C;
    AL("       %s%s%s",                          sl,"%%%%%%%",r);
    AL("     %s%%%s%s%s%s%%%s",                  sl,r,sb,"##########",r,sl);  /* simplified */
    AL("    %s%s%s%s%s%s%s%s%s",                 sl,"%",r,sb,"################",r,sl,"%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s",                 sl,"%",r,sb,"####################",r,sl,"%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",    sl,"%",r,sb,"###############",r,sl,"%%",r,sb,"####",r,sl,"%%%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", sl,"%",r,sb,"#############",r,sl,"%%%%%",r,sd,"@",r,sb,"###",r,sl,"%%%%%",r,sb,"##",r);
    AL("    %s%s%s%s%s%s%s%s%s %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", sl,"%%",r,sb,"###########",r,sl,"%%",r,sl,"%%%",r,sd,"@",r,sb,"###",r,sl,"%%%%%%%",r,sb,"#",r,sl,"%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", sl,"%%%%%",r,sb,"###########",r,sl,"%%",r,sb,"####",r,sl,"%%%%%%%%%",r,sb,"##",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",    sl,"%%%%",r,sb,"################",r,sl,"%%%%%%%%%",r,sb,"##",r,sl,"%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",    sl,"%%%%",r,sb,"#############",r,sl,"%%%%%%%%",r,sb,"##",r,sl,"%%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s",          sb,"##############",r,sl,"%%%%%%%",r,sb,"###",r,sl,"%%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s",          sb,"################",r,sl,"%%%%%",r,sb,"###",r,sl,"%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s",          sb,"#################",r,sl,"%%%%",r,sb,"###",r,sl,"%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s",          sb,"##################",r,sl,"%%%",r,sb,"###",r,sl,"%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s%s%s%s",          sb,"##################",r,sl,"%",r,sb,"####",r,sl,"%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s",                sl,"%",r,sb,"####################",r,sl,"%%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s",                sl,"%",r,sb,"################",r,sl,"%%%%%",r);
    AL("    %s%s%s%s%s%s%s%s%s",                sl,"%",r,sb,"###########",r,sl,"%%%%%%",r);
    AL("    %s%s%s%s%s%s",                      sl,"%%%%%%%%",r,sd,"@@@@@@",r);
    AL("    %s%s%s",                             sd,"@@@@@@",r);
}

static void build_ascii_opensuse(void) {
    const char *g=GREEN, *sg=SGREEN, *r=RESET_C;
    ALN();
    AL("      %s#########%s",                    g,r);
    AL("   %s#####%s%s*-----*%s%s#####%s",       g,r,sg,r,g,r);
    AL("  %s####%s%s---------------%s%s####%s",  g,r,sg,r,g,r);
    AL(" %s###%s%s-------------------%s%s###%s", g,r,sg,r,g,r);
    AL(" %s#####%s%s--%s%s########%s%s-----------%s%s###%s", g,r,sg,r,g,r,sg,r,g,r);
    AL(" %s####################%s%s+------%s%s##%s", g,r,sg,r,g,r);
    AL(" %s###############%s%s+--%s%s#%s%s=-=%s%s#%s%s*-----%s%s##%s", g,r,sg,r,g,r,sg,r,g,r,sg,r,g,r);
    AL(" %s###############%s%s+-%s%s##%s%s--%s%s#%s%s-=%s%s#%s%s=----%s%s###%s", g,r,sg,r,g,r,sg,r,g,r,sg,r,g,r,sg,r,g,r);
    AL(" %s###############%s%s=-%s%s#####%s%s--%s%s##%s%s-----%s%s##%s", g,r,sg,r,g,r,sg,r,g,r,sg,r,g,r);
    AL(" %s###########%s%s-%s%s####%s%s---%s%s#%s%s=--%s%s####%s%s----%s%s##%s", g,r,sg,r,g,r,sg,r,g,r,sg,r,g,r,sg,r,g,r);
    AL(" %s############%s%s----%s%s#########%s%s+----%s%s###%s", g,r,sg,r,g,r,sg,r,g,r);
    AL(" %s###############%s%s=--------%s%s#%s%s+---%s%s##%s", g,r,sg,r,g,r,sg,r,g,r);
    AL(" %s#######################%s%s=---%s%s##%s", g,r,sg,r,g,r);
    AL(" %s###################%s%s-------%s%s###%s", g,r,sg,r,g,r);
    AL(" %s###%s%s-------------------%s%s###%s", g,r,sg,r,g,r);
    AL("  %s####%s%s---------------%s%s####%s",  g,r,sg,r,g,r);
    AL("   %s#####%s%s*-----*%s%s#####%s",       g,r,sg,r,g,r);
    AL("      %s#########%s",                    g,r);
    ALN();
}

static void build_ascii_linux(void) {
    const char *b=SBLACK, *r=RESET_C;
    AL("      %sa8888b.%s",                      b,r);
    AL("     %sd888888b.%s",                     b,r);
    AL("     %s8P\"%sYP\"%sY88%s",               b,r,b,r);
    AL("     8|o||o|88");
    AL("     8'  .88");
    AL("     8'._.' Y8.");
    AL("    d/    'Y8b.");
    AL("   .dP   .   Y8b.");
    AL("  d8:'    \"  '::88b.");
    AL(" d8\"         'Y88b");
    AL(":8P   '      :888");
    AL(" 8a.   :     _a88P");
    AL("._/\"Yaa_ :   .| 88P|");
    AL("\\ YP\"    '| 8P  '.");
    AL("/  \\._____.d|    .'");
    AL("'--..__)888888P'._.''");
}

static void build_ascii(void) {
    n_ascii = 0;
    if (g_is_proxmox) { build_ascii_proxmox(); return; }
    if (g_is_android) { build_ascii_android(); return; }

    const char *id = g_distro_id, *base = g_distro_base;
    if (!strcmp(id,"arch") || !strcmp(id,"endeavouros") ||
        !strcmp(id,"manjaro") || !strcmp(id,"garuda"))
        { build_ascii_arch(); return; }
    if (!strcmp(id,"artix"))     { build_ascii_artix();    return; }
    if (!strcmp(id,"ubuntu"))    { build_ascii_ubuntu();   return; }
    if (!strcmp(id,"linuxmint")) { build_ascii_mint();     return; }
    if (!strcmp(id,"pop"))       { build_ascii_pop();      return; }
    if (!strcmp(id,"cachyos"))   { build_ascii_cachyos();  return; }
    if (!strcmp(id,"void"))      { build_ascii_void();     return; }
    if (!strcmp(id,"nixos"))     { build_ascii_nixos();    return; }
    if (!strcmp(id,"debian"))    { build_ascii_debian();   return; }
    if (!strcmp(id,"gentoo"))    { build_ascii_gentoo();   return; }
    if (strstr(id,"opensuse"))   { build_ascii_opensuse(); return; }
    if (strstr(base,"suse") || strstr(base,"opensuse"))
        { build_ascii_opensuse(); return; }
    if (strstr(base,"arch"))
        { build_ascii_arch(); return; }
    if (strstr(base,"ubuntu") || strstr(base,"debian"))
        { build_ascii_debian(); return; }
    build_ascii_linux();
}

static void build_info(void) {
    n_info = 0;
#define IL(label, val) \
    snprintf(info_lines[n_info++], LINE_LEN, \
             "%s" label ":%s %s%s%s", label_color, RESET_C, INFO_COLOR, (val), (*INFO_COLOR ? RESET_C : ""))

    IL("OS",      g_os);
    {
        char tmp[280];
        snprintf(tmp, sizeof(tmp), "Linux %s", g_kernel);
        snprintf(info_lines[n_info++], LINE_LEN,
                 "%sKernel:%s %s%s%s", label_color, RESET_C,
                 INFO_COLOR, tmp, (*INFO_COLOR ? RESET_C : ""));
    }
    IL("Device",       g_device);
    IL("Uptime",       g_uptime);
    IL("Shell",        g_shell);
    if (g_de[0])
        IL("DE", g_de);
    else if (g_wm[0])
        IL("WM", g_wm);
    IL("Terminal",     g_terminal);
    IL("Display-res",  g_disp1);
    if (g_disp2[0])
        IL("Display-res2", g_disp2);
    IL("CPU", g_cpu);
    IL("GPU", g_gpu);
    if (g_gpu2[0])
        IL("GPU-2", g_gpu2);
    IL("Ram", g_ram);
#undef IL
}

int main(int argc, char *argv[]) {
    bool grey = false;
    const char *color_hex = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--grey") == 0)
            grey = true;
        else if (strncmp(argv[i], "--color:", 8) == 0)
            color_hex = argv[i] + 8;
    }

    init_colors(grey, color_hex);
    gather();
    choose_label_color();
    if (color_hex && g_color_seq_bold[0])
        label_color = g_color_seq_bold;
    build_ascii();
    build_info();

    size_t max_vlen = 0;
    for (int i = 0; i < n_info; i++) {
        size_t v = vis_len(info_lines[i]);
        if (v > max_vlen) max_vlen = v;
    }

    int box_w = (int)max_vlen;
    int inner  = box_w + 2;

    char border[512*3+1];
    int bpos = 0;
    for (int i = 0; i < inner && bpos+3 < (int)sizeof(border); i++) {
        border[bpos++] = '\xe2';
        border[bpos++] = '\x94';
        border[bpos++] = '\x80';
    }
    border[bpos] = '\0';

    int box_h = n_info + 2;
    int rows  = n_ascii > box_h ? n_ascii : box_h;

    for (int i = 0; i < rows; i++) {
        const char *left = (i < n_ascii) ? ascii_lines[i] : "";
        char right[LINE_LEN*3+32] = "";

        if (i < box_h) {
            if (i == 0) {
                char tmp[LINE_LEN*3+32];
                snprintf(tmp, sizeof(tmp),
                         "%s\xe2\x95\xad%s%s%s\xe2\x95\xae%s",
                         INFO_COLOR, RESET_C, border, INFO_COLOR, RESET_C);
                snprintf(right, sizeof(right), "%s", tmp);
            } else if (i == box_h-1) {
                char tmp[LINE_LEN*3+32];
                snprintf(tmp, sizeof(tmp),
                         "%s\xe2\x95\xb0%s%s%s\xe2\x95\xaf%s",
                         INFO_COLOR, RESET_C, border, INFO_COLOR, RESET_C);
                snprintf(right, sizeof(right), "%s", tmp);
            } else {
                const char *content = info_lines[i-1];
                int cvlen  = (int)vis_len(content);
                int spaces = box_w - cvlen;
                int rp     = 0;

                const char *ic   = INFO_COLOR;
                const char *rst  = (*ic) ? RESET_C : "";
                size_t iclen  = strlen(ic);
                size_t rstlen = strlen(rst);

                memcpy(right+rp, ic,  iclen);  rp += (int)iclen;
                right[rp++]='\xe2'; right[rp++]='\x94'; right[rp++]='\x82';
                memcpy(right+rp, rst, rstlen); rp += (int)rstlen;
                right[rp++]=' ';

                size_t clen = strlen(content);
                if (rp+(int)clen+spaces+5 < (int)sizeof(right)) {
                    memcpy(right+rp, content, clen);
                    rp += (int)clen;
                }
                for (int p = 0; p < spaces && rp < (int)sizeof(right)-5; p++)
                    right[rp++]=' ';
                right[rp++]=' ';
                memcpy(right+rp, ic,  iclen);  rp += (int)iclen;
                right[rp++]='\xe2'; right[rp++]='\x94'; right[rp++]='\x82';
                memcpy(right+rp, rst, rstlen); rp += (int)rstlen;
                right[rp] = '\0';
            }
        }

        int pad = LEFT_WIDTH - (int)vis_len(left);
        if (pad < 0) pad = 0;
        printf("%s%*s%s\n", left, pad, "", right);
    }

    printf("\n");
    return 0;
}
