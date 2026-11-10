// Tsugaru (UWP / Xbox) - the frontend's platform layer (src/platform/platform.h) on Direct3D 11.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// No DuckStation, no ImGui: this file owns the D3D11 device and the DXGI swap chain of the CoreWindow and draws the
// frontend's UI itself.
//  - The UI is authored for a 1920x1080 logical canvas; every vertex is scaled by min(sw/1920, sh/1080) and centred
//    (letterbox) into the real swap-chain size.
//  - Drawing is immediate-mode batched: plat_draw_* append vertices (ARGB, uv, position) to one CPU list; the list is
//    flushed into a dynamic vertex buffer when the texture / sampler / blend / scissor changes and at plat_end_frame.
//    One vertex shader, one pixel shader (tex * colour), a 1x1 white texture for flat colour.
//  - Textures keep a CPU copy (RGBA8) and create the GPU object lazily; mip chains are made on the CPU. A lost device
//    just bumps a generation counter and every texture re-uploads on its next use.
//  - The emulated picture comes from plat_upload_game (0RGB1555 / XRGB8888 / RGB565 -> BGRA8 dynamic texture) and is
//    drawn by plat_draw_game with the REF maths (aspect, integer scale, TV-frame shrink, manual crop, crop edges,
//    smooth / sharp sampling, scanlines and dimming).
//  - Input: XInput 0..3 (xinputuap) + the PC keyboard mask merged into pad 0, rumble with a 2 s watchdog. Audio: one
//    XAudio2 source voice for the UI sounds fed from a 4 s ring (plat_audio_*).
// Thread rule: every plat_* function runs on the frontend thread, except plat_texture_create / plat_texture_free (mutex)
// and the PlatD3D11_* / plat_xbox_key_event entry points (UI thread, they only set atomics under a mutex).
// Shaders are compiled offline (bridge/shaders/plat_d3d11.hlsl -> *_vs.h / *_ps.h); there is no runtime D3DCompile.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <xaudio2.h>
#include <Xinput.h>

#include <winrt/base.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.UserProfile.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

// The frontend headers are C, compiled here as C++ inside extern "C" (see the previous bridge's swanui_cdecl.h):
// the GNU attribute syntax is hints only.
#ifndef SWANUI_XBOX
#define SWANUI_XBOX 1
#endif
#define __attribute__(x)
extern "C" {
#include "src/SwanStationPS5.h"
#include "src/i18n.h"
#include "src/platform/platform.h"
#include "src/platform/xbr.h"
}
#undef __attribute__

#include "plat_d3d11.h"

#include "shaders/plat_d3d11_game_ps.h"
#include "shaders/plat_d3d11_ps.h"
#include "shaders/plat_d3d11_vs.h"

// The opaque frontend texture (platform.h: "typedef struct PlatTexture PlatTexture;").
struct PlatTexture
{
    std::vector<uint8_t> rgba; // width*height*4, owned (device-lost recovery and mip source)
    int width = 0;
    int height = 0;
    bool smooth = false;
    bool mips = false;
    winrt::com_ptr<ID3D11ShaderResourceView> srv;
    uint32_t gen = 0;                  // device generation of `srv`
    uint32_t failed_gen = UINT32_MAX;  // do not retry a failed upload every frame
    bool freed = false;                // plat_texture_free called; destroyed at the next plat_begin_frame
};

namespace {

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s16 = int16_t;

constexpr int kCanvasW = 1920;
constexpr int kCanvasH = 1080;
constexpr u32 kVbVerts = 3u * 43690u; // dynamic vertex buffer capacity (a multiple of 3)
constexpr u64 kRumbleWatchdogUs = 2000000;
constexpr u64 kPadProbeUs = 1000000;
constexpr int kMaxPads = SwanStationPS5_MAX_PADS;

// ---- small helpers -----------------------------------------------------------------------------------------------

u64 NowUs()
{
    return static_cast<u64>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

#define Log(...) SwanStationPS5_log(__VA_ARGS__)

float Clamp01(float v)
{
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

// ---- state -------------------------------------------------------------------------------------------------------

bool s_initialized = false;
bool s_gpu_alive = false;
char s_init_error[256] = "";
char s_screen_info[320] = "Direct3D 11 (not initialised)";
u32 s_gen = 1; // device generation: GPU objects of an older one are dead
bool s_need_device = false;

// window (set from the UI thread)
std::mutex s_dev_mutex; // guards s_dev against PlatD3D11_Trim (UI thread)
std::mutex s_win_mutex;
winrt::com_ptr<IUnknown> s_window;
int s_win_w = kCanvasW, s_win_h = kCanvasH;
float s_dpi = 1.0f;
bool s_win_changed = false;     // a new window object: rebuild the swap chain
bool s_size_changed = false;    // new size: ResizeBuffers
std::atomic<bool> s_active{true};

// D3D objects (frontend thread only)
winrt::com_ptr<ID3D11Device> s_dev;
winrt::com_ptr<ID3D11DeviceContext> s_ctx;
winrt::com_ptr<IDXGISwapChain1> s_sc;
winrt::com_ptr<ID3D11RenderTargetView> s_rtv;
winrt::com_ptr<ID3D11VertexShader> s_vs;
winrt::com_ptr<ID3D11PixelShader> s_ps;
winrt::com_ptr<ID3D11PixelShader> s_ps_game; // the game picture: colour, sharpen, FSR-like scale, CRT/LCD/NTSC looks
winrt::com_ptr<ID3D11InputLayout> s_layout;
winrt::com_ptr<ID3D11Buffer> s_cb;
winrt::com_ptr<ID3D11Buffer> s_cb_game; // cbuffer Picture (b1), 6 x float4
winrt::com_ptr<ID3D11Buffer> s_vb;
winrt::com_ptr<ID3D11SamplerState> s_samp[3]; // 0 point, 1 linear, 2 trilinear
winrt::com_ptr<ID3D11BlendState> s_blend[2];  // 0 alpha blend, 1 opaque
winrt::com_ptr<ID3D11RasterizerState> s_raster;
winrt::com_ptr<ID3D11ShaderResourceView> s_white;
u32 s_vb_pos = 0;
int s_sw = kCanvasW, s_sh = kCanvasH; // swap-chain size
float s_scale = 1.0f, s_ox = 0.0f, s_oy = 0.0f;
u32 s_frame_serial = 0;
int s_active_pass = 0;
bool s_in_frame = false;

inline float TX(float x) { return s_ox + x * s_scale; }
inline float TY(float y) { return s_oy + y * s_scale; }

// batch
struct Vtx
{
    float x, y, u, v;
    u32 col; // ARGB (B8G8R8A8_UNORM input element)
};
struct ClipRect
{
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool empty = false;
};
std::vector<Vtx> s_verts;
ID3D11ShaderResourceView* s_b_srv = nullptr;
int s_b_samp = 0;
bool s_b_opaque = false;
ClipRect s_b_clip;
ClipRect s_clip; // current clip, physical (full swap chain when off)
bool s_clip_on = false;

// textures
std::mutex s_tex_mutex;
std::unordered_set<PlatTexture*> s_live;
std::vector<PlatTexture*> s_free_queue;

// the game picture
winrt::com_ptr<ID3D11Texture2D> s_game_tex;
winrt::com_ptr<ID3D11ShaderResourceView> s_game_srv;
u32 s_game_gen = 0;
int s_game_w = 0, s_game_h = 0;     // texture size
int s_src_w = 0, s_src_h = 0;       // last uploaded picture
bool s_game_have = false;
int s_game_rect[4] = {0, 0, 0, 0};  // logical, plat_game_rect
std::vector<u32> s_game_conv;       // the 32-bit copy of a 16-bit frame, or the frame itself when a prescaler runs
std::vector<u32> s_game_scaled;     // prescaler output (Scale2x / xBR)
int s_prescale = 1;                 // texture = picture x s_prescale
// picture options (plat_set_colour / plat_set_fsr); the frontend calls them with the Settings values
int s_brightness = 1, s_colour = 0, s_sharpen = 0;
bool s_fsr = false;

// input
int s_player_order[kMaxPads] = {0, 1, 2, 3};
bool s_connected[kMaxPads] = {};
u64 s_last_probe[kMaxPads] = {};
u32 s_rumble_strong[kMaxPads] = {};
u32 s_rumble_weak[kMaxPads] = {};
u64 s_rumble_at[kMaxPads] = {};
std::atomic<u32> s_key_bits{0};
bool s_pad_motion = false, s_pad_touch = false;
int s_want_output = 0;
std::mutex s_ptr_mutex;
float s_ptr_x = 0, s_ptr_y = 0, s_ptr_last_x = 0, s_ptr_last_y = 0;
bool s_ptr_pressed = false, s_ptr_valid = false, s_ptr_have_last = false;
float s_ptr_dx = 0, s_ptr_dy = 0;

// display refresh estimate
u64 s_present_last_us = 0;
double s_present_ema_us = 0.0;
int s_present_samples = 0;

// clock
bool s_clock_24h = true;
bool s_clock_checked = false;
u64 s_clock_checked_us = 0;

// backdrop
const u8* s_backdrop_shown = nullptr;
PlatTexture* s_backdrop_tex = nullptr;
int s_backdrop_w = 0, s_backdrop_h = 0;
bool s_backdrop_dirty = false;

// ---- audio (UI sounds) -------------------------------------------------------------------------------------------

constexpr int kAudioBuffers = 4;
constexpr int kAudioFrames = 1024; // per XAudio2 buffer

std::mutex s_ring_mutex;
std::vector<s16> s_ring; // interleaved stereo, s_ring_frames * 2 samples
size_t s_ring_frames = 0;
u64 s_read = 0, s_write = 0;
int s_rate = 0;
double s_audio_ratio = 1.0, s_audio_pos = 0.0;
s16 s_audio_last[2] = {0, 0};
std::vector<s16> s_stretch_out;

winrt::com_ptr<IXAudio2> s_xa;
IXAudio2MasteringVoice* s_master = nullptr;
IXAudio2SourceVoice* s_voice = nullptr;
std::atomic<bool> s_audio_live{false};
s16 s_abuf[kAudioBuffers][kAudioFrames * 2];

void AudioFill(int index)
{
    s16* out = s_abuf[index];
    u32 copied = 0;
    {
        std::lock_guard<std::mutex> lk(s_ring_mutex);
        if (s_ring_frames > 0)
        {
            const u64 queued = s_write - s_read;
            copied = static_cast<u32>(std::min<u64>(queued, kAudioFrames));
            for (u32 i = 0; i < copied; ++i)
            {
                const size_t p = static_cast<size_t>((s_read + i) % s_ring_frames) * 2;
                out[i * 2] = s_ring[p];
                out[i * 2 + 1] = s_ring[p + 1];
            }
            s_read += copied;
        }
    }
    if (copied < kAudioFrames) // silence on underrun
        std::memset(out + static_cast<size_t>(copied) * 2, 0, (kAudioFrames - copied) * 2 * sizeof(s16));
}

void AudioSubmit(int index)
{
    if (!s_audio_live.load(std::memory_order_acquire) || !s_voice)
        return;
    AudioFill(index);
    XAUDIO2_BUFFER b = {};
    b.AudioBytes = kAudioFrames * 2 * sizeof(s16);
    b.pAudioData = reinterpret_cast<const BYTE*>(s_abuf[index]);
    b.pContext = reinterpret_cast<void*>(static_cast<intptr_t>(index));
    s_voice->SubmitSourceBuffer(&b);
}

class UiAudioCallback final : public IXAudio2VoiceCallback
{
public:
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnBufferEnd(void* context) override
    {
        AudioSubmit(static_cast<int>(reinterpret_cast<intptr_t>(context)));
    }
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}
};
UiAudioCallback s_audio_cb;

void RingPush(const s16* frames, size_t n)
{
    std::lock_guard<std::mutex> lk(s_ring_mutex);
    if (s_ring_frames == 0 || n == 0)
        return;
    if (n > s_ring_frames) // keep the newest
    {
        frames += (n - s_ring_frames) * 2;
        n = s_ring_frames;
    }
    const u64 queued = s_write - s_read;
    if (queued + n > s_ring_frames)
        s_read = s_write + n - s_ring_frames; // drop the oldest
    for (size_t i = 0; i < n; ++i)
    {
        const size_t p = static_cast<size_t>((s_write + i) % s_ring_frames) * 2;
        s_ring[p] = frames[i * 2];
        s_ring[p + 1] = frames[i * 2 + 1];
    }
    s_write += n;
}

void RingClear()
{
    std::lock_guard<std::mutex> lk(s_ring_mutex);
    s_read = s_write;
}

// ---- WinRT lookups -----------------------------------------------------------------------------------------------

std::string AppRootPath()
{
    try
    {
        return winrt::to_string(winrt::Windows::ApplicationModel::Package::Current().InstalledLocation().Path());
    }
    catch (...)
    {
        return std::string();
    }
}

std::string DataRootPath()
{
    try
    {
        return winrt::to_string(winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path());
    }
    catch (...)
    {
        return std::string();
    }
}

void CopyPathSlashes(char* out, size_t size, const std::string& path)
{
    if (!out || size == 0)
        return;
    const size_t n = std::min(path.size(), size - 1);
    for (size_t i = 0; i < n; ++i)
        out[i] = (path[i] == '\\') ? '/' : path[i];
    out[n] = '\0';
}

void RefreshClockPreference(u64 now, bool force)
{
    if (!force && s_clock_checked && now - s_clock_checked_us < 60000000ull)
        return;
    s_clock_checked = true;
    s_clock_checked_us = now;
    bool h24 = s_clock_24h;
    bool got = false;
    try
    {
        const auto clocks = winrt::Windows::System::UserProfile::GlobalizationPreferences::Clocks();
        if (clocks && clocks.Size() > 0)
        {
            h24 = (clocks.GetAt(0) == winrt::Windows::Globalization::ClockIdentifiers::TwentyFourHour());
            got = true;
        }
    }
    catch (...)
    {
    }
    if (!got)
    {
        wchar_t buf[80] = {};
        if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT, buf, 80) > 0)
            h24 = (wcschr(buf, L'H') != nullptr);
    }
    s_clock_24h = h24;
}

// ---- D3D11 device ------------------------------------------------------------------------------------------------

void UpdateTransform()
{
    const float sx = static_cast<float>(s_sw) / static_cast<float>(kCanvasW);
    const float sy = static_cast<float>(s_sh) / static_cast<float>(kCanvasH);
    s_scale = std::min(sx, sy);
    if (s_scale <= 0.0f)
        s_scale = 1.0f;
    s_ox = (static_cast<float>(s_sw) - static_cast<float>(kCanvasW) * s_scale) * 0.5f;
    s_oy = (static_cast<float>(s_sh) - static_cast<float>(kCanvasH) * s_scale) * 0.5f;
}

bool CreateDeviceObjects()
{
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                  D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
                                   ARRAYSIZE(levels), D3D11_SDK_VERSION, s_dev.put(), &got, s_ctx.put());
    if (hr == E_INVALIDARG) // a runtime without 11_1
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels + 1,
                               ARRAYSIZE(levels) - 1, D3D11_SDK_VERSION, s_dev.put(), &got, s_ctx.put());
    if (FAILED(hr))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "D3D11CreateDevice failed: 0x%08X", static_cast<unsigned>(hr));
        return false;
    }

    if (FAILED(s_dev->CreateVertexShader(g_plat_vs, sizeof(g_plat_vs), nullptr, s_vs.put())) ||
        FAILED(s_dev->CreatePixelShader(g_plat_ps, sizeof(g_plat_ps), nullptr, s_ps.put())) ||
        FAILED(s_dev->CreatePixelShader(g_plat_ps_game, sizeof(g_plat_ps_game), nullptr, s_ps_game.put())))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "creating the UI shaders failed");
        return false;
    }
    const D3D11_INPUT_ELEMENT_DESC elems[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    if (FAILED(s_dev->CreateInputLayout(elems, ARRAYSIZE(elems), g_plat_vs, sizeof(g_plat_vs), s_layout.put())))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "creating the UI input layout failed");
        return false;
    }

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 16;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(s_dev->CreateBuffer(&bd, nullptr, s_cb.put())))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "creating the constant buffer failed");
        return false;
    }
    bd.ByteWidth = 6 * 16; // cbuffer Picture
    if (FAILED(s_dev->CreateBuffer(&bd, nullptr, s_cb_game.put())))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "creating the picture constant buffer failed");
        return false;
    }
    bd = {};
    bd.ByteWidth = kVbVerts * sizeof(Vtx);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(s_dev->CreateBuffer(&bd, nullptr, s_vb.put())))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "creating the vertex buffer failed");
        return false;
    }
    s_vb_pos = 0;

    const D3D11_FILTER filters[3] = {D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
                                     D3D11_FILTER_MIN_MAG_MIP_LINEAR};
    for (int i = 0; i < 3; ++i)
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = filters[i];
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(s_dev->CreateSamplerState(&sd, s_samp[i].put())))
        {
            std::snprintf(s_init_error, sizeof(s_init_error), "creating a sampler failed");
            return false;
        }
    }
    for (int i = 0; i < 2; ++i)
    {
        D3D11_BLEND_DESC bl = {};
        D3D11_RENDER_TARGET_BLEND_DESC& rt = bl.RenderTarget[0];
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (i == 0)
        {
            rt.BlendEnable = TRUE;
            rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOp = D3D11_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        }
        if (FAILED(s_dev->CreateBlendState(&bl, s_blend[i].put())))
        {
            std::snprintf(s_init_error, sizeof(s_init_error), "creating a blend state failed");
            return false;
        }
    }
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.ScissorEnable = TRUE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(s_dev->CreateRasterizerState(&rd, s_raster.put())))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "creating the rasterizer state failed");
        return false;
    }

    // 1x1 white: flat colour goes through the same pixel shader
    const u32 white = 0xffffffffu;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 1;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sr = {&white, 4, 0};
    winrt::com_ptr<ID3D11Texture2D> wt;
    if (FAILED(s_dev->CreateTexture2D(&td, &sr, wt.put())) || FAILED(s_dev->CreateShaderResourceView(wt.get(), nullptr, s_white.put())))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "creating the white texture failed");
        return false;
    }
    return true;
}

bool CreateTargets()
{
    s_rtv = nullptr;
    winrt::com_ptr<ID3D11Texture2D> bb;
    if (FAILED(s_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), bb.put_void())))
        return false;
    if (FAILED(s_dev->CreateRenderTargetView(bb.get(), nullptr, s_rtv.put())))
        return false;
    DXGI_SWAP_CHAIN_DESC1 d = {};
    if (SUCCEEDED(s_sc->GetDesc1(&d)))
    {
        s_sw = static_cast<int>(d.Width);
        s_sh = static_cast<int>(d.Height);
    }
    UpdateTransform();
    return true;
}

bool CreateSwapChain()
{
    winrt::com_ptr<IUnknown> window;
    int w, h;
    {
        std::lock_guard<std::mutex> lk(s_win_mutex);
        window = s_window;
        w = s_win_w;
        h = s_win_h;
        s_win_changed = false;
        s_size_changed = false;
    }
    if (!window)
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "no CoreWindow: PlatD3D11_SetWindow was not called");
        return false;
    }
    winrt::com_ptr<IDXGIDevice1> dxgi_dev;
    if (FAILED(s_dev->QueryInterface(__uuidof(IDXGIDevice1), dxgi_dev.put_void())))
        return false;
    dxgi_dev->SetMaximumFrameLatency(1);
    winrt::com_ptr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_dev->GetAdapter(adapter.put())))
        return false;
    winrt::com_ptr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory2), factory.put_void())))
        return false;

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = static_cast<UINT>(std::max(w, 1));
    sd.Height = static_cast<UINT>(std::max(h, 1));
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    s_sc = nullptr;
    HRESULT hr = factory->CreateSwapChainForCoreWindow(s_dev.get(), window.get(), &sd, nullptr, s_sc.put());
    if (FAILED(hr))
    {
        std::snprintf(s_init_error, sizeof(s_init_error), "CreateSwapChainForCoreWindow failed: 0x%08X",
                      static_cast<unsigned>(hr));
        return false;
    }
    return CreateTargets();
}

void ReleaseGpuObjectsImpl()
{
    if (s_ctx)
    {
        s_ctx->ClearState();
        s_ctx->Flush();
    }
    s_rtv = nullptr;
    s_sc = nullptr;
    s_white = nullptr;
    s_game_srv = nullptr;
    s_game_tex = nullptr;
    s_game_w = s_game_h = 0;
    s_raster = nullptr;
    for (auto& b : s_blend)
        b = nullptr;
    for (auto& s : s_samp)
        s = nullptr;
    s_vb = nullptr;
    s_cb = nullptr;
    s_cb_game = nullptr;
    s_layout = nullptr;
    s_ps = nullptr;
    s_ps_game = nullptr;
    s_vs = nullptr;
    {
        std::lock_guard<std::mutex> lk(s_tex_mutex);
        for (PlatTexture* t : s_live)
            t->srv = nullptr;
    }
    s_ctx = nullptr;
    s_dev = nullptr;
    s_b_srv = nullptr;
    s_verts.clear();
}

void ReleaseGpuObjects()
{
    std::lock_guard<std::mutex> lk(s_dev_mutex);
    ReleaseGpuObjectsImpl();
}

bool BuildGpu()
{
    std::lock_guard<std::mutex> lk(s_dev_mutex);
    s_init_error[0] = '\0';
    if (!CreateDeviceObjects() || !CreateSwapChain())
    {
        ReleaseGpuObjectsImpl();
        return false;
    }
    ++s_gen;
    s_vb_pos = 0;
    s_game_have = false; // the picture lives in the old device: wait for the next upload
    return true;
}

// ---- batching ----------------------------------------------------------------------------------------------------

void SetBaseState()
{
    if (!s_ctx || !s_rtv)
        return;
    const float cb[4] = {2.0f / static_cast<float>(s_sw), 2.0f / static_cast<float>(s_sh), 0.0f, 0.0f};
    s_ctx->UpdateSubresource(s_cb.get(), 0, nullptr, cb, 0, 0);
    ID3D11RenderTargetView* rtv = s_rtv.get();
    s_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    D3D11_VIEWPORT vp = {0.0f, 0.0f, static_cast<float>(s_sw), static_cast<float>(s_sh), 0.0f, 1.0f};
    s_ctx->RSSetViewports(1, &vp);
    s_ctx->RSSetState(s_raster.get());
    s_ctx->IASetInputLayout(s_layout.get());
    s_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    s_ctx->VSSetShader(s_vs.get(), nullptr, 0);
    ID3D11Buffer* cbp = s_cb.get();
    s_ctx->VSSetConstantBuffers(0, 1, &cbp);
    s_ctx->PSSetShader(s_ps.get(), nullptr, 0);
}

void Flush()
{
    if (s_verts.empty())
        return;
    if (!s_ctx || !s_rtv || !s_b_srv || s_b_clip.empty)
    {
        s_verts.clear();
        return;
    }
    ID3D11ShaderResourceView* srv = s_b_srv;
    s_ctx->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* smp = s_samp[s_b_samp].get();
    s_ctx->PSSetSamplers(0, 1, &smp);
    s_ctx->OMSetBlendState(s_blend[s_b_opaque ? 1 : 0].get(), nullptr, 0xffffffffu);
    const D3D11_RECT sc = {s_b_clip.x0, s_b_clip.y0, s_b_clip.x1, s_b_clip.y1};
    s_ctx->RSSetScissorRects(1, &sc);

    size_t done = 0;
    const size_t total = s_verts.size();
    while (done < total)
    {
        const u32 chunk = static_cast<u32>(std::min<size_t>(total - done, kVbVerts));
        D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
        if (s_vb_pos + chunk > kVbVerts)
        {
            s_vb_pos = 0;
            mode = D3D11_MAP_WRITE_DISCARD;
        }
        else if (s_vb_pos == 0)
        {
            mode = D3D11_MAP_WRITE_DISCARD;
        }
        D3D11_MAPPED_SUBRESOURCE m = {};
        if (FAILED(s_ctx->Map(s_vb.get(), 0, mode, 0, &m)))
            break;
        std::memcpy(static_cast<Vtx*>(m.pData) + s_vb_pos, s_verts.data() + done, chunk * sizeof(Vtx));
        s_ctx->Unmap(s_vb.get(), 0);
        ID3D11Buffer* vb = s_vb.get();
        const UINT stride = sizeof(Vtx), offset = 0;
        s_ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        s_ctx->Draw(chunk, s_vb_pos);
        s_vb_pos += chunk;
        done += chunk;
    }
    s_verts.clear();
}

bool SameClip(const ClipRect& a, const ClipRect& b)
{
    return a.x0 == b.x0 && a.y0 == b.y0 && a.x1 == b.x1 && a.y1 == b.y1 && a.empty == b.empty;
}

// Makes the batch state match this draw; flushes the previous batch when anything differs.
void Select(ID3D11ShaderResourceView* srv, int samp, bool opaque)
{
    if (s_b_srv != srv || s_b_samp != samp || s_b_opaque != opaque || !SameClip(s_b_clip, s_clip))
    {
        Flush();
        s_b_srv = srv;
        s_b_samp = samp;
        s_b_opaque = opaque;
        s_b_clip = s_clip;
    }
}

inline void PushTri(const Vtx& a, const Vtx& b, const Vtx& c)
{
    s_verts.push_back(a);
    s_verts.push_back(b);
    s_verts.push_back(c);
}

// A quad in logical coordinates (two triangles).
void QuadSrv(ID3D11ShaderResourceView* srv, int samp, bool opaque, float x0, float y0, float x1, float y1, float u0,
             float v0, float u1, float v1, u32 argb)
{
    if (!s_in_frame || !srv || s_clip.empty)
        return;
    Select(srv, samp, opaque);
    const float px0 = TX(x0), py0 = TY(y0), px1 = TX(x1), py1 = TY(y1);
    const Vtx a = {px0, py0, u0, v0, argb};
    const Vtx b = {px1, py0, u1, v0, argb};
    const Vtx c = {px1, py1, u1, v1, argb};
    const Vtx d = {px0, py1, u0, v1, argb};
    PushTri(a, b, c);
    PushTri(a, c, d);
}

// ---- textures ----------------------------------------------------------------------------------------------------

int SampOf(const PlatTexture* t)
{
    return t->mips ? 2 : (t->smooth ? 1 : 0);
}

// Next mip level, 2x2 box filter weighted by alpha so transparent texels do not bleed their colour.
void BuildNextLevel(const std::vector<u8>& src, int sw, int sh, std::vector<u8>& dst, int& dw, int& dh)
{
    dw = std::max(1, sw / 2);
    dh = std::max(1, sh / 2);
    dst.assign(static_cast<size_t>(dw) * static_cast<size_t>(dh) * 4, 0);
    for (int y = 0; y < dh; ++y)
    {
        const int y0 = std::min(y * 2, sh - 1), y1 = std::min(y * 2 + 1, sh - 1);
        for (int x = 0; x < dw; ++x)
        {
            const int x0 = std::min(x * 2, sw - 1), x1 = std::min(x * 2 + 1, sw - 1);
            const u8* p[4] = {&src[(static_cast<size_t>(y0) * sw + x0) * 4], &src[(static_cast<size_t>(y0) * sw + x1) * 4],
                              &src[(static_cast<size_t>(y1) * sw + x0) * 4], &src[(static_cast<size_t>(y1) * sw + x1) * 4]};
            u32 a = 0, r = 0, g = 0, b = 0;
            for (int k = 0; k < 4; ++k)
            {
                a += p[k][3];
                r += p[k][0] * p[k][3];
                g += p[k][1] * p[k][3];
                b += p[k][2] * p[k][3];
            }
            u8* o = &dst[(static_cast<size_t>(y) * dw + x) * 4];
            if (a)
            {
                o[0] = static_cast<u8>(r / a);
                o[1] = static_cast<u8>(g / a);
                o[2] = static_cast<u8>(b / a);
            }
            else
            {
                u32 rr = 0, gg = 0, bb = 0;
                for (int k = 0; k < 4; ++k)
                {
                    rr += p[k][0];
                    gg += p[k][1];
                    bb += p[k][2];
                }
                o[0] = static_cast<u8>(rr / 4);
                o[1] = static_cast<u8>(gg / 4);
                o[2] = static_cast<u8>(bb / 4);
            }
            o[3] = static_cast<u8>((a + 2) / 4);
        }
    }
}

bool EnsureUploaded(PlatTexture* t)
{
    if (t->srv && t->gen == s_gen)
        return true;
    if (t->failed_gen == s_gen || !s_dev)
        return false;
    t->srv = nullptr;
    if (t->width <= 0 || t->height <= 0 || t->width > 16384 || t->height > 16384 || t->rgba.size() < static_cast<size_t>(t->width) * t->height * 4)
    {
        t->failed_gen = s_gen;
        return false;
    }
    std::vector<std::vector<u8>> levels;
    std::vector<int> lw, lh;
    int levelCount = 1;
    if (t->mips)
    {
        int m = std::max(t->width, t->height);
        while (m > 1)
        {
            m >>= 1;
            ++levelCount;
        }
    }
    lw.push_back(t->width);
    lh.push_back(t->height);
    for (int i = 1; i < levelCount; ++i)
    {
        std::vector<u8> next;
        int nw, nh;
        BuildNextLevel(i == 1 ? t->rgba : levels[static_cast<size_t>(i) - 2], lw.back(), lh.back(), next, nw, nh);
        levels.push_back(std::move(next));
        lw.push_back(nw);
        lh.push_back(nh);
    }
    // levels[i-1] holds level i
    std::vector<D3D11_SUBRESOURCE_DATA> sr(static_cast<size_t>(levelCount));
    for (int i = 0; i < levelCount; ++i)
    {
        sr[static_cast<size_t>(i)].pSysMem = i == 0 ? t->rgba.data() : levels[static_cast<size_t>(i) - 1].data();
        sr[static_cast<size_t>(i)].SysMemPitch = static_cast<UINT>(lw[static_cast<size_t>(i)] * 4);
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = static_cast<UINT>(t->width);
    td.Height = static_cast<UINT>(t->height);
    td.MipLevels = static_cast<UINT>(levelCount);
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    winrt::com_ptr<ID3D11Texture2D> tex;
    if (FAILED(s_dev->CreateTexture2D(&td, sr.data(), tex.put())) ||
        FAILED(s_dev->CreateShaderResourceView(tex.get(), nullptr, t->srv.put())))
    {
        t->srv = nullptr;
        t->failed_gen = s_gen;
        Log("plat: GPU texture %dx%d could not be created", t->width, t->height);
        return false;
    }
    t->gen = s_gen;
    return true;
}

PlatTexture* TextureCreate(const u8* rgba, int width, int height, bool smooth, bool mips)
{
    if (!rgba || width <= 0 || height <= 0)
        return nullptr;
    PlatTexture* t = new (std::nothrow) PlatTexture;
    if (!t)
        return nullptr;
    t->width = width;
    t->height = height;
    t->smooth = smooth;
    t->mips = mips;
    try
    {
        t->rgba.assign(rgba, rgba + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    }
    catch (...)
    {
        delete t;
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(s_tex_mutex);
    s_live.insert(t);
    return t;
}

void DrainFrees()
{
    std::vector<PlatTexture*> q;
    {
        std::lock_guard<std::mutex> lk(s_tex_mutex);
        q.swap(s_free_queue);
        for (PlatTexture* t : q)
            s_live.erase(t);
    }
    for (PlatTexture* t : q)
        delete t;
}

// ---- the game picture ---------------------------------------------------------------------------------------------

bool EnsureGameTexture(int w, int h)
{
    if (s_game_tex && s_game_gen == s_gen && s_game_w == w && s_game_h == h)
        return true;
    s_game_srv = nullptr;
    s_game_tex = nullptr;
    if (!s_dev || w <= 0 || h <= 0 || w > 16384 || h > 16384)
        return false;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(s_dev->CreateTexture2D(&td, nullptr, s_game_tex.put())) ||
        FAILED(s_dev->CreateShaderResourceView(s_game_tex.get(), nullptr, s_game_srv.put())))
    {
        s_game_tex = nullptr;
        s_game_srv = nullptr;
        return false;
    }
    s_game_w = w;
    s_game_h = h;
    s_game_gen = s_gen;
    return true;
}

// REF plat_sdl.c:1609-1646 in logical units: the picture's rectangle.
void GameRect(const Settings* settings, float display_aspect, int lines, int& dx, int& dy, int& dw, int& dh)
{
    const int out_w = kCanvasW, out_h = kCanvasH;
    const float aspect = display_aspect > 0.0f ? display_aspect : 4.0f / 3.0f;
    dh = out_h;
    dw = static_cast<int>(static_cast<float>(out_h) * aspect + 0.5f);
    if (dw > out_w)
    {
        dw = out_w;
        dh = static_cast<int>(static_cast<float>(out_w) / aspect + 0.5f);
    }
    if (settings->integer_scale)
    {
        int k = dh / (lines > 0 ? lines : 240);
        if (k < 1)
            k = 1;
        dh = lines * k;
        dw = static_cast<int>(static_cast<float>(dh) * aspect + 0.5f);
        if (dw > out_w)
            dw = out_w;
    }
    if (settings->border == 2)
    {
        dw = dw * 86 / 100; // TV frame: leave room for the TV around the picture
        dh = dh * 86 / 100;
    }
    dx = (out_w - dw) / 2;
    dy = (out_h - dh) / 2;
    if (settings->manual_crop)
    {
        const int mw = settings->mc_w < 25 ? 25 : settings->mc_w > 150 ? 150 : settings->mc_w;
        const int mh = settings->mc_h < 25 ? 25 : settings->mc_h > 150 ? 150 : settings->mc_h;
        dw = dw * mw / 100;
        dh = dh * mh / 100;
        dx = (out_w - dw) / 2 + settings->mc_x * out_w / 1000;
        dy = (out_h - dh) / 2 + settings->mc_y * out_h / 1000;
    }
}

// ---- input helpers -----------------------------------------------------------------------------------------------

int Physical(int port)
{
    if (port < 0 || port >= kMaxPads)
        return -1;
    const int i = s_player_order[port];
    return (i >= 0 && i < kMaxPads) ? i : -1;
}

void SetVibration(int i, u32 strong, u32 weak)
{
    XINPUT_VIBRATION v = {};
    v.wLeftMotorSpeed = static_cast<WORD>(strong);
    v.wRightMotorSpeed = static_cast<WORD>(weak);
    XInputSetState(static_cast<DWORD>(i), &v);
}

void RumbleWatchdog(u64 now)
{
    for (int i = 0; i < kMaxPads; ++i)
    {
        if ((s_rumble_strong[i] || s_rumble_weak[i]) && now - s_rumble_at[i] > kRumbleWatchdogUs)
        {
            s_rumble_strong[i] = s_rumble_weak[i] = 0;
            if (s_connected[i])
                SetVibration(i, 0, 0);
        }
    }
}

void NotePresent()
{
    const u64 now = NowUs();
    if (s_present_last_us)
    {
        const double dt = static_cast<double>(now - s_present_last_us);
        if (dt > 3000.0 && dt < 250000.0)
        {
            s_present_ema_us = s_present_samples ? s_present_ema_us * 0.95 + dt * 0.05 : dt;
            if (s_present_samples < 1000)
                ++s_present_samples;
        }
    }
    s_present_last_us = now;
}

void EndFrameInternal(bool present)
{
    Flush();
    s_in_frame = false;
    if (!s_sc)
        return;
    if (!present)
        return;
    const HRESULT hr = s_sc->Present(1, 0);
    NotePresent();
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        Log("plat: Present returned 0x%08X, the device will be rebuilt", static_cast<unsigned>(hr));
        s_need_device = true;
    }
}

} // namespace

// ---- host-facing entry points (UI thread) ------------------------------------------------------------------------

extern "C" {

void PlatD3D11_SetWindow(IUnknown* core_window, int pixel_w, int pixel_h, float dpi_scale)
{
    std::lock_guard<std::mutex> lk(s_win_mutex);
    s_window = nullptr;
    if (core_window)
    {
        core_window->AddRef();
        s_window.attach(core_window);
    }
    s_win_w = pixel_w > 0 ? pixel_w : kCanvasW;
    s_win_h = pixel_h > 0 ? pixel_h : kCanvasH;
    s_dpi = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    s_win_changed = true;
}

void PlatD3D11_Resize(int pixel_w, int pixel_h, float dpi_scale)
{
    if (pixel_w <= 0 || pixel_h <= 0)
        return;
    std::lock_guard<std::mutex> lk(s_win_mutex);
    s_win_w = pixel_w;
    s_win_h = pixel_h;
    if (dpi_scale > 0.0f)
        s_dpi = dpi_scale;
    s_size_changed = true;
}

void PlatD3D11_Trim(void)
{
    // Called from the UI thread while the frontend thread may be drawing: the immediate context is not touched, only
    // IDXGIDevice3::Trim, which D3D11 allows from any thread (the device is free-threaded).
    std::lock_guard<std::mutex> lk(s_dev_mutex);
    if (!s_dev)
        return;
    winrt::com_ptr<IDXGIDevice3> d3;
    if (SUCCEEDED(s_dev->QueryInterface(__uuidof(IDXGIDevice3), d3.put_void())))
        d3->Trim();
}

void PlatD3D11_SetActive(bool active)
{
    s_active.store(active, std::memory_order_release);
    if (!active)
        s_key_bits.store(0, std::memory_order_release);
}

void PlatD3D11_Pointer(float x, float y, bool pressed, bool valid)
{
    std::lock_guard<std::mutex> lk(s_ptr_mutex);
    if (valid && s_ptr_have_last)
    {
        s_ptr_dx += x - s_ptr_last_x;
        s_ptr_dy += y - s_ptr_last_y;
    }
    s_ptr_have_last = valid;
    s_ptr_last_x = x;
    s_ptr_last_y = y;
    s_ptr_x = x;
    s_ptr_y = y;
    s_ptr_pressed = pressed;
    s_ptr_valid = valid;
}

void plat_xbox_device_lost(void)
{
    s_need_device = true;
}

void plat_xbox_key_event(uint32_t virtual_key, bool down)
{
    if (virtual_key >= 0xC3u && virtual_key <= 0xDAu)
        return; // VK_GAMEPAD_A .. VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT: the pad comes through XInput
    u32 bit = 0;
    switch (virtual_key) // REF desktop map, plat_sdl.c:883-896
    {
        case 'Z': bit = BIT(BTN_CROSS); break;
        case 'X': bit = BIT(BTN_CIRCLE); break;
        case 'A': bit = BIT(BTN_SQUARE); break;
        case 'S': bit = BIT(BTN_TRIANGLE); break;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT: bit = BIT(BTN_SELECT); break;
        case VK_RETURN: bit = BIT(BTN_START); break;
        case 'Q': bit = BIT(BTN_L1); break;
        case 'W': bit = BIT(BTN_R1); break;
        case '1': bit = BIT(BTN_L2); break;
        case '2': bit = BIT(BTN_R2); break;
        case VK_UP: bit = BIT(BTN_UP); break;
        case VK_DOWN: bit = BIT(BTN_DOWN); break;
        case VK_LEFT: bit = BIT(BTN_LEFT); break;
        case VK_RIGHT: bit = BIT(BTN_RIGHT); break;
        case VK_ESCAPE:
        case VK_TAB: bit = BIT(BTN_MENU); break;
        default: return;
    }
    if (down)
        s_key_bits.fetch_or(bit, std::memory_order_acq_rel);
    else
        s_key_bits.fetch_and(~bit, std::memory_order_acq_rel);
}

void plat_xbox_keys_reset(void)
{
    s_key_bits.store(0, std::memory_order_release);
}

float plat_xbox_ui_scale(void)
{
    return s_scale;
}

// Kept so code written against the DuckStation bridge links: the Direct3D picture needs no core display settings.
bool plat_xbox_sync_display_settings(const Settings* view)
{
    (void)view;
    return false;
}

// ---- lifecycle ---------------------------------------------------------------------------------------------------

bool plat_init(void)
{
    s_init_error[0] = '\0';
    if (s_gpu_alive)
        ReleaseGpuObjects();
    if (!BuildGpu())
    {
        if (!s_init_error[0])
            std::snprintf(s_init_error, sizeof(s_init_error), "Direct3D 11 initialisation failed");
        return false;
    }
    SetBaseState();
    std::snprintf(s_screen_info, sizeof(s_screen_info), "Direct3D 11, swap chain %dx%d (dpi x%.2f), UI 1920x1080 x%.3f at (%.0f,%.0f)",
                  s_sw, s_sh, static_cast<double>(s_dpi), static_cast<double>(s_scale), static_cast<double>(s_ox),
                  static_cast<double>(s_oy));
    RefreshClockPreference(NowUs(), true);
    for (int i = 0; i < kMaxPads; ++i)
    {
        s_connected[i] = false;
        s_last_probe[i] = 0;
        s_rumble_strong[i] = s_rumble_weak[i] = 0;
        s_rumble_at[i] = 0;
    }
    s_present_last_us = 0;
    s_present_samples = 0;
    s_in_frame = false;
    s_verts.reserve(1u << 15);
    s_initialized = true;
    s_gpu_alive = true;
    s_need_device = false;
    return true;
}

const char* plat_init_error(void)
{
    return s_init_error;
}

const char* plat_screen_info(void)
{
    return s_screen_info;
}

void plat_shutdown(void)
{
    plat_audio_close();
    for (int i = 0; i < kMaxPads; ++i)
    {
        if (s_connected[i])
            SetVibration(i, 0, 0);
        s_rumble_strong[i] = s_rumble_weak[i] = 0;
    }
    if (s_gpu_alive)
    {
        s_verts.clear();
        DrainFrees();
        ReleaseGpuObjects();
        s_gpu_alive = false;
    }
    if (s_backdrop_tex)
    {
        plat_texture_free(s_backdrop_tex);
        s_backdrop_tex = nullptr;
        s_backdrop_shown = nullptr;
        DrainFrees();
    }
    s_game_have = false;
    s_initialized = false;
}

void plat_release(void)
{
    plat_shutdown(); // idempotent
}

bool plat_prepare_storage(char* error, size_t size)
{
    if (error && size > 0)
        error[0] = '\0';
    return true; // the UWP LocalState is always listable
}

const char* plat_sandbox_probe(void)
{
    return "not applicable (UWP LocalState)";
}

bool plat_unlock_disabled(void)
{
    return false;
}

void plat_set_unlock_disabled(bool disabled)
{
    (void)disabled; // PS5 only
}

void plat_default_root(char* out, size_t size)
{
    if (!out || size == 0)
        return;
    const std::string root = DataRootPath();
    if (root.empty())
    {
        out[0] = '\0';
        Log("plat: no LocalState folder for the data root");
        return;
    }
    static bool registered = false; // once, before other threads (SWANUI_PORTING_NOTES.md section 9 "Data root")
    if (!registered)
    {
        registered = true;
        if (swanui_set_data_root(root.c_str()) != 0)
            Log("plat: swanui_set_data_root failed");
    }
    CopyPathSlashes(out, size, root + "\\Tsugaru");
}

// ---- input -------------------------------------------------------------------------------------------------------

void plat_poll(PadState pads[SwanStationPS5_MAX_PADS], bool* quit)
{
    (void)quit; // never set on Xbox: the app ends through the lifecycle events
    if (!pads)
        return;
    std::memset(pads, 0, sizeof(PadState) * kMaxPads);
    const u64 now = NowUs();
    PadState raw[kMaxPads] = {};
    for (int i = 0; i < kMaxPads; ++i)
    {
        if (!s_connected[i] && now - s_last_probe[i] < kPadProbeUs)
            continue; // probe empty slots once a second
        s_last_probe[i] = now;
        XINPUT_STATE st = {};
        if (XInputGetState(static_cast<DWORD>(i), &st) != ERROR_SUCCESS)
        {
            if (s_connected[i])
            {
                s_connected[i] = false;
                s_rumble_strong[i] = s_rumble_weak[i] = 0;
            }
            continue;
        }
        s_connected[i] = true;
        PadState& p = raw[i];
        const WORD b = st.Gamepad.wButtons;
        p.connected = true;
        if (b & XINPUT_GAMEPAD_A) p.buttons |= BIT(BTN_CROSS);
        if (b & XINPUT_GAMEPAD_B) p.buttons |= BIT(BTN_CIRCLE);
        if (b & XINPUT_GAMEPAD_X) p.buttons |= BIT(BTN_SQUARE);
        if (b & XINPUT_GAMEPAD_Y) p.buttons |= BIT(BTN_TRIANGLE);
        if (b & XINPUT_GAMEPAD_LEFT_SHOULDER) p.buttons |= BIT(BTN_L1);
        if (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) p.buttons |= BIT(BTN_R1);
        if (b & XINPUT_GAMEPAD_LEFT_THUMB) p.buttons |= BIT(BTN_L3);
        if (b & XINPUT_GAMEPAD_RIGHT_THUMB) p.buttons |= BIT(BTN_R3);
        if (b & XINPUT_GAMEPAD_START) p.buttons |= BIT(BTN_START); // Menu button
        if (b & XINPUT_GAMEPAD_BACK) p.buttons |= BIT(BTN_MENU);   // View button = the DualSense touchpad's role
        if (b & XINPUT_GAMEPAD_DPAD_UP) p.buttons |= BIT(BTN_UP);
        if (b & XINPUT_GAMEPAD_DPAD_DOWN) p.buttons |= BIT(BTN_DOWN);
        if (b & XINPUT_GAMEPAD_DPAD_LEFT) p.buttons |= BIT(BTN_LEFT);
        if (b & XINPUT_GAMEPAD_DPAD_RIGHT) p.buttons |= BIT(BTN_RIGHT);
        p.l2 = st.Gamepad.bLeftTrigger; // 0..255
        p.r2 = st.Gamepad.bRightTrigger;
        if (p.l2 > 93) // = SDL axis (v*32767/255) > 12000, REF plat_sdl.c:927-930
            p.buttons |= BIT(BTN_L2);
        if (p.r2 > 93)
            p.buttons |= BIT(BTN_R2);
        p.lx = st.Gamepad.sThumbLX;
        p.ly = static_cast<int16_t>(~st.Gamepad.sThumbLY); // XInput +Y is up; SDL/PS5 +Y is down (~v = -v-1, exact)
        p.rx = st.Gamepad.sThumbRX;
        p.ry = static_cast<int16_t>(~st.Gamepad.sThumbRY);
    }
    RumbleWatchdog(now);

    // players (order_pads, REF plat_sdl.c:529-535)
    PadState ordered[kMaxPads] = {};
    for (int i = 0; i < kMaxPads; ++i)
    {
        const int src = Physical(i);
        if (src >= 0)
            ordered[i] = raw[src];
    }

    if (!s_active.load(std::memory_order_acquire))
    {
        for (int i = 0; i < kMaxPads; ++i)
            pads[i].connected = ordered[i].connected; // neutral while the window is inactive (Guide, focus loss)
        return;
    }
    for (int i = 0; i < kMaxPads; ++i)
        pads[i] = ordered[i];

    const u32 keys = s_key_bits.load(std::memory_order_acquire); // PC keyboard
    if (keys)
    {
        pads[0].buttons |= keys;
        pads[0].connected = true;
    }

    // pointer: touch position (logical canvas) and mouse deltas on pad 0
    {
        std::lock_guard<std::mutex> lk(s_ptr_mutex);
        if (s_ptr_valid && s_pad_touch && s_ptr_pressed)
        {
            const float lx = (s_ptr_x - s_ox) / s_scale;
            const float ly = (s_ptr_y - s_oy) / s_scale;
            pads[0].touching = true;
            pads[0].touch_x = static_cast<uint16_t>(std::clamp(lx, 0.0f, static_cast<float>(kCanvasW - 1)));
            pads[0].touch_y = static_cast<uint16_t>(std::clamp(ly, 0.0f, static_cast<float>(kCanvasH - 1)));
            pads[0].connected = true;
        }
        if (s_ptr_dx != 0.0f || s_ptr_dy != 0.0f)
        {
            const float k = s_scale > 0.0f ? 1.0f / s_scale : 1.0f;
            pads[0].mouse_dx = static_cast<int16_t>(std::clamp(s_ptr_dx * k, -32767.0f, 32767.0f));
            pads[0].mouse_dy = static_cast<int16_t>(std::clamp(s_ptr_dy * k, -32767.0f, 32767.0f));
            s_ptr_dx = s_ptr_dy = 0.0f;
        }
    }
}

void plat_rumble(int port, uint16_t strong, uint16_t weak)
{
    const int i = Physical(port);
    if (i < 0 || !s_connected[i])
        return;
    if (strong || weak)
        s_rumble_at[i] = NowUs(); // always refresh the watchdog for a non-zero value
    if (s_rumble_strong[i] == strong && s_rumble_weak[i] == weak)
        return;
    s_rumble_strong[i] = strong;
    s_rumble_weak[i] = weak;
    SetVibration(i, strong, weak);
}

void plat_set_lightbar(int port, uint32_t rgb)
{
    (void)port; // no light bar on Xbox pads
    (void)rgb;
}

void plat_set_player_order(const int order[SwanStationPS5_MAX_PADS])
{
    if (!order)
        return;
    for (int i = 0; i < kMaxPads; ++i)
        s_player_order[i] = (order[i] >= 0 && order[i] < kMaxPads) ? order[i] : i;
}

void plat_player_order(int order[SwanStationPS5_MAX_PADS])
{
    if (!order)
        return;
    for (int i = 0; i < kMaxPads; ++i)
        order[i] = s_player_order[i];
}

const char* plat_pad_name(int port)
{
    (void)port;
    return nullptr; // the UI shows "Controller"
}

void plat_pad_motion(bool on)
{
    s_pad_motion = on; // no motion sensors on Xbox pads: the fields stay zero
}

void plat_pad_touch(bool on)
{
    s_pad_touch = on; // no touch pad on Xbox pads (the pointer fills it when there is one)
}

void plat_pad_triggers(int port, PlatTrigger l2, PlatTrigger r2)
{
    (void)port; // no adaptive triggers on Xbox pads
    (void)l2;
    (void)r2;
}

int plat_pad_battery(int port)
{
    (void)port;
    return -1; // unknown
}

// ---- picture options (applied by the game pixel shader, see plat_draw_game) ---------------------------------------

void plat_set_colour(int brightness, int colour, int sharpen)
{
    s_brightness = brightness < 0 ? 0 : brightness > 3 ? 3 : brightness;
    s_colour = colour < 0 ? 0 : colour > 5 ? 5 : colour;
    s_sharpen = sharpen < 0 ? 0 : sharpen > 2 ? 2 : sharpen;
}

void plat_set_fsr(bool on)
{
    s_fsr = on;
}

double plat_framegen_hz(void)
{
    return 0.0;
}

int plat_framegen_times(void)
{
    return 0;
}

void plat_set_framegen(int generated, double core_hz, double speed, bool nominal)
{
    (void)generated;
    (void)core_hz;
    (void)speed;
    (void)nominal;
}

void plat_want_high_refresh(bool on)
{
    (void)on; // the console picks the HDMI mode
}

bool plat_gpu_ui_active(void)
{
    return s_initialized; // turns on reflections and 3D cases (REF ui/coverflow.c)
}

bool plat_set_output(int resolution)
{
    (void)resolution;
    return false;
}

void plat_want_output(int resolution)
{
    s_want_output = resolution; // stored, no effect
}

void plat_clock(char* out, size_t size)
{
    if (!out || size == 0)
        return;
    RefreshClockPreference(NowUs(), false);
    const std::time_t t = std::time(nullptr);
    std::tm tm = {};
    if (localtime_s(&tm, &t) != 0)
    {
        out[0] = '\0';
        return;
    }
    const int hour = tm.tm_hour, minute = tm.tm_min;
    if (s_clock_24h)
        std::snprintf(out, size, "%02d:%02d", hour, minute);
    else
        std::snprintf(out, size, "%d:%02d %s", hour % 12 ? hour % 12 : 12, minute, hour < 12 ? "AM" : "PM");
}

int plat_system_language(void)
{
    std::string tag;
    try
    {
        const auto langs = winrt::Windows::System::UserProfile::GlobalizationPreferences::Languages();
        if (langs && langs.Size() > 0)
            tag = winrt::to_string(langs.GetAt(0)); // BCP-47
    }
    catch (...)
    {
    }
    if (tag.empty())
    {
        wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
        if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0)
        {
            for (const wchar_t* p = name; *p; ++p)
                tag.push_back(static_cast<char>(*p < 128 ? *p : '?'));
        }
    }
    std::string primary;
    for (char ch : tag)
    {
        if (ch == '-' || ch == '_')
            break;
        primary.push_back(static_cast<char>((ch >= 'A' && ch <= 'Z') ? ch + ('a' - 'A') : ch));
    }
    if (primary == "fr")
        return LANG_FR;
    if (primary == "pt")
        return LANG_PT;
    if (primary == "es")
        return LANG_ES;
    if (primary == "ja")
        return LANG_JA;
    return LANG_EN;
}

void plat_profile(const char* name)
{
    (void)name; // no per-frame profiler here
}

void plat_draw_backdrop(const uint8_t* grey, int w, int h, uint32_t tint_argb)
{
    if (!grey || w <= 0 || h <= 0)
        return;
    // the grey picture as a texture, remade when rewritten; the GPU tints and scales it
    if (!s_backdrop_tex || s_backdrop_dirty || s_backdrop_shown != grey || s_backdrop_w != w || s_backdrop_h != h)
    {
        if (s_backdrop_tex)
            plat_texture_free(s_backdrop_tex);
        s_backdrop_tex = nullptr;
        const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
        u8* px = static_cast<u8*>(std::malloc(n * 4));
        if (px)
        {
            for (size_t i = 0; i < n; ++i)
            {
                px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = grey[i];
                px[i * 4 + 3] = 255;
            }
            s_backdrop_tex = plat_texture_create(px, w, h, true);
            std::free(px);
        }
        s_backdrop_shown = grey;
        s_backdrop_w = w;
        s_backdrop_h = h;
        s_backdrop_dirty = false;
    }
    if (s_backdrop_tex)
        plat_draw_texture(s_backdrop_tex, 0.0f, 0.0f, static_cast<float>(kCanvasW), static_cast<float>(kCanvasH),
                          0xff000000u | (tint_argb & 0x00ffffffu), true);
}

void plat_backdrop_changed(void)
{
    s_backdrop_dirty = true;
}

// ---- audio -------------------------------------------------------------------------------------------------------

bool plat_audio_open(int sample_rate)
{
    if (sample_rate <= 0)
        return false;
    if (s_voice && s_rate == sample_rate)
    {
        RingClear();
        return true;
    }
    plat_audio_close();
    {
        std::lock_guard<std::mutex> lk(s_ring_mutex);
        s_ring_frames = static_cast<size_t>(sample_rate) * 4; // 4 s
        s_ring.assign(s_ring_frames * 2, 0);
        s_read = s_write = 0;
    }
    s_audio_pos = 0.0;
    s_audio_last[0] = s_audio_last[1] = 0;

    HRESULT hr = XAudio2Create(s_xa.put(), 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (SUCCEEDED(hr))
        hr = s_xa->CreateMasteringVoice(&s_master, 2, 0, 0, nullptr, nullptr);
    if (SUCCEEDED(hr))
    {
        WAVEFORMATEX fmt = {};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 2;
        fmt.nSamplesPerSec = static_cast<DWORD>(sample_rate);
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 4;
        fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * 4;
        hr = s_xa->CreateSourceVoice(&s_voice, &fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &s_audio_cb, nullptr, nullptr);
    }
    if (FAILED(hr))
    {
        Log("audio open failed: 0x%08X", static_cast<unsigned>(hr));
        if (s_master)
        {
            s_master->DestroyVoice();
            s_master = nullptr;
        }
        s_xa = nullptr;
        std::lock_guard<std::mutex> lk(s_ring_mutex);
        s_ring_frames = 0;
        s_ring.clear();
        return false;
    }
    s_rate = sample_rate;
    s_audio_live.store(true, std::memory_order_release);
    for (int i = 0; i < kAudioBuffers; ++i)
        AudioSubmit(i);
    s_voice->Start(0);
    Log("audio: %d Hz (XAudio2)", sample_rate);
    return true;
}

void plat_audio_close(void)
{
    s_audio_live.store(false, std::memory_order_release);
    // DestroyVoice waits for a running callback, which takes the ring mutex: never hold it here.
    if (s_voice)
    {
        s_voice->Stop(0);
        s_voice->DestroyVoice();
        s_voice = nullptr;
    }
    if (s_master)
    {
        s_master->DestroyVoice();
        s_master = nullptr;
    }
    s_xa = nullptr;
    std::lock_guard<std::mutex> lk(s_ring_mutex);
    s_ring_frames = 0;
    s_ring.clear();
    s_read = s_write = 0;
    s_rate = 0;
}

void plat_audio_push(const int16_t* frames, size_t frame_count)
{
    if (!s_voice || !frames || !frame_count)
        return;
    if (s_audio_ratio > 0.9999 && s_audio_ratio < 1.0001)
    {
        s_audio_pos = 0;
        s_audio_last[0] = frames[frame_count * 2 - 2];
        s_audio_last[1] = frames[frame_count * 2 - 1];
        RingPush(frames, frame_count);
        return;
    }
    // REF plat_sdl.c:1067-1098: out[k] is read at audio_pos + k * ratio, linear interpolation
    const size_t need = static_cast<size_t>(static_cast<double>(frame_count) / s_audio_ratio) + 3;
    if (s_stretch_out.size() < need * 2)
        s_stretch_out.resize(need * 2);
    size_t n = 0;
    double p = s_audio_pos;
    while (p < static_cast<double>(frame_count) && n < need)
    {
        const size_t i = static_cast<size_t>(p);
        const double f = p - static_cast<double>(i);
        for (int c = 0; c < 2; ++c)
        {
            const double a = (i == 0) ? s_audio_last[c] : frames[(i - 1) * 2 + c];
            const double b = frames[i * 2 + c];
            s_stretch_out[n * 2 + c] = static_cast<s16>(a + (b - a) * f);
        }
        ++n;
        p += s_audio_ratio;
    }
    s_audio_pos = p - static_cast<double>(frame_count);
    s_audio_last[0] = frames[frame_count * 2 - 2];
    s_audio_last[1] = frames[frame_count * 2 - 1];
    if (n)
        RingPush(s_stretch_out.data(), n);
}

size_t plat_audio_queued_frames(void)
{
    std::lock_guard<std::mutex> lk(s_ring_mutex);
    return s_voice ? static_cast<size_t>(s_write - s_read) : 0;
}

void plat_audio_clear(void)
{
    RingClear();
}

void plat_audio_set_ratio(double ratio)
{
    s_audio_ratio = (ratio > 0.0) ? ratio : 1.0;
}

double plat_display_hz(void)
{
    if (s_present_samples < 30 || s_present_ema_us <= 0.0)
        return 60.0;
    const double hz = 1000000.0 / s_present_ema_us;
    static const double common[] = {24.0, 25.0, 30.0, 48.0, 50.0, 60.0, 72.0, 75.0, 90.0, 100.0, 120.0, 144.0};
    for (double c : common)
    {
        if (std::fabs(hz - c) <= c * 0.03)
            return c;
    }
    return hz;
}

// ---- drawing -----------------------------------------------------------------------------------------------------

int plat_width(void)
{
    return kCanvasW; // the logical canvas; never 0 (callers divide by it)
}

int plat_height(void)
{
    return kCanvasH;
}

void plat_begin_frame(uint32_t clear_argb)
{
    ++s_frame_serial;
    s_in_frame = false;
    s_verts.clear();
    s_b_srv = nullptr;
    DrainFrees();
    if (!s_gpu_alive)
        return;

    // window / device changes are applied here, on the frontend thread
    bool rebuild = s_need_device;
    bool resize = false;
    {
        std::lock_guard<std::mutex> lk(s_win_mutex);
        if (s_win_changed)
            rebuild = true;
        else if (s_size_changed)
            resize = true;
    }
    if (rebuild)
    {
        Log("plat: rebuilding the Direct3D device and swap chain");
        ReleaseGpuObjects();
        ++s_gen;
        s_need_device = false;
        if (!BuildGpu())
        {
            Log("plat: rebuild failed: %s", s_init_error);
            s_need_device = true; // try again next frame
            return;
        }
        SetBaseState();
    }
    else if (resize)
    {
        int w, h;
        {
            std::lock_guard<std::mutex> lk(s_win_mutex);
            w = s_win_w;
            h = s_win_h;
            s_size_changed = false;
        }
        if (s_sc && (w != s_sw || h != s_sh))
        {
            s_ctx->OMSetRenderTargets(0, nullptr, nullptr);
            s_rtv = nullptr;
            const HRESULT hr = s_sc->ResizeBuffers(0, static_cast<UINT>(w), static_cast<UINT>(h), DXGI_FORMAT_UNKNOWN, 0);
            if (FAILED(hr) || !CreateTargets())
            {
                Log("plat: ResizeBuffers failed (0x%08X), rebuilding the device", static_cast<unsigned>(hr));
                s_need_device = true;
                return;
            }
            SetBaseState();
        }
    }
    if (!s_rtv || !s_ctx)
        return;

    UpdateTransform();
    SetBaseState(); // flip-model swap chains unbind the target on Present
    const u32 c = (clear_argb >> 24) ? clear_argb : 0u;
    const float col[4] = {static_cast<float>((c >> 16) & 0xff) / 255.0f, static_cast<float>((c >> 8) & 0xff) / 255.0f,
                          static_cast<float>(c & 0xff) / 255.0f, 1.0f};
    s_ctx->ClearRenderTargetView(s_rtv.get(), col);
    s_clip_on = false;
    s_clip.x0 = 0;
    s_clip.y0 = 0;
    s_clip.x1 = s_sw;
    s_clip.y1 = s_sh;
    s_clip.empty = false;
    s_b_clip = s_clip;
    s_in_frame = true;
}

// Scale2x (AdvMAME2x) on XRGB8888, as REF plat_sdl.c: edges keep their colour, everything else is duplicated.
static void Scale2x(const u32* src, int w, int h, size_t pitch_px, u32* dst)
{
    const int dw = w * 2;
    for (int y = 0; y < h; ++y)
    {
        const u32* up = src + static_cast<size_t>(y > 0 ? y - 1 : y) * pitch_px;
        const u32* row = src + static_cast<size_t>(y) * pitch_px;
        const u32* dn = src + static_cast<size_t>(y < h - 1 ? y + 1 : y) * pitch_px;
        u32* o0 = dst + static_cast<size_t>(y) * 2 * dw;
        u32* o1 = o0 + dw;
        for (int x = 0; x < w; ++x)
        {
            const u32 B = up[x], D = row[x > 0 ? x - 1 : x], E = row[x];
            const u32 F = row[x < w - 1 ? x + 1 : x], H = dn[x];
            const bool edge = B != H && D != F;
            o0[x * 2] = edge && D == B ? D : E;
            o0[x * 2 + 1] = edge && B == F ? F : E;
            o1[x * 2] = edge && D == H ? D : E;
            o1[x * 2 + 1] = edge && H == F ? F : E;
        }
    }
}

// Converts one row of the core's frame to opaque BGRA8.
static void ConvertRow(const u8* row, int pixel_format, int width, u32* dst)
{
    if (pixel_format == 1) // XRGB8888 (BGRA in memory): alpha forced opaque
    {
        const u32* s = reinterpret_cast<const u32*>(row);
        for (int x = 0; x < width; ++x)
            dst[x] = s[x] | 0xff000000u;
    }
    else if (pixel_format == 2) // RGB565
    {
        const uint16_t* s = reinterpret_cast<const uint16_t*>(row);
        for (int x = 0; x < width; ++x)
        {
            const u32 v = s[x];
            const u32 r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
            dst[x] = 0xff000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
        }
    }
    else // 0RGB1555
    {
        const uint16_t* s = reinterpret_cast<const uint16_t*>(row);
        for (int x = 0; x < width; ++x)
        {
            const u32 v = s[x];
            const u32 r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
            dst[x] = 0xff000000u | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
        }
    }
}

void plat_upload_game(const void* pixels, int width, int height, size_t pitch, int pixel_format, int upscale,
                      int filter)
{
    if (!pixels || width <= 0 || height <= 0 || !s_gpu_alive || !s_ctx)
        return;

    // Prescale (REF plat_sdl.c): UPSCALE_SHARP needs no CPU pass here, the GPU samples the texels point-wise at any
    // size; UPSCALE_SMOOTH_PIXELS runs Scale2x once (2x, 3x) or twice (4x); UPSCALE_XBR runs the frontend's xBR 2x
    // once (REF: one pass, hi-res frames over 400 px wide are left alone). Textures stay within 4096x2048.
    int k = 1;
    if (upscale > 1 && filter != UPSCALE_SHARP)
    {
        k = filter == UPSCALE_XBR ? (width > 400 ? 1 : 2) : (upscale >= 4 ? 4 : 2);
        while (k > 1 && (width * k > 4096 || height * k > 2048))
            k /= 2;
    }

    const u8* src = static_cast<const u8*>(pixels);
    if (k > 1)
    {
        const size_t n = static_cast<size_t>(width) * height;
        s_game_conv.resize(n);
        for (int y = 0; y < height; ++y)
            ConvertRow(src + static_cast<size_t>(y) * pitch, pixel_format, width, s_game_conv.data() + static_cast<size_t>(y) * width);
        s_game_scaled.resize(n * static_cast<size_t>(k) * k);
        if (filter == UPSCALE_XBR)
            xbr2x(s_game_conv.data(), width, height, static_cast<size_t>(width), s_game_scaled.data());
        else if (k == 2)
            Scale2x(s_game_conv.data(), width, height, static_cast<size_t>(width), s_game_scaled.data());
        else // 4x: Scale2x twice
        {
            static std::vector<u32> two;
            two.resize(n * 4);
            Scale2x(s_game_conv.data(), width, height, static_cast<size_t>(width), two.data());
            Scale2x(two.data(), width * 2, height * 2, static_cast<size_t>(width) * 2, s_game_scaled.data());
        }
    }

    const int tw = width * k, th = height * k;
    if (!EnsureGameTexture(tw, th))
        return;
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(s_ctx->Map(s_game_tex.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
        return;
    for (int y = 0; y < th; ++y)
    {
        u32* dst = reinterpret_cast<u32*>(static_cast<u8*>(m.pData) + static_cast<size_t>(y) * m.RowPitch);
        if (k > 1)
            std::memcpy(dst, s_game_scaled.data() + static_cast<size_t>(y) * tw, static_cast<size_t>(tw) * 4);
        else
            ConvertRow(src + static_cast<size_t>(y) * pitch, pixel_format, width, dst);
    }
    s_ctx->Unmap(s_game_tex.get(), 0);
    s_src_w = width;
    s_src_h = height;
    s_prescale = k;
    s_game_have = true;
}

void plat_upload_game_gpu(int width, int height)
{
    (void)width; // the core renders into the frontend's own texture only through plat_upload_game here
    (void)height;
}

void plat_draw_game(const Settings* settings, float display_aspect, uint8_t dim)
{
    if (!settings || !s_in_frame || !s_game_have || !s_game_srv || s_game_gen != s_gen)
        return;
    // Scanlines the game actually has: interlaced (480) and 2x-internal frames show at the same size as 240-line ones.
    int lines = s_src_h;
    while (lines > 288)
        lines /= 2;
    // Crop black edges: the share of the picture's height cut at the top and at the bottom (8 or 16 of 240 lines).
    const float crop =
        (!settings->manual_crop && settings->crop_edges > 0 && settings->crop_edges < 3 ? settings->crop_edges * 8 : 0) / 240.0f;

    int dx, dy, dw, dh;
    GameRect(settings, display_aspect, lines, dx, dy, dw, dh);
    s_game_rect[0] = dx;
    s_game_rect[1] = dy;
    s_game_rect[2] = dw;
    s_game_rect[3] = dh;
    if (dw <= 0 || dh <= 0)
        return;

    // ---- the picture's options -> cbuffer Picture (b1) ----
    // settings->shader (SwanStationPS5.h): 0 off, 1 LCD3x, 2 CRT Royale Fast, 3/4/11 retired, 5-8 NTSC (320/256 px,
    // S-Video/Composite), 9 CRT Lottes, 10 CRT Pi, 12 zfast LCD, 13 CRT Royale, 14 Newpixie, 15 Hyllian NTSC, 16 Mattias.
    struct Look
    {
        float beam, mask, corner, grid, sub, ntsc_px, ring;
    };
    static const Look kLooks[17] = {
        /* 0 off            */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 1 LCD3x          */ {0.00f, 0.00f, 0.0f, 0.55f, 0.6f, 0.0f, 0.0f},
        /* 2 Royale Fast    */ {0.60f, 0.25f, 0.5f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 3 retired        */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 4 retired        */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 5 NTSC 320 SVid  */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 1.0f, 0.0f},
        /* 6 NTSC 320 Comp  */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 2.0f, 0.35f},
        /* 7 NTSC 256 SVid  */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 1.0f, 0.0f},
        /* 8 NTSC 256 Comp  */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 2.0f, 0.35f},
        /* 9 CRT Lottes     */ {0.70f, 0.35f, 1.0f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 10 CRT Pi        */ {0.50f, 0.15f, 0.0f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 11 retired       */ {0.00f, 0.00f, 0.0f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 12 zfast LCD     */ {0.00f, 0.00f, 0.0f, 0.35f, 0.3f, 0.0f, 0.0f},
        /* 13 CRT Royale    */ {0.80f, 0.40f, 1.0f, 0.00f, 0.0f, 0.0f, 0.0f},
        /* 14 CRT Newpixie  */ {0.60f, 0.20f, 0.5f, 0.00f, 0.0f, 0.6f, 0.0f},
        /* 15 Hyllian NTSC  */ {0.60f, 0.25f, 0.5f, 0.00f, 0.0f, 1.0f, 0.0f},
        /* 16 CRT Mattias   */ {0.50f, 0.30f, 1.0f, 0.00f, 0.0f, 0.5f, 0.2f},
    };
    const int shader_id = settings->shader >= 0 && settings->shader < 17 ? settings->shader : 0;
    Look look = kLooks[shader_id];
    // NTSC blur widths are given for a 320 (or 256) column signal: scale them to this frame's columns.
    if (look.ntsc_px > 0.0f)
    {
        const float columns = (shader_id == 7 || shader_id == 8) ? 256.0f : 320.0f;
        look.ntsc_px *= static_cast<float>(s_game_w) / columns;
    }
    static const float kGain[4] = {0.80f, 1.00f, 1.15f, 1.30f};
    float saturation = 1.0f, temperature = 0.0f, mono = 0.0f;
    switch (s_colour)
    {
    case 1: saturation = 1.35f; break;          // vivid
    case 2: saturation = 0.80f; break;          // soft
    case 3: temperature = 1.0f; break;          // warm
    case 4: temperature = -1.0f; break;         // cool
    case 5: mono = 1.0f; break;                 // black and white
    default: break;
    }
    static const float kSharpen[3] = {0.0f, 0.45f, 0.9f};
    const bool fsr = s_fsr && !settings->integer_scale;
    // FSR implies a smooth, edge-sharpened scale; otherwise sharp texels unless the Smooth setting asks for bilinear.
    const bool sharp_sampling = !fsr && !settings->smooth;
    const float cb[6][4] = {
        {static_cast<float>(s_game_w), static_cast<float>(s_game_h), 1.0f / static_cast<float>(s_game_w),
         1.0f / static_cast<float>(s_game_h)},
        {kGain[s_brightness], saturation, temperature, mono},
        {kSharpen[s_sharpen] + (fsr ? 0.25f : 0.0f) > 0.9f ? 0.9f : kSharpen[s_sharpen] + (fsr ? 0.25f : 0.0f),
         fsr ? 1.0f : 0.0f, sharp_sampling ? 1.0f : 0.0f, static_cast<float>(lines)},
        {look.ntsc_px, look.ring, 0.0f, 0.0f},
        {look.beam, look.mask, look.corner, look.grid},
        {look.sub, static_cast<float>(dw) * s_scale, static_cast<float>(dh) * s_scale, 0.0f},
    };

    // The picture goes through its own pixel shader (one draw, outside the UI batches); the UI shader is put back after.
    Flush();
    s_ctx->UpdateSubresource(s_cb_game.get(), 0, nullptr, cb, 0, 0);
    ID3D11Buffer* cbp = s_cb_game.get();
    s_ctx->PSSetConstantBuffers(1, 1, &cbp);
    s_ctx->PSSetShader(s_ps_game.get(), nullptr, 0);
    QuadSrv(s_game_srv.get(), 1 /* linear: the shader snaps or filters by itself */, true, static_cast<float>(dx),
            static_cast<float>(dy), static_cast<float>(dx + dw), static_cast<float>(dy + dh), 0.0f, crop, 1.0f,
            1.0f - crop, 0xffffffffu);
    Flush();
    s_ctx->PSSetShader(s_ps.get(), nullptr, 0);

    // over the picture: scanlines or one dim fill (REF plat_sdl.c:1647-1668)
    static const uint8_t scan_strength[] = {0, 110, 200};
    const uint8_t scan = scan_strength[static_cast<unsigned>(settings->crt) % 3u];
    if (scan && lines > 0)
    {
        for (int y = 0; y < dh; ++y)
        {
            const int64_t pos = (static_cast<int64_t>(y) * 2 + 1) * lines * 128 / dh;
            const int phase = static_cast<int>(pos & 255) - 128;
            const uint32_t edge = static_cast<uint32_t>(phase * phase) >> 6;
            const uint32_t keep = static_cast<uint32_t>(dim) * (256 - ((edge * scan) >> 8)) >> 8;
            const uint32_t argb = (255u - (keep > 255u ? 255u : keep)) << 24;
            if (argb >> 24)
                plat_fill_rect(dx, dy + y, dw, 1, argb);
        }
    }
    else if (dim < 255)
    {
        plat_fill_rect(dx, dy, dw, dh, static_cast<uint32_t>(255 - dim) << 24);
    }
}

void plat_fill_rect(int x, int y, int w, int h, uint32_t argb)
{
    if ((argb >> 24) == 0 || w <= 0 || h <= 0)
        return;
    QuadSrv(s_white.get(), 0, false, static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + w),
            static_cast<float>(y + h), 0.0f, 0.0f, 1.0f, 1.0f, argb);
}

void plat_game_rect(int* x, int* y, int* w, int* h)
{
    if (x) *x = s_game_rect[0];
    if (y) *y = s_game_rect[1];
    if (w) *w = s_game_rect[2];
    if (h) *h = s_game_rect[3];
}

void plat_end_frame(void)
{
    if (!s_gpu_alive)
        return;
    EndFrameInternal(s_rtv != nullptr);
}

// ---- textures and meshes -----------------------------------------------------------------------------------------

PlatTexture* plat_texture_create(const uint8_t* rgba, int width, int height, bool smooth)
{
    return TextureCreate(rgba, width, height, smooth, false);
}

PlatTexture* plat_texture_create_mips(const uint8_t* rgba, int width, int height)
{
    return TextureCreate(rgba, width, height, true, true);
}

void plat_texture_free(PlatTexture* texture)
{
    if (!texture)
        return;
    std::lock_guard<std::mutex> lk(s_tex_mutex);
    if (s_live.find(texture) == s_live.end() || texture->freed)
        return;
    texture->freed = true;
    s_free_queue.push_back(texture); // still drawable this frame; destroyed at the next plat_begin_frame
}

void plat_texture_size(const PlatTexture* texture, int* width, int* height)
{
    if (width)
        *width = texture ? texture->width : 0;
    if (height)
        *height = texture ? texture->height : 0;
}

void plat_draw_mesh(PlatTexture* texture, const PlatVertex* vertices, int vertex_count, const int* indices,
                    int index_count)
{
    if (!vertices || vertex_count <= 0 || !s_in_frame || s_clip.empty)
        return;
    ID3D11ShaderResourceView* srv;
    int samp;
    if (texture)
    {
        if (!EnsureUploaded(texture))
            return;
        srv = texture->srv.get();
        samp = SampOf(texture);
    }
    else
    {
        srv = s_white.get();
        samp = 0;
    }
    if (!srv)
        return;
    Select(srv, samp, false);
    const auto conv = [](const PlatVertex& pv) {
        Vtx v;
        v.x = TX(pv.x);
        v.y = TY(pv.y);
        v.u = pv.u;
        v.v = pv.v;
        v.col = pv.argb;
        return v;
    };
    if (!indices)
    {
        const int n = vertex_count - vertex_count % 3;
        for (int i = 0; i < n; ++i)
            s_verts.push_back(conv(vertices[i]));
        return;
    }
    const int ni = index_count - index_count % 3;
    for (int i = 0; i < ni; i += 3)
    {
        const int a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a < 0 || b < 0 || c < 0 || a >= vertex_count || b >= vertex_count || c >= vertex_count)
            continue; // drop a triangle whose index is outside the vertices
        PushTri(conv(vertices[a]), conv(vertices[b]), conv(vertices[c]));
    }
}

void plat_draw_texture(PlatTexture* texture, float x, float y, float w, float h, uint32_t tint, bool blend)
{
    if (!texture || (tint >> 24) == 0 || !s_in_frame || !EnsureUploaded(texture))
        return;
    const int x0 = static_cast<int>(x + 0.5f), y0 = static_cast<int>(y + 0.5f);
    const int x1 = x0 + static_cast<int>(w + 0.5f), y1 = y0 + static_cast<int>(h + 0.5f);
    // blend=false ignores the picture's alpha (blend state off, alpha forced opaque)
    QuadSrv(texture->srv.get(), SampOf(texture), !blend, static_cast<float>(x0), static_cast<float>(y0),
            static_cast<float>(x1), static_cast<float>(y1), 0.0f, 0.0f, 1.0f, 1.0f, blend ? tint : (tint | 0xff000000u));
}

void plat_draw_texture_region(PlatTexture* texture, int sx, int sy, int sw, int sh, float x, float y, float w, float h,
                              uint32_t tint)
{
    if (!texture || (tint >> 24) == 0 || texture->width <= 0 || texture->height <= 0 || !s_in_frame ||
        !EnsureUploaded(texture))
        return;
    const int gx0 = static_cast<int>(x + 0.5f), gy0 = static_cast<int>(y + 0.5f);
    const int gw = static_cast<int>(x + w + 0.5f) - gx0, gh = static_cast<int>(y + h + 0.5f) - gy0;
    if (gw <= 0 || gh <= 0)
        return;
    const float tw = static_cast<float>(texture->width), th = static_cast<float>(texture->height);
    QuadSrv(texture->srv.get(), SampOf(texture), false, static_cast<float>(gx0), static_cast<float>(gy0),
            static_cast<float>(gx0 + gw), static_cast<float>(gy0 + gh), static_cast<float>(sx) / tw,
            static_cast<float>(sy) / th, static_cast<float>(sx + sw) / tw, static_cast<float>(sy + sh) / th, tint);
}

void plat_fill_rectf(float x, float y, float w, float h, uint32_t argb)
{
    if ((argb >> 24) == 0)
        return;
    const int x0 = static_cast<int>(x + 0.5f), y0 = static_cast<int>(y + 0.5f);
    const int rw = static_cast<int>(x + w + 0.5f) - x0, rh = static_cast<int>(y + h + 0.5f) - y0;
    if (rw <= 0 || rh <= 0)
        return;
    QuadSrv(s_white.get(), 0, false, static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x0 + rw),
            static_cast<float>(y0 + rh), 0.0f, 0.0f, 1.0f, 1.0f, argb);
}

void plat_set_clip(int x, int y, int w, int h)
{
    if (!s_in_frame)
        return;
    ClipRect c;
    if (w <= 0 || h <= 0) // clipping off: the whole swap chain
    {
        c.x0 = 0;
        c.y0 = 0;
        c.x1 = s_sw;
        c.y1 = s_sh;
        s_clip_on = false;
    }
    else
    {
        c.x0 = std::clamp(static_cast<int>(std::lround(TX(static_cast<float>(x)))), 0, s_sw);
        c.y0 = std::clamp(static_cast<int>(std::lround(TY(static_cast<float>(y)))), 0, s_sh);
        c.x1 = std::clamp(static_cast<int>(std::lround(TX(static_cast<float>(x + w)))), 0, s_sw);
        c.y1 = std::clamp(static_cast<int>(std::lround(TY(static_cast<float>(y + h)))), 0, s_sh);
        c.empty = (c.x1 <= c.x0 || c.y1 <= c.y0); // draws nothing, like a zero scissor
        s_clip_on = true;
    }
    s_clip = c;
}

void plat_asset_path(char* out, size_t size, const char* relative)
{
    if (!out || size == 0)
        return;
    // <InstalledLocation>/swanui/assets/<relative> with '/' inside the frontend
    std::string path = AppRootPath();
    if (path.empty())
        path = "swanui/assets";
    else
        path += "\\swanui\\assets";
    if (relative && *relative)
    {
        const char* rel = relative;
        while (*rel == '/' || *rel == '\\')
            ++rel;
        if (*rel)
        {
            path.push_back('/');
            path.append(rel);
        }
    }
    CopyPathSlashes(out, size, path);
}

// ---- time and notices --------------------------------------------------------------------------------------------

uint64_t plat_ticks_us(void)
{
    return NowUs();
}

void plat_sleep_us(uint32_t us)
{
    if (us == 0)
    {
        std::this_thread::yield();
        return;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

void plat_notify(const char* message)
{
    if (!message)
        return;
    Log("notify: %s", message); // the UI draws its own toasts; this is the log line (and a debugger string)
    OutputDebugStringA("notify: ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

} // extern "C"
