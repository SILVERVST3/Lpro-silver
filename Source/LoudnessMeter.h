#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

// Medidor LUFS: ponderação K (BS.1770), momentâneo (400 ms), curto prazo (3 s)
// e integrado com gates absoluto (-70 LUFS) e relativo (-10 LU).
class LoudnessMeter
{
public:
    void prepare (double sampleRate)
    {
        const double pi = 3.14159265358979323846;
        fs = sampleRate;
        blockLen = std::max (1, (int) std::lround (fs * 0.1));

        {   // estágio 1: shelf de alta frequência
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& f : shelf)
            {
                f.b0 = (Vh + Vb * K / Q + K * K) / a0;
                f.b1 = 2.0 * (K * K - Vh) / a0;
                f.b2 = (Vh - Vb * K / Q + K * K) / a0;
                f.a1 = 2.0 * (K * K - 1.0) / a0;
                f.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        {   // estágio 2: passa-altas (RLB)
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& f : hp)
            {
                f.b0 = 1.0;
                f.b1 = -2.0;
                f.b2 = 1.0;
                f.a1 = 2.0 * (K * K - 1.0) / a0;
                f.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        reset();
    }

    void requestReset() { resetFlag.store (true); }

    // chamado na thread de áudio
    void process (const float* const* ch, int numCh, int numSamples)
    {
        if (resetFlag.exchange (false))
            reset();

        for (int i = 0; i < numSamples; ++i)
        {
            double e = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                const double y = hp[c].process (shelf[c].process ((double) ch[c][i]));
                e += y * y;
            }
            acc += e;
            if (++cnt >= blockLen)
                endBlock();
        }
    }

    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    static double toLufs (double e) { return -0.691 + 10.0 * std::log10 (std::max (e, 1.0e-12)); }

    double meanLast (int k) const
    {
        double s = 0.0;
        for (int j = 1; j <= k; ++j)
            s += ring[(size_t) ((ringPos - j + 30) % 30)];
        return s / (double) k;
    }

    void endBlock()
    {
        const double e = acc / (double) blockLen;
        acc = 0.0;
        cnt = 0;

        ring[(size_t) ringPos] = e;
        ringPos = (ringPos + 1) % 30;
        filled = std::min (filled + 1, 30);

        if (filled < 4)
            return;

        const double e4 = meanLast (4);
        const double lm = toLufs (e4);
        momentary.store ((float) lm);
        shortTerm.store ((float) toLufs (meanLast (filled)));

        if (lm > -70.0)
        {
            const int bin = std::clamp ((int) ((lm + 70.0) * 10.0), 0, numBins - 1);
            binE[(size_t) bin] += e4;
            binN[(size_t) bin] += 1;
        }

        double sE = 0.0;
        long long sN = 0;
        for (int i = 0; i < numBins; ++i)
        {
            sE += binE[(size_t) i];
            sN += binN[(size_t) i];
        }
        if (sN == 0)
            return;

        const double relThr = toLufs (sE / (double) sN) - 10.0;
        const int startBin = std::clamp ((int) std::floor ((relThr + 70.0) * 10.0), 0, numBins - 1);

        double sE2 = 0.0;
        long long sN2 = 0;
        for (int i = startBin; i < numBins; ++i)
        {
            sE2 += binE[(size_t) i];
            sN2 += binN[(size_t) i];
        }
        if (sN2 > 0)
            integrated.store ((float) toLufs (sE2 / (double) sN2));
    }

    void reset()
    {
        for (auto& f : shelf) f.z1 = f.z2 = 0.0;
        for (auto& f : hp) f.z1 = f.z2 = 0.0;
        acc = 0.0;
        cnt = 0;
        ring.fill (0.0);
        ringPos = 0;
        filled = 0;
        binE.fill (0.0);
        binN.fill (0);
        momentary.store (-100.0f);
        shortTerm.store (-100.0f);
        integrated.store (-100.0f);
    }

    static constexpr int numBins = 800; // -70 a +10 LUFS, passos de 0,1 LU

    double fs = 48000.0;
    int blockLen = 4800, cnt = 0, ringPos = 0, filled = 0;
    double acc = 0.0;
    Biquad shelf[2], hp[2];
    std::array<double, 30> ring {};
    std::array<double, numBins> binE {};
    std::array<long long, numBins> binN {};
    std::atomic<bool> resetFlag { false };
};
