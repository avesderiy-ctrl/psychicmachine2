// Standalone sanity test for the DSP engine (no JUCE needed).
//   g++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test && ./engine_test
#include "PsychickEngine.h"
#include <cstdio>
#include <cstdlib>
#include <random>

static int failures = 0;
#define CHECK(cond, msg)                                                   \
    do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; }    \
         else std::printf("  ok:   %s\n", msg); } while (0)

static double zeroCrossFreq(const std::vector<float>& x, double sr, size_t from, size_t to)
{
    int crossings = 0;
    for (size_t i = from + 1; i < to; ++i)
        if (x[i - 1] <= 0.f && x[i] > 0.f) ++crossings;
    return crossings * sr / (double) (to - from);
}

int main()
{
    const double sr = 48000.0;
    const int N = (int) (sr * 2.0);
    std::mt19937 rng(23);
    std::uniform_real_distribution<float> nd(-1.f, 1.f);

    // 1) pitch accuracy: 440 Hz sine, +12 st -> ~880 Hz, -12 st -> ~220 Hz
    for (float st : { 12.f, -12.f, 7.f })
    {
        psy::Engine e; e.prepare(sr);
        psy::Params p; p.pitchSt = st; p.limit = false;
        std::vector<float> L(N), R(N);
        for (int i = 0; i < N; ++i) L[i] = R[i] = 0.5f * std::sin(2.0 * 3.14159265358979 * 440.0 * i / sr);
        for (int b = 0; b < N; b += 256) e.process(&L[b], &R[b], std::min(256, N - b), p);
        const double f = zeroCrossFreq(L, sr, (size_t) sr, (size_t) (2 * sr) - 100);
        const double want = 440.0 * std::pow(2.0, st / 12.0);
        std::printf("pitch %+.0f st: measured %.1f Hz, expected %.1f Hz\n", st, f, want);
        CHECK(std::fabs(f - want) / want < 0.03, "pitch within 3%");
    }

    // 2) identity: everything off, mix=1 -> output == input (limiter off)
    {
        psy::Engine e; e.prepare(sr);
        psy::Params p; p.limit = false; p.width = 1.f; p.crysMix = 0.f;
        std::vector<float> L(N), R(N), L0, R0;
        for (int i = 0; i < N; ++i) { L[i] = 0.4f * nd(rng); R[i] = 0.4f * nd(rng); }
        L0 = L; R0 = R;
        for (int b = 0; b < N; b += 333) e.process(&L[b], &R[b], std::min(333, N - b), p);
        double err = 0; for (int i = 0; i < N; ++i) err = std::max(err, (double) std::fabs(L[i] - L0[i]));
        std::printf("identity max error: %g\n", err);
        CHECK(err < 1e-4, "bypass-state is transparent");
    }

    // 3) stress: all knobs high, noisy + loud input, various block sizes -> finite, bounded
    for (int blk : { 1, 64, 480, 1024 })
    {
        psy::Engine e; e.prepare(sr);
        psy::Params p;
        p.pitchSt = 5.f; p.grainMs = 30.f; p.ringHz = 900.f; p.ringMix = 0.8f; p.drive = 1.f; p.fold = 1.f;
        p.acidCut = 400.f; p.acidRes = 1.f; p.acidRate = 6.f; p.acidDepth = 1.f; p.acidMix = 1.f;
        p.swirlRate = 3.f; p.swirlDepth = 1.f; p.swirlFb = 0.95f; p.swirlMix = 1.f;
        p.crysMix = 1.f; p.crysFb = 0.92f; p.crysPitchSt = 12.f; p.shatter = 1.f;
        p.width = 3.f; p.spread = 1.f; p.trip = 1.f; p.mix = 1.f; p.outDb = 12.f; p.limit = true;
        std::vector<float> L(N), R(N);
        for (int i = 0; i < N; ++i) { L[i] = 0.9f * nd(rng); R[i] = 0.9f * nd(rng); }
        for (int b = 0; b < N; b += blk) e.process(&L[b], &R[b], std::min(blk, N - b), p);
        float peak = 0; bool fin = true;
        for (int i = 0; i < N; ++i) { peak = std::max({ peak, std::fabs(L[i]), std::fabs(R[i]) }); fin &= std::isfinite(L[i]) && std::isfinite(R[i]); }
        std::printf("stress block=%d: peak %.3f\n", blk, peak);
        CHECK(fin, "all samples finite");
        CHECK(peak <= 1.0001f, "safety limiter holds <= 1.0");
    }

    // 4) each module individually changes the signal and stays finite
    struct Case { const char* name; void (*f)(psy::Params&); };
    Case cases[] = {
        { "ring",    [](psy::Params& p) { p.ringMix = 1.f; p.ringHz = 300.f; } },
        { "drive",   [](psy::Params& p) { p.drive = 0.8f; } },
        { "fold",    [](psy::Params& p) { p.fold = 0.8f; } },
        { "acid",    [](psy::Params& p) { p.acidMix = 1.f; p.acidRes = 0.8f; } },
        { "swirl",   [](psy::Params& p) { p.swirlMix = 1.f; } },
        { "crystal", [](psy::Params& p) { p.crysMix = 0.8f; } },
        { "shatter", [](psy::Params& p) { p.shatter = 0.7f; } },
        { "width",   [](psy::Params& p) { p.width = 2.f; } },
        { "spread",  [](psy::Params& p) { p.spread = 0.8f; } },
    };
    for (auto& cs : cases)
    {
        psy::Engine e; e.prepare(sr);
        psy::Params p; p.limit = false; cs.f(p);
        std::vector<float> L(N), R(N), L0;
        for (int i = 0; i < N; ++i) { L[i] = 0.5f * std::sin(2.0 * 3.14159265358979 * 330.0 * i / sr); R[i] = 0.5f * std::sin(2.0 * 3.14159265358979 * 495.0 * i / sr); }
        L0 = L;
        for (int b = 0; b < N; b += 512) e.process(&L[b], &R[b], std::min(512, N - b), p);
        double diff = 0; bool fin = true;
        for (int i = 0; i < N; ++i) { diff += std::fabs(L[i] - L0[i]); fin &= std::isfinite(L[i]) && std::isfinite(R[i]); }
        std::printf("module %-8s mean|diff| %.4f\n", cs.name, diff / N);
        CHECK(fin && diff / N > 0.003, cs.name);
    }

    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
    return failures ? 1 : 0;
}
