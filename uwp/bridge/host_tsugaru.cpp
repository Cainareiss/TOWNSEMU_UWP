// Flamenco One on Tsugaru - the frontend's emulator interface (core/host.h + compat/swanui_host_xbox.h) over the
// Tsugaru FM TOWNS emulator.  Tsugaru runs on its own thread (TsugaruUwpMain in main_cui/main.cpp); this file starts
// it with a synthesized command line, receives its pictures (TsugaruUwpPublishFrame), feeds it the controllers
// (YsGamePadUwpInject) and drives it with console commands (TsugaruUwpPostCommand).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "swanui_cdecl.h"

extern "C" {
#include "src/SwanStationPS5.h"
#include "src/core/host.h"
#include "compat/swanui_host_xbox.h"
#include "compat/swanui_main.h"
#include "compat/swanui_towns_media.h"
FILE *swanui_fopen_u8(const char *path, const char *mode);
}

#include "tsugaru_uwp_host.h"
#include "ysgamepad_uwp.h"
#include "uwp_paths.h"
#include "host_tsugaru_vm.h"

int TsugaruUwpMain(int ac, char *av[]);
void TsugaruUwpPostCommand(const std::string &cmd);

namespace
{
std::mutex g_lock;

// the VM thread
std::thread g_vm;
std::atomic<bool> g_running{false};
std::atomic<bool> g_loaded{false};
std::string g_core_name = "Tsugaru";

// the newest picture (BGRA, the frontend's XRGB8888)
std::vector<uint8_t> g_frame;
int g_frame_w = 0, g_frame_h = 0;
std::atomic<uint32_t> g_frames_new{0};
std::atomic<uint32_t> g_frames_seen{0};
std::atomic<uint32_t> g_frames_repeated{0};
bool g_fresh = false;

// the pads the frontend gave us
PadState g_pads[SwanStationPS5_MAX_PADS];

std::string g_rom_dir;
std::vector<std::string> g_discs; // the game's discs (one, or the lines of a .m3u)
int g_disc_index = 0;
size_t g_state_size = 0;          // host_state_size, measured once per game
constexpr uint32_t kStateMagic = 0x54535354; // "TSST"
std::string g_cmos_path;
bool g_paused = false;

bool file_exists(const std::string &path)
{
    FILE *f = swanui_fopen_u8(path.c_str(), "rb");
    if (!f)
        return false;
    fclose(f);
    return true;
}

std::string lower_ext(const std::string &path)
{
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return std::string();
    std::string e = path.substr(dot + 1);
    for (char &c : e)
        c = (char)tolower((unsigned char)c);
    return e;
}

// A folder with the system ROMs (FMT_SYS.ROM, FMT_DOS.ROM, FMT_FNT.ROM, FMT_DIC.ROM, optional FMT_F20.ROM, ...).
bool has_roms(const std::string &dir)
{
    return !dir.empty() && file_exists(dir + "/FMT_SYS.ROM");
}

std::string find_rom_dir(const Paths *paths)
{
    std::vector<std::string> candidates;
    if (paths)
    {
        candidates.push_back(paths->bios);
        candidates.push_back(std::string(paths->bios) + "/ROM");
        candidates.push_back(std::string(paths->root) + "/rom");
        candidates.push_back(std::string(paths->games) + "/ROM");
    }
    std::string install = uwp_install_root();
    for (char &c : install)
        if (c == '\\')
            c = '/';
    candidates.push_back(install + "/swanui/rom");
    for (const std::string &c : candidates)
    {
        std::string d = c;
        for (char &ch : d)
            if (ch == '\\')
                ch = '/';
        if (has_roms(d))
            return d;
    }
    return std::string();
}

std::string dir_of(const std::string &path)
{
    const size_t cut = path.find_last_of("/\\");
    return cut == std::string::npos ? std::string() : path.substr(0, cut);
}

// A .m3u lists the discs of a game, one path a line (relative to the playlist's folder, or absolute).
std::vector<std::string> read_playlist(const std::string &m3u)
{
    std::vector<std::string> discs;
    FILE *f = swanui_fopen_u8(m3u.c_str(), "rb");
    if (!f)
        return discs;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        text.append(buf, n);
    fclose(f);
    const std::string dir = dir_of(m3u);
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t end = text.find_first_of("\r\n", pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#')
            continue;
        line = line.substr(first);
        const bool absolute = line.size() > 1 && (line[1] == ':' || line[0] == '/' || line[0] == '\\');
        if (!absolute && !dir.empty())
            line = dir + "/" + line;
        for (char &c : line)
            if (c == '\\')
                c = '/';
        discs.push_back(line);
    }
    return discs;
}

void convert_frame(int w, int h, const unsigned char *rgba)
{
    std::lock_guard<std::mutex> lock(g_lock);
    const size_t n = (size_t)w * h;
    g_frame.resize(n * 4);
    uint8_t *d = g_frame.data();
    for (size_t i = 0; i < n; ++i)
    {
        // Tsugaru: R,G,B,A  ->  libretro XRGB8888 in memory: B,G,R,X
        d[i * 4 + 0] = rgba[i * 4 + 2];
        d[i * 4 + 1] = rgba[i * 4 + 1];
        d[i * 4 + 2] = rgba[i * 4 + 0];
        d[i * 4 + 3] = 0xFF;
    }
    g_frame_w = w;
    g_frame_h = h;
    g_fresh = true;
    g_frames_new++;
}

void send_pad(int id, const PadState &p)
{
    YsGamePadReading r;
    YsGamePadClear(&r);
    if (p.connected)
    {
        const uint32_t b = p.buttons;
        r.buttons[0] = (b & (BIT(BTN_CROSS) | BIT(BTN_SQUARE))) ? 1 : 0;   // A
        r.buttons[1] = (b & (BIT(BTN_CIRCLE) | BIT(BTN_TRIANGLE))) ? 1 : 0; // B
        r.buttons[2] = (b & BIT(BTN_START)) ? 1 : 0;                       // RUN
        r.buttons[3] = (b & BIT(BTN_SELECT)) ? 1 : 0;                      // SELECT
        r.buttons[4] = (b & BIT(BTN_L1)) ? 1 : 0;
        r.buttons[5] = (b & BIT(BTN_R1)) ? 1 : 0;
        bool up = (b & BIT(BTN_UP)) != 0, down = (b & BIT(BTN_DOWN)) != 0;
        bool left = (b & BIT(BTN_LEFT)) != 0, right = (b & BIT(BTN_RIGHT)) != 0;
        const int dead = 12000;
        if (p.ly < -dead) up = true;
        if (p.ly > dead) down = true;
        if (p.lx < -dead) left = true;
        if (p.lx > dead) right = true;
        r.dirs[0].upDownLeftRight[0] = up;
        r.dirs[0].upDownLeftRight[1] = down;
        r.dirs[0].upDownLeftRight[2] = left;
        r.dirs[0].upDownLeftRight[3] = right;
        r.axes[0] = p.lx / 32767.0f;
        r.axes[1] = p.ly / 32767.0f;
        r.axes[2] = p.rx / 32767.0f;
        r.axes[3] = p.ry / 32767.0f;
    }
    YsGamePadUwpInject(id, &r);
}
} // namespace

// ---- Tsugaru -> host (called from the emulator's window thread, see connect_fssimplewindow) ----

extern "C" void TsugaruUwpPublishFrame(int wid, int hei, const unsigned char *rgba, int, int, const unsigned char *)
{
    if (wid > 0 && hei > 0 && rgba)
        convert_frame(wid, hei, rgba);
}

// ---- core/host.h ----

extern "C" bool host_load(const char *game_path, const char *serial, const Paths *paths, const Settings *settings,
                          char *error, size_t error_size)
{
    (void)serial;
    (void)settings;
    auto fail = [&](const char *msg) {
        if (error && error_size)
            snprintf(error, error_size, "%s", msg);
        uwp_log("host_load: %s", msg);
        return false;
    };

    if (g_running)
        return fail("An FM TOWNS game is already running.");
    if (!game_path || !*game_path)
        return fail("No game given.");
    if (!file_exists(game_path))
        return fail("The game file cannot be opened.");

    // The disc set: a .m3u playlist, or the one image.
    g_discs.clear();
    g_disc_index = 0;
    g_state_size = 0;
    if (lower_ext(game_path) == "m3u")
    {
        g_discs = read_playlist(game_path);
        if (g_discs.empty())
            return fail("The playlist (.m3u) lists no disc.");
        if (!file_exists(g_discs[0]))
            return fail("The first disc of the playlist cannot be opened.");
        game_path = g_discs[0].c_str();
    }
    else
    {
        g_discs.push_back(game_path);
    }

    g_rom_dir = find_rom_dir(paths);
    if (g_rom_dir.empty())
        return fail("FM TOWNS system ROMs not found. Put FMT_SYS.ROM, FMT_DOS.ROM, FMT_FNT.ROM and FMT_DIC.ROM in the bios folder.");

    g_cmos_path = std::string(paths ? paths->user : uwp_data_root()) + "/towns-cmos.bin";

    // The command line Tsugaru would get from a terminal.
    std::vector<std::string> args = {"tsugaru", g_rom_dir};
    if (towns_media_of_path(game_path) == TOWNS_MEDIA_FD)
    {
        args.insert(args.end(), {"-FD0", game_path, "-BOOTKEY", "F0"});
    }
    else
    {
        args.insert(args.end(), {"-CD", game_path, "-BOOTKEY", "CD"});
        char companion[SwanStationPS5_PATH_MAX];
        if (towns_companion_fd(game_path, companion, sizeof(companion)))
            args.insert(args.end(), {"-FD0", std::string(companion)});
    }
    args.insert(args.end(), {"-CMOS", g_cmos_path, "-GAMEPORT0", "PHYS0", "-GAMEPORT1", "PHYS1"});

    {
        std::lock_guard<std::mutex> lock(g_lock);
        g_frame.clear();
        g_frame_w = g_frame_h = 0;
        g_fresh = false;
    }
    g_frames_new = 0;
    g_frames_seen = 0;
    g_frames_repeated = 0;
    g_paused = false;

    g_running = true;
    g_loaded = true;
    g_vm = std::thread([args]() mutable {
        std::vector<char *> av;
        for (std::string &a : args)
            av.push_back(a.data());
        av.push_back(nullptr);
        uwp_log("Tsugaru starting: %s", args.size() > 3 ? args[3].c_str() : "");
        const int rc = TsugaruUwpMain((int)args.size(), av.data());
        uwp_log("Tsugaru ended (%d)", rc);
        g_running = false;
    });
    return true;
}

extern "C" void host_unload(void)
{
    if (g_vm.joinable())
    {
        if (g_running)
            TsugaruUwpPostCommand("Q");
        // The VM closes within a frame or two; give it a moment, then force it.
        for (int i = 0; i < 300 && g_running; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (g_running)
            TsugaruUwpPostCommand("FORCEQUIT");
        g_vm.join();
    }
    g_running = false;
    g_loaded = false;
    TsugaruUwpResetInput();
}

extern "C" bool host_loaded(void)
{
    // False once the VM thread has ended by itself (ROM error, or the game quit).
    return g_loaded && g_running;
}

extern "C" const char *host_core_name(void)
{
    return g_core_name.c_str();
}

extern "C" const char *host_emulator_for(const Settings *, const char *)
{
    return g_core_name.c_str();
}

extern "C" void host_set_pads(const PadState pads[SwanStationPS5_MAX_PADS])
{
    for (int i = 0; i < SwanStationPS5_MAX_PADS; ++i)
        g_pads[i] = pads[i];
    // Two game ports; pad 1 plays port 0, pad 2 port 1.
    send_pad(0, pads[0]);
    send_pad(1, pads[1]);
}

extern "C" void host_set_gun(int) {}
extern "C" void host_set_region_override(const char *) {}
extern "C" void host_set_initial_disc(int) {}
extern "C" void host_set_special(int) {}
extern "C" bool host_analog_mode(int) { return false; }
extern "C" void host_apply_devices(const Settings *) {}
extern "C" void host_set_fixes(unsigned, const GameInfo *) {}
extern "C" float host_rumble_level(int) { return 0.0f; }

extern "C" void host_run_frame(void)
{
    // Tsugaru runs by itself on its own thread.
}

// Diagnostic: with <LocalState>\selftest-state.txt present, once the game has run ~10 s the in-memory and file save
// states are exercised and the results logged (verifies the VM-thread job path without touching the controller).
static void selftest_states(void)
{
    static int phase = 0; // 0 not checked, 1 armed, 2 done/off
    if (phase == 2 || !g_running)
        return;
    if (phase == 0)
    {
        phase = file_exists(std::string(uwp_data_root()) + "/selftest-state.txt") ? 1 : 2;
        return;
    }
    if (g_frames_new < 600)
        return;
    phase = 2;
    const size_t slot = host_state_size();
    std::vector<uint8_t> buf(slot);
    const bool ser = slot > 0 && host_serialize(buf.data(), slot);
    const bool unser = ser && host_unserialize(buf.data(), slot);
    const std::string file = std::string(uwp_data_root()) + "/selftest.tsst";
    const bool saved = host_save_state(file.c_str());
    const bool loaded = saved && host_load_state(file.c_str());
    uwp_log("selftest: slot %zu bytes, serialize %s, unserialize %s, file save %s, file load %s, discs %d", slot,
            ser ? "ok" : "FAILED", unser ? "ok" : "FAILED", saved ? "ok" : "FAILED", loaded ? "ok" : "FAILED",
            host_disc_count());
}

// Called by the app loop before every frontend frame: tells the VM when the screen wants it paused or running.
extern "C" void host_tsugaru_pump(void)
{
    selftest_states();
    static int last = -1;
    const int want = swanui_main_wants_emulation() ? 1 : 0;
    if (want != last && g_running)
    {
        TsugaruUwpPostCommand(want ? "RUN" : "PAUSE");
        g_paused = !want;
        if (!want)
            TsugaruUwpResetInput();
        last = want;
    }
    if (!g_running)
        last = -1;
}

extern "C" void host_set_speculative(bool) {}

extern "C" uint32_t host_frames_new(void)
{
    return g_frames_new;
}

extern "C" uint32_t host_frames_repeated(void)
{
    return g_frames_repeated;
}

extern "C" void host_reset(void)
{
    if (g_running && !tsvm_reset())
        TsugaruUwpPostCommand("RESET");
}

extern "C" void host_apply_settings(const Settings *) {}

extern "C" double host_fps(void)
{
    return 60.0;
}

extern "C" int host_sample_rate(void)
{
    return 44100;
}

extern "C" float host_aspect(void)
{
    return 4.0f / 3.0f;
}

extern "C" const void *host_frame(int *width, int *height, size_t *pitch, int *pixel_format, bool *fresh)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_frame.empty() || g_frame_w <= 0)
        return nullptr;
    if (width)
        *width = g_frame_w;
    if (height)
        *height = g_frame_h;
    if (pitch)
        *pitch = (size_t)g_frame_w * 4;
    if (pixel_format)
        *pixel_format = 1; // XRGB8888
    if (fresh)
    {
        *fresh = g_fresh;
        g_fresh = false;
    }
    return g_frame.data();
}

extern "C" bool host_save_state(const char *path)
{
    if (!g_running || !path)
        return false;
    std::string p = path;
    for (char &c : p)
        if (c == '\\')
            c = '/';
    const bool ok = tsvm_save_state(p);
    uwp_log("save state %s: %s", path, ok ? "ok" : "failed");
    return ok;
}

extern "C" bool host_load_state(const char *path)
{
    if (!g_running || !path)
        return false;
    std::string p = path;
    for (char &c : p)
        if (c == '\\')
            c = '/';
    const bool ok = tsvm_load_state(p);
    uwp_log("load state %s: %s", path, ok ? "ok" : "failed");
    return ok;
}

// Save states in memory (rewind, quick resume): Tsugaru's SaveStateMem/LoadStateMem run on the VM thread.
// The buffer: u32 magic, u32 length, then the state.  host_state_size is the measured size rounded up with headroom
// (the state grows a little with the devices' queues), so the frontend's fixed-size slots always fit.
extern "C" size_t host_state_size(void)
{
    if (!g_running)
        return 0;
    if (g_state_size == 0)
    {
        std::vector<uint8_t> probe;
        if (!tsvm_serialize(probe))
            return 0;
        const size_t need = probe.size() + 8;
        g_state_size = ((need + need / 8) + 0xFFFF) & ~(size_t)0xFFFF; // +12.5 %, 64 KB multiple
        uwp_log("state size: %zu bytes (slot %zu)", probe.size(), g_state_size);
    }
    return g_state_size;
}

extern "C" bool host_serialize(void *buffer, size_t size)
{
    if (!g_running || !buffer || size < 8)
        return false;
    std::vector<uint8_t> state;
    if (!tsvm_serialize(state))
        return false;
    if (state.size() + 8 > size)
    {
        uwp_log("serialize: state %zu bytes does not fit the %zu-byte slot", state.size(), size);
        g_state_size = 0; // re-measure next time
        return false;
    }
    uint8_t *d = static_cast<uint8_t *>(buffer);
    const uint32_t len = static_cast<uint32_t>(state.size());
    memcpy(d, &kStateMagic, 4);
    memcpy(d + 4, &len, 4);
    memcpy(d + 8, state.data(), state.size());
    return true;
}

extern "C" bool host_unserialize(const void *buffer, size_t size)
{
    if (!g_running || !buffer || size < 8)
        return false;
    const uint8_t *d = static_cast<const uint8_t *>(buffer);
    uint32_t magic, len;
    memcpy(&magic, d, 4);
    memcpy(&len, d + 4, 4);
    if (magic != kStateMagic || len + 8 > size)
        return false;
    return tsvm_unserialize(d + 8, len);
}

extern "C" bool host_capture(uint8_t *rgba, int w, int h)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_frame.empty() || w <= 0 || h <= 0)
        return false;
    for (int y = 0; y < h; ++y)
    {
        const int sy = y * g_frame_h / h;
        for (int x = 0; x < w; ++x)
        {
            const int sx = x * g_frame_w / w;
            const uint8_t *s = &g_frame[((size_t)sy * g_frame_w + sx) * 4];
            uint8_t *d = rgba + ((size_t)y * w + x) * 4;
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d[3] = 0xFF;
        }
    }
    return true;
}

extern "C" void host_set_patches_dir(const char *) {}

struct retro_memory_map;
extern "C" const struct retro_memory_map *host_memory_map(void) { return nullptr; }
extern "C" void *host_memory_data(unsigned) { return nullptr; }
extern "C" size_t host_memory_size(unsigned) { return 0; }

extern "C" int host_disc_count(void)
{
    return g_running ? (int)g_discs.size() : 0;
}

extern "C" int host_disc_index(void)
{
    return g_disc_index;
}

// Another disc of the playlist into the CD-ROM drive (eject, swap, insert: the game sees a real disc change).
extern "C" bool host_disc_select(int index)
{
    if (!g_running || index < 0 || index >= (int)g_discs.size())
        return false;
    if (index == g_disc_index)
        return true;
    if (!file_exists(g_discs[index]))
    {
        uwp_log("disc %d: cannot open %s", index + 1, g_discs[index].c_str());
        return false;
    }
    tsvm_eject_cd();
    if (!tsvm_load_cd(g_discs[index]))
    {
        uwp_log("disc %d: Tsugaru could not load %s", index + 1, g_discs[index].c_str());
        return false;
    }
    g_disc_index = index;
    uwp_log("disc %d of %d inserted: %s", index + 1, (int)g_discs.size(), g_discs[index].c_str());
    return true;
}

extern "C" void host_cheat_reset(void) {}
extern "C" void host_cheat_set(unsigned, const char *) {}

// ---- compat/swanui_host_xbox.h ----

extern "C" bool host_set_speed(bool, bool back)
{
    return !back;
}

extern "C" uint32_t host_frames_advanced(void)
{
    const uint32_t now = g_frames_new;
    const uint32_t prev = g_frames_seen.exchange(now);
    return now - prev;
}

extern "C" bool host_save_state_bg(const char *path, bool wait)
{
    if (wait || !path)
        return host_save_state(path);
    std::string p = path;
    std::thread([p]() { host_save_state(p.c_str()); }).detach();
    return true;
}

extern "C" bool host_serial_for_path(const char *, char *serial, size_t size)
{
    if (serial && size)
        serial[0] = '\0';
    return false;
}

extern "C" bool host_bios_present(void)
{
    return !g_rom_dir.empty();
}

// ---- mouse: the app gives the pointer in the frontend's 1920x1080 canvas; the emulator wants picture pixels ----

extern "C" void plat_game_rect(int *x, int *y, int *w, int *h);

extern "C" void host_tsugaru_pointer(float canvas_x, float canvas_y, bool left, bool middle, bool right)
{
    if (!g_running || g_paused)
        return;
    int gx = 0, gy = 0, gw = 0, gh = 0;
    plat_game_rect(&gx, &gy, &gw, &gh);
    int w, h;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        w = g_frame_w;
        h = g_frame_h;
    }
    if (gw <= 0 || gh <= 0 || w <= 0 || h <= 0)
        return;
    int x = (int)((canvas_x - gx) * w / gw);
    int y = (int)((canvas_y - gy) * h / gh);
    x = std::max(0, std::min(w - 1, x));
    y = std::max(0, std::min(h - 1, y));

    static bool pl = false, pm = false, pr = false;
    TsugaruUwpMouseMove(x, y);
    if (left != pl)
    {
        TsugaruUwpMouseButton(0, left, x, y);
        pl = left;
    }
    if (middle != pm)
    {
        TsugaruUwpMouseButton(1, middle, x, y);
        pm = middle;
    }
    if (right != pr)
    {
        TsugaruUwpMouseButton(2, right, x, y);
        pr = right;
    }
}
