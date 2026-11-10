// Tsugaru UWP - UI batcher shaders and the game-picture shader (platform layer, bridge/plat_d3d11.cpp).
// Compiled offline:  fxc /nologo /T vs_4_0 /E VSMain /Vn g_plat_vs      /Fh plat_d3d11_vs.h      plat_d3d11.hlsl
//                    fxc /nologo /T ps_4_0 /E PSMain /Vn g_plat_ps      /Fh plat_d3d11_ps.h      plat_d3d11.hlsl
//                    fxc /nologo /T ps_4_0 /E PSGame /Vn g_plat_ps_game /Fh plat_d3d11_game_ps.h plat_d3d11.hlsl
// SPDX-License-Identifier: GPL-3.0-or-later

cbuffer Screen : register(b0)
{
    float2 g_scale; // 2 / swap-chain width, 2 / height
    float2 g_pad;
};

// The game picture's options (plat_draw_game fills it every frame it draws the picture).
cbuffer Picture : register(b1)
{
    float4 g_tex;   // texture w, h, 1/w, 1/h
    float4 g_grade; // brightness gain, saturation, temperature (-1 cool .. +1 warm), mono (0/1)
    float4 g_filt;  // sharpen amount (0..1), fsr (0/1), sharp sampling (0/1), source lines (for the CRT beam)
    float4 g_ntsc;  // chroma blur in texels, luma ringing (0..1), 0, 0
    float4 g_crt;   // beam (0..1), mask (0..1), corner (0..1), lcd grid (0..1)
    float4 g_lcd;   // lcd subpixels (0..1), destination w (px), destination h (px), 0
};

struct VSIn
{
    float2 pos : POSITION;   // swap-chain pixels
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;     // B8G8R8A8_UNORM input: ARGB word -> rgba
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos = float4(i.pos.x * g_scale.x - 1.0, 1.0 - i.pos.y * g_scale.y, 0.0, 1.0);
    o.uv = i.uv;
    o.col = i.col;
    return o;
}

Texture2D    g_texture : register(t0);
SamplerState g_smp     : register(s0); // UI: the texture's own sampler; game pass: always linear

// ---- UI --------------------------------------------------------------------------------------------------------------

float4 PSMain(VSOut i) : SV_Target
{
    return g_texture.Sample(g_smp, i.uv) * i.col;
}

// ---- game picture ------------------------------------------------------------------------------------------------------

static const float PI = 3.14159265;

float3 Tap(float2 uv)
{
    return g_texture.SampleLevel(g_smp, uv, 0).rgb;
}

// Catmull-Rom bicubic with 9 bilinear fetches (the FSR 1 upscale stand-in: EASU-like edge sharpness).
float3 CatmullRom(float2 uv)
{
    float2 size = g_tex.xy, inv = g_tex.zw;
    float2 sp = uv * size - 0.5;
    float2 tc = floor(sp) + 0.5;
    float2 f = sp - (tc - 0.5);
    float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    float2 w3 = f * f * (-0.5 + 0.5 * f);
    float2 w12 = w1 + w2;
    float2 off12 = w2 / w12;
    float2 t0 = (tc - 1.0) * inv;
    float2 t3 = (tc + 2.0) * inv;
    float2 t12 = (tc + off12) * inv;
    float3 r = 0;
    r += Tap(float2(t0.x, t0.y)) * w0.x * w0.y;
    r += Tap(float2(t12.x, t0.y)) * w12.x * w0.y;
    r += Tap(float2(t3.x, t0.y)) * w3.x * w0.y;
    r += Tap(float2(t0.x, t12.y)) * w0.x * w12.y;
    r += Tap(float2(t12.x, t12.y)) * w12.x * w12.y;
    r += Tap(float2(t3.x, t12.y)) * w3.x * w12.y;
    r += Tap(float2(t0.x, t3.y)) * w0.x * w3.y;
    r += Tap(float2(t12.x, t3.y)) * w12.x * w3.y;
    r += Tap(float2(t3.x, t3.y)) * w3.x * w3.y;
    return max(r, 0.0);
}

// Where the picture is read for this output pixel: snapped to the texel centre for sharp pixels.
float2 SourceUv(float2 uv)
{
    if (g_filt.z > 0.5)
        return (floor(uv * g_tex.xy) + 0.5) * g_tex.zw;
    return uv;
}

float3 Fetch(float2 uv)
{
    float3 c;
    if (g_filt.y > 0.5)
        c = CatmullRom(uv);
    else
        c = Tap(SourceUv(uv));
    return c;
}

// NTSC: luma stays, chroma (I/Q) is blurred sideways; composite adds a little ringing on the luma edges.
float3 Ntsc(float2 uv, float3 base)
{
    float blur = g_ntsc.x;
    if (blur <= 0.0)
        return base;
    const float3x3 toYiq = float3x3(0.299, 0.587, 0.114, 0.596, -0.274, -0.322, 0.211, -0.523, 0.312);
    const float3x3 toRgb = float3x3(1.0, 0.956, 0.621, 1.0, -0.272, -0.647, 1.0, -1.106, 1.703);
    float dx = blur * g_tex.z;
    float2 s = SourceUv(uv);
    float3 c0 = mul(toYiq, base);
    float3 cm1 = mul(toYiq, Tap(s + float2(-dx, 0)));
    float3 cp1 = mul(toYiq, Tap(s + float2(dx, 0)));
    float3 cm2 = mul(toYiq, Tap(s + float2(-2.0 * dx, 0)));
    float3 cp2 = mul(toYiq, Tap(s + float2(2.0 * dx, 0)));
    float2 chroma = (c0.yz * 0.30 + (cm1.yz + cp1.yz) * 0.22 + (cm2.yz + cp2.yz) * 0.13);
    float luma = c0.x;
    if (g_ntsc.y > 0.0)
    {
        // a mild sharpen with negative lobes = the "ringing" of a composite signal
        float ring = g_ntsc.y;
        luma = c0.x * (1.0 + 0.6 * ring) - (cm1.x + cp1.x) * 0.3 * ring;
    }
    return saturate(mul(toRgb, float3(luma, chroma)));
}

// CAS-style sharpening (AMD FidelityFX CAS, the 5-tap form): the picture's neighbours decide the amount.
float3 Sharpen(float2 uv, float3 c, float amount)
{
    if (amount <= 0.0)
        return c;
    float2 d = g_tex.zw;
    float2 s = SourceUv(uv);
    float3 n = Tap(s + float2(0, -d.y));
    float3 w = Tap(s + float2(-d.x, 0));
    float3 e = Tap(s + float2(d.x, 0));
    float3 so = Tap(s + float2(0, d.y));
    float3 mn = min(min(min(n, w), min(e, so)), c);
    float3 mx = max(max(max(n, w), max(e, so)), c);
    float3 rcpM = 1.0 / max(mx, 0.001);
    float3 amp = saturate(min(mn, 2.0 - mx) * rcpM);
    amp = sqrt(amp);
    float peak = -1.0 / lerp(8.0, 5.0, amount);
    float3 wgt = amp * peak;
    float3 rcpW = 1.0 / (1.0 + 4.0 * wgt);
    return saturate((n * wgt + w * wgt + e * wgt + so * wgt + c) * rcpW);
}

float3 Grade(float3 c)
{
    c *= g_grade.x;
    float y = dot(c, float3(0.299, 0.587, 0.114));
    c = lerp(y.xxx, c, g_grade.y);
    // temperature: warm pushes red and pulls blue, cool the reverse
    float t = g_grade.z;
    c *= float3(1.0 + 0.10 * t, 1.0 + 0.02 * t, 1.0 - 0.12 * t);
    if (g_grade.w > 0.5)
        c = dot(c, float3(0.299, 0.587, 0.114)).xxx;
    return saturate(c);
}

float4 PSGame(VSOut i) : SV_Target
{
    float2 uv = i.uv;
    float3 c = Fetch(uv);
    c = Ntsc(uv, c);
    c = Sharpen(uv, c, g_filt.x);
    c = Grade(c);

    // CRT: a gaussian beam over each source line, an aperture mask over the output pixels, soft corners.
    float beam = g_crt.x;
    if (beam > 0.0)
    {
        float lines = max(g_filt.w, 1.0);
        float ly = uv.y * lines;
        float phase = frac(ly) - 0.5;
        float sigma = lerp(0.45, 0.22, beam);
        float w = exp(-(phase * phase) / (2.0 * sigma * sigma));
        float norm = lerp(1.0, 1.0 / (sigma * 2.5066), 0.5); // keep the average brightness near 1
        c *= lerp(1.0, w * norm, beam);
        float mask = g_crt.y;
        if (mask > 0.0)
        {
            float px = i.pos.x;
            float3 tri = float3(0.0, 1.0, 2.0);
            float3 m = 1.0 - mask * (0.5 - 0.5 * cos((px + tri) * (2.0 * PI / 3.0)));
            c *= m * (1.0 + mask * 0.25);
        }
        float corner = g_crt.z;
        if (corner > 0.0)
        {
            float2 e = min(uv, 1.0 - uv);
            float r = 0.02 * corner;
            float2 q = max(r - e, 0.0);
            float d = length(q) / r;
            c *= smoothstep(1.0, 0.9, d);
        }
    }

    // LCD: a dark grid between the source pixels and R/G/B sub-columns inside each.
    float grid = g_crt.w;
    if (grid > 0.0)
    {
        float2 fp = frac(uv * g_tex.xy);
        float2 edge = min(fp, 1.0 - fp) * g_lcd.yz * g_tex.zw; // distance to the pixel border, in output px
        float g = smoothstep(0.0, 1.2, min(edge.x, edge.y));
        c *= lerp(1.0, lerp(0.35, 1.0, g), grid);
        float sub = g_lcd.x;
        if (sub > 0.0)
        {
            float col = floor(fp.x * 3.0);
            float3 tint = float3(col == 0.0, col == 1.0, col == 2.0);
            c *= lerp(1.0, tint * 2.4 + 0.2, sub * 0.6);
        }
    }

    return float4(saturate(c), 1.0) * i.col;
}
