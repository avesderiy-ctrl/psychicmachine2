// THEE PSYCHICK MACHINE - DSP engine
// Pure C++17 (std only), no JUCE dependency, so it can be unit-tested on its own.
//
// Signal chain (per channel):
//   input -> TRANSMUTE (pitch shift) -> SIGIL (ring mod) -> FIRE (drive + wavefold)
//         -> ELIXIR (acid resonant filter) -> VORTEX (phaser swirl)
//         -> SCRYING (cross-fed shimmer, crystalline) -> SHATTER (decimate + bitcrush)
//         -> ASTRAL (mid/side width + Haas/detune spread) -> mix / output / safety limiter
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace psy
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

struct Params
{
    // TRANSMUTE
    float pitchSt = 0.f;      // -24..+24 semitones
    float grainMs = 50.f;     // 20..150 ms, grain window of the pitch shifter
    // SIGIL
    float ringHz = 220.f;     // 20..4000 Hz
    float ringMix = 0.f;      // 0..1
    // FIRE
    float drive = 0.f;        // 0..1
    float fold = 0.f;         // 0..1
    // ELIXIR (acid filter)
    float acidCut = 1200.f;   // 50..12000 Hz
    float acidRes = 0.3f;     // 0..1
    float acidRate = 0.5f;    // 0.02..12 Hz
    float acidDepth = 0.4f;   // 0..1
    float acidMix = 0.f;      // 0..1
    // VORTEX (phaser)
    float swirlRate = 0.4f;   // 0.02..8 Hz
    float swirlDepth = 0.6f;  // 0..1
    float swirlFb = 0.4f;     // 0..0.95
    float swirlMix = 0.f;     // 0..1
    // SCRYING (shimmer)
    float crysMix = 0.f;      // 0..1
    float crysFb = 0.5f;      // 0..0.92
    float crysPitchSt = 12.f; // -12..+24
    // SHATTER
    float shatter = 0.f;      // 0..1
    // ASTRAL
    float width = 1.f;        // 0..3
    float spread = 0.f;       // 0..1
    // MACRO / OUTPUT
    float trip = 0.f;         // 0..1
    float mix = 1.f;          // 0..1
    float outDb = 0.f;        // -24..+12
    bool  limit = true;
};

namespace detail
{
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float fastTanh(float x)
{
    // Padé-style rational approximation, accurate to ~1e-3 on [-3,3], saturates cleanly beyond.
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

struct Smoother
{
    float v = 0.f, target = 0.f, coef = 0.f;
    void prepare(double sr, float ms) { coef = std::exp(-1.f / (0.001f * ms * (float) sr)); }
    void snap(float x) { v = target = x; }
    void set(float t) { target = t; }
    float next() { v = target + (v - target) * coef; return v; }
};

struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init(int minSize)
    {
        int n = 1;
        while (n < minSize + 4) n <<= 1;
        buf.assign((size_t) n, 0.f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.f); w = 0; }
    void push(float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d = 0 -> most recently pushed sample
    float read(float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = buf[(size_t) ((w - 1 - i) & mask)];
        const float b = buf[(size_t) ((w - 2 - i) & mask)];
        return a + (b - a) * f;
    }
};

// Two-tap crossfaded delay-line pitch shifter (classic "rotating tape head" granular method).
struct PitchShifter
{
    DelayLine dl;
    float phase = 0.f;
    void init(int maxWindow) { dl.init(maxWindow + 8); }
    void reset() { dl.clear(); phase = 0.f; }
    float process(float x, float ratio, float win)
    {
        dl.push(x);
        phase += (1.f - ratio) / win;
        phase -= std::floor(phase);
        float p2 = phase + 0.5f;
        p2 -= std::floor(p2);
        float g1 = std::sin(kPi * phase); g1 *= g1;
        float g2 = std::sin(kPi * p2);    g2 *= g2;
        return dl.read(phase * win) * g1 + dl.read(p2 * win) * g2;
    }
};

struct Allpass
{
    std::vector<float> b;
    int i = 0;
    float g = 0.5f;
    void init(int len, float gain) { b.assign((size_t) std::max(1, len), 0.f); i = 0; g = gain; }
    void clear() { std::fill(b.begin(), b.end(), 0.f); i = 0; }
    float process(float x)
    {
        const float d = b[(size_t) i];
        const float w = x + g * d;
        b[(size_t) i] = w;
        if (++i >= (int) b.size()) i = 0;
        return -g * w + d;
    }
};

// Zavalishin TPT state-variable filter (lowpass output)
struct SVF
{
    float s1 = 0.f, s2 = 0.f;
    void clear() { s1 = s2 = 0.f; }
    float lowpass(float x, float g, float k)
    {
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.f * v1 - s1;
        s2 = 2.f * v2 - s2;
        return v2;
    }
};
} // namespace detail

class Engine
{
public:
    void prepare(double sampleRate)
    {
        sr = (float) sampleRate;
        const int maxWin = (int) (0.16 * sampleRate) + 16;
        for (auto& ch : c)
        {
            ch.ps.init(maxWin);
            ch.psShim.init((int) (0.08 * sampleRate) + 16);
            ch.shimDelay.init((int) (0.30 * sampleRate));
            ch.ap1.init((int) (0.0071f * sr), 0.55f);
            ch.ap2.init((int) (0.0113f * sr), 0.5f);
        }
        haas.init((int) (0.05 * sampleRate));
        for (auto* s : { &sPitch, &sGrain, &sRingHz, &sRingMix, &sDrive, &sFold, &sCut, &sRes, &sAcidRate,
                         &sAcidDepth, &sAcidMix, &sSwirlRate, &sSwirlDepth, &sSwirlFb, &sSwirlMix,
                         &sCrysMix, &sCrysFb, &sCrysPitch, &sShatter, &sWidth, &sSpread, &sTrip, &sMix,
                         &sOut })
            s->prepare(sampleRate, 25.f);
        reset();
        primed = false;
    }

    void reset()
    {
        for (auto& ch : c)
        {
            ch.ps.reset(); ch.psShim.reset(); ch.shimDelay.clear(); ch.ap1.clear(); ch.ap2.clear();
            ch.svf.clear(); ch.env = 0.f; ch.hp = 0.f; ch.fb = 0.f;
            for (auto& z : ch.apZ) z = 0.f;
            ch.apLast = 0.f; ch.held = 0.f;
        }
        haas.clear();
        holdCount = 0.f;
        ringPh = acidPh = swirlPh = modPh = 0.0;
        tripPhA = tripPhB = tripPhC = 0.f;
    }

    void process(float* left, float* right, int n, const Params& p)
    {
        using namespace detail;
        pushTargets(p);
        if (!primed) { snapAll(); primed = true; }

        const float invSr = 1.f / sr;
        const float nyq = 0.45f * sr;

        for (int i = 0; i < n; ++i)
        {
            const float pitchSt   = sPitch.next();
            const float grainS    = sGrain.next() * 0.001f * sr;
            const float ringHz    = sRingHz.next();
            const float ringMix   = sRingMix.next();
            const float drive     = sDrive.next();
            const float fold      = sFold.next();
            const float cut       = sCut.next();
            const float res       = sRes.next();
            const float trip      = sTrip.next();
            const float acidDepth = std::min(1.f, sAcidDepth.next() + trip * 0.5f);
            const float acidMix   = sAcidMix.next();
            const float swDepth   = std::min(1.f, sSwirlDepth.next() + trip * 0.3f);
            const float swFb      = sSwirlFb.next();
            const float swMix     = sSwirlMix.next();
            const float crysMix   = sCrysMix.next();
            const float crysFb    = sCrysFb.next();
            const float crysSt    = sCrysPitch.next();
            const float shatter   = sShatter.next();
            const float width     = sWidth.next();
            const float spread    = sSpread.next();
            const float mixv      = sMix.next();
            const float outGain   = std::pow(10.f, sOut.next() / 20.f);

            // slow chaotic drift driven by TRIP
            tripPhA += 0.071f * invSr; tripPhB += 0.193f * invSr; tripPhC += 0.037f * invSr;
            tripPhA -= std::floor(tripPhA); tripPhB -= std::floor(tripPhB); tripPhC -= std::floor(tripPhC);
            const float wobSt = trip * 0.35f * (std::sin(kTwoPi * tripPhA) + 0.5f * std::sin(kTwoPi * tripPhB));
            const float tripAcidOct = trip * 1.5f * std::sin(kTwoPi * tripPhC + 1.3f * std::sin(kTwoPi * tripPhB));

            // LFOs
            ringPh += (double) ringHz * invSr;   ringPh -= std::floor(ringPh);
            acidPh += (double) acidRateS(i) * invSr; acidPh -= std::floor(acidPh);
            swirlPh += (double) swirlRateS(i) * invSr; swirlPh -= std::floor(swirlPh);
            modPh += 0.27 * invSr; modPh -= std::floor(modPh);
            const float carrier = std::sin(kTwoPi * (float) ringPh);
            const float acidLfo = std::sin(kTwoPi * (float) acidPh);

            float dry[2] = { left[i], right[i] };
            float x[2] = { dry[0], dry[1] };

            const bool ctrl = (ctrlCounter++ & 7) == 0;

            for (int ch = 0; ch < 2; ++ch)
            {
                Chan& s = c[ch];
                float v = x[ch];

                // ---- TRANSMUTE: pitch shift (+ detune spread L/R)
                {
                    const float det = (ch == 0 ? 1.f : -1.f) * spread * 0.25f;   // +-25 cents
                    const float st = pitchSt + det + wobSt;
                    const float engage = clamp01(std::fabs(st) / 0.08f);
                    if (engage > 0.f)
                    {
                        const float ratio = std::exp2(st / 12.f);
                        const float sh = s.ps.process(v, ratio, grainS);
                        v = lerp(v, sh, engage);
                    }
                    else
                        s.ps.dl.push(v);
                }

                // ---- SIGIL: ring modulation
                v = lerp(v, v * carrier, ringMix);

                // ---- FIRE: drive + wavefold
                if (drive > 0.0005f || fold > 0.0005f)
                {
                    float y = v;
                    if (drive > 0.0005f)
                        y = fastTanh(y * (1.f + drive * 28.f)) / (1.f + drive * 1.2f);
                    if (fold > 0.0005f)
                    {
                        const float folded = std::sin(y * (1.f + fold * 9.f) * 1.35f);
                        y = lerp(y, folded, clamp01(fold * 5.f));
                    }
                    v = y;
                }

                // ---- ELIXIR: acid resonant filter, LFO + envelope follower
                if (acidMix > 0.0005f)
                {
                    const float a = std::fabs(v);
                    s.env += (a > s.env ? 0.02f : 0.0003f) * 1.f * (a - s.env) * (48000.f * invSr) ;
                    if (ctrl)
                    {
                        const float oct = acidLfo * acidDepth * 3.f + s.env * acidDepth * 2.5f + tripAcidOct;
                        const float fc = std::min(nyq, std::max(30.f, cut * std::exp2(oct)));
                        s.g = std::tan(kPi * fc * invSr);
                        s.q = 0.7f + res * res * 19.f;
                        s.k = 1.f / s.q;
                    }
                    const float in = fastTanh(v * (1.f + res * 1.5f));
                    float f = s.svf.lowpass(in, s.g, s.k);
                    f = fastTanh(f * 1.2f) / (1.f + 0.1f * s.q);
                    v = lerp(v, f * 1.6f, acidMix);
                }

                // ---- VORTEX: 6-stage phaser with feedback
                if (swMix > 0.0005f)
                {
                    if (ctrl)
                    {
                        const float lfo = 0.5f + 0.5f * std::sin(kTwoPi * ((float) swirlPh + (ch ? 0.25f : 0.f)));
                        const float base = 180.f * std::exp2(lfo * swDepth * 5.f);
                        static const float mult[6] = { 1.f, 1.35f, 1.8f, 2.4f, 3.2f, 4.3f };
                        for (int k = 0; k < 6; ++k)
                        {
                            const float f = std::min(nyq, base * mult[k]);
                            const float t = std::tan(kPi * f * invSr);
                            s.apA[k] = (t - 1.f) / (t + 1.f);
                        }
                    }
                    float sig = v + swFb * detail::fastTanh(s.apLast);
                    for (int k = 0; k < 6; ++k)
                    {
                        const float y = s.apA[k] * sig + s.apZ[k];
                        s.apZ[k] = sig - s.apA[k] * y;
                        sig = y;
                    }
                    s.apLast = sig;
                    v = v * (1.f - 0.5f * swMix) + sig * 0.5f * swMix;
                }

                x[ch] = v;
            }

            // ---- SCRYING: ping-pong shimmer (pitched feedback delay, diffused, high-passed)
            {
                const float ratio = std::exp2(crysSt / 12.f);
                const float hpA = 1.f - std::exp(-kTwoPi * 550.f * invSr);
                float out[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    Chan& s = c[ch];
                    const float cross = c[1 - ch].fb;
                    const float tmp = x[ch] * 0.7f + cross * crysFb;
                    s.shimDelay.push(tmp);
                    const float baseLen = (ch == 0 ? 0.131f : 0.173f) * sr;
                    const float mod = std::sin(kTwoPi * ((float) modPh + (ch ? 0.31f : 0.f))) * 0.0035f * sr;
                    const float delayed = s.shimDelay.read(baseLen + mod);
                    const float shifted = s.psShim.process(delayed, ratio, 0.06f * sr);
                    s.hp += hpA * (shifted - s.hp);
                    const float bright = shifted - s.hp;
                    const float diff = s.ap2.process(s.ap1.process(bright));
                    const float sat = fastTanh(diff * 1.3f) * 0.77f;
                    s.fb = sat;
                    out[ch] = sat + 0.3f * bright;
                }
                x[0] += out[0] * crysMix * 1.2f;
                x[1] += out[1] * crysMix * 1.2f;
            }

            // ---- SHATTER: sample-rate decimation + bit reduction
            if (shatter > 0.0005f)
            {
                const float holdLen = 1.f + shatter * shatter * 40.f;
                const float levels = std::exp2(std::max(3.f, 15.f - shatter * 11.f));
                holdCount += 1.f;
                const bool take = holdCount >= holdLen;
                if (take) holdCount -= holdLen;
                const float amt = clamp01(shatter * 1.6f);
                for (int ch = 0; ch < 2; ++ch)
                {
                    Chan& s = c[ch];
                    if (take) s.held = std::round(x[ch] * levels) / levels;
                    x[ch] = lerp(x[ch], s.held, amt);
                }
            }

            // ---- ASTRAL: width (M/S) + Haas spread on right
            {
                const float m = 0.5f * (x[0] + x[1]);
                const float sd = 0.5f * (x[0] - x[1]) * width;
                x[0] = m + sd;
                x[1] = m - sd;
                haas.push(x[1]);
                const float d = spread * 0.012f * sr;
                x[1] = lerp(x[1], haas.read(d), clamp01(spread * 4.f));
            }

            // ---- mix / output / safety
            float o[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = (dry[ch] * (1.f - mixv) + x[ch] * mixv) * outGain;
                if (!std::isfinite(y)) y = 0.f;
                if (limitOn)
                {
                    const float knee = 0.85f;
                    const float a = std::fabs(y);
                    if (a > knee) y = std::copysign(knee + (1.f - knee) * fastTanh((a - knee) / (1.f - knee)), y);
                }
                o[ch] = y;
            }
            left[i] = o[0];
            right[i] = o[1];
        }
    }

private:
    struct Chan
    {
        detail::PitchShifter ps, psShim;
        detail::DelayLine shimDelay;
        detail::Allpass ap1, ap2;
        detail::SVF svf;
        float env = 0.f, g = 0.1f, k = 1.f, q = 1.f;
        float apA[6] = { 0, 0, 0, 0, 0, 0 };
        float apZ[6] = { 0, 0, 0, 0, 0, 0 };
        float apLast = 0.f;
        float hp = 0.f, fb = 0.f, held = 0.f;
    };

    float acidRateS(int) { return sAcidRate.next(); }
    float swirlRateS(int) { return sSwirlRate.next(); }

    void pushTargets(const Params& p)
    {
        sPitch.set(p.pitchSt);
        sGrain.set(p.grainMs);
        sRingHz.set(p.ringHz);
        sRingMix.set(p.ringMix);
        sDrive.set(p.drive);
        sFold.set(p.fold);
        sCut.set(p.acidCut);
        sRes.set(p.acidRes);
        sAcidRate.set(p.acidRate);
        sAcidDepth.set(p.acidDepth);
        sAcidMix.set(p.acidMix);
        sSwirlRate.set(p.swirlRate);
        sSwirlDepth.set(p.swirlDepth);
        sSwirlFb.set(p.swirlFb);
        sSwirlMix.set(p.swirlMix);
        sCrysMix.set(p.crysMix);
        sCrysFb.set(std::min(0.92f, p.crysFb));
        sCrysPitch.set(p.crysPitchSt);
        sShatter.set(p.shatter);
        sWidth.set(p.width);
        sSpread.set(p.spread);
        sTrip.set(p.trip);
        sMix.set(p.mix);
        sOut.set(p.outDb);
        limitOn = p.limit;
    }

    void snapAll()
    {
        for (auto* s : { &sPitch, &sGrain, &sRingHz, &sRingMix, &sDrive, &sFold, &sCut, &sRes, &sAcidRate,
                         &sAcidDepth, &sAcidMix, &sSwirlRate, &sSwirlDepth, &sSwirlFb, &sSwirlMix, &sCrysMix,
                         &sCrysFb, &sCrysPitch, &sShatter, &sWidth, &sSpread, &sTrip, &sMix, &sOut })
            s->snap(s->target);
    }

    float sr = 44100.f;
    bool primed = false, limitOn = true;
    int ctrlCounter = 0;
    Chan c[2];
    detail::DelayLine haas;
    double ringPh = 0, acidPh = 0, swirlPh = 0, modPh = 0;
    float tripPhA = 0, tripPhB = 0, tripPhC = 0, holdCount = 0;

    detail::Smoother sPitch, sGrain, sRingHz, sRingMix, sDrive, sFold, sCut, sRes, sAcidRate, sAcidDepth,
        sAcidMix, sSwirlRate, sSwirlDepth, sSwirlFb, sSwirlMix, sCrysMix, sCrysFb, sCrysPitch, sShatter,
        sWidth, sSpread, sTrip, sMix, sOut;
};
} // namespace psy
