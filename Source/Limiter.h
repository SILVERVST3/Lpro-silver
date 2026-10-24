#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Mínimo deslizante (deque monotônico): O(1) amortizado por amostra.
class SlidingMin
{
public:
    void setWindow (int w)
    {
        window = std::max (1, w);
        cap = window + 1;
        vals.assign ((size_t) cap, 1.0f);
        idxs.assign ((size_t) cap, 0);
        head = 0;
        count = 0;
        pos = 0;
    }

    float push (float v)
    {
        while (count > 0 && vals[(size_t) ((head + count - 1) % cap)] >= v)
            --count;

        const int slot = (head + count) % cap;
        vals[(size_t) slot] = v;
        idxs[(size_t) slot] = pos;
        ++count;

        if (idxs[(size_t) head] <= pos - window)
        {
            head = (head + 1) % cap;
            --count;
        }

        ++pos;
        return vals[(size_t) head];
    }

private:
    std::vector<float> vals;
    std::vector<int64_t> idxs;
    int window = 1, cap = 2, head = 0, count = 0;
    int64_t pos = 0;
};

/*  Limitador com lookahead.

    Cadeia por canal (taxa já sobreamostrada):
      1. ganho alvo t = ceiling / |x| quando |x| > ceiling
      2. linking de transientes (mistura com o menor ganho entre canais)
      3. mínimo deslizante numa janela N = D + 1  (D = atraso de lookahead)
      4. release exponencial (só sobe devagar; desce instantaneamente)
      5. linking de release
      6. média móvel de comprimento Na <= N (attack: suaviza a descida)
      7. áudio atrasado D amostras * ganho

    Como a média móvel cobre apenas amostras já "protegidas" pelo mínimo da janela,
    o ganho aplicado a cada amostra é sempre <= ao ganho necessário para ela,
    portanto não há overshoot (além do clamp final de segurança).
*/
class Limiter
{
public:
    static constexpr int maxChannels = 2;

    void prepare (int maxWindowSamples)
    {
        maxWin = std::max (2, maxWindowSamples);
        const size_t cap = (size_t) maxWin + 2;
        for (int c = 0; c < maxChannels; ++c)
        {
            rbuf[c].assign (cap, 1.0f);
            dly[c].assign (cap, 0.0f);
        }
        configure (0);
    }

    void setNumChannels (int n) { numCh = std::clamp (n, 1, maxChannels); }

    // delaySamples = lookahead na taxa de processamento (já sobreamostrada)
    void configure (int delaySamples)
    {
        D = std::clamp (delaySamples, 0, maxWin - 1);
        N = D + 1;
        Na = N;
        reset();
    }

    void setAttackFraction (float f)
    {
        const int newNa = std::clamp ((int) std::lround ((double) N * (double) f), 1, N);
        if (newNa == Na)
            return;

        Na = newNa;
        const int cap = (int) rbuf[0].size();
        for (int c = 0; c < maxChannels; ++c)
        {
            double s = 0.0;
            for (int j = 1; j <= Na; ++j)
                s += rbuf[c][(size_t) ((wp - j + cap) % cap)];
            sum[c] = s;
        }
    }

    void reset()
    {
        for (int c = 0; c < maxChannels; ++c)
        {
            sm[c].setWindow (N);
            std::fill (rbuf[c].begin(), rbuf[c].end(), 1.0f);
            std::fill (dly[c].begin(), dly[c].end(), 0.0f);
            rs[c] = 1.0f;
            sum[c] = (double) Na;
        }
        wp = 0;
    }

    void process (float* const* ch, int numSamples, float ceilingLin,
                  float linkT, float linkR, float relCoef)
    {
        const int cap = (int) rbuf[0].size();
        float minG = 1.0f, peak = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            float t[maxChannels] = { 1.0f, 1.0f };
            float x[maxChannels] = { 0.0f, 0.0f };
            float tmin = 1.0f;

            for (int c = 0; c < numCh; ++c)
            {
                x[c] = ch[c][n];
                const float a = std::abs (x[c]);
                t[c] = a > ceilingLin ? ceilingLin / a : 1.0f;
                tmin = std::min (tmin, t[c]);
            }

            float rmin = 1.0f;
            for (int c = 0; c < numCh; ++c)
            {
                t[c] += (tmin - t[c]) * linkT;
                const float m = sm[c].push (t[c]);
                rs[c] = m < rs[c] ? m : rs[c] * relCoef + m * (1.0f - relCoef);
                rmin = std::min (rmin, rs[c]);
            }

            for (int c = 0; c < numCh; ++c)
            {
                rs[c] += (rmin - rs[c]) * linkR;

                const int oldIdx = (wp - Na + cap) % cap;
                sum[c] += (double) rs[c] - (double) rbuf[c][(size_t) oldIdx];
                rbuf[c][(size_t) wp] = rs[c];
                const float g = std::min (1.0f, (float) (sum[c] / (double) Na));

                dly[c][(size_t) wp] = x[c];
                const float xd = dly[c][(size_t) ((wp - D + cap) % cap)];

                const float y = std::clamp (xd * g, -ceilingLin, ceilingLin);
                ch[c][n] = y;

                minG = std::min (minG, g);
                peak = std::max (peak, std::abs (y));
            }

            wp = (wp + 1) % cap;
        }

        blockMinGain = minG;
        blockPeak = peak;
    }

    float blockMinGain = 1.0f;
    float blockPeak = 0.0f;

private:
    int numCh = 2, maxWin = 2, D = 0, N = 1, Na = 1, wp = 0;
    SlidingMin sm[maxChannels];
    std::vector<float> rbuf[maxChannels], dly[maxChannels];
    float rs[maxChannels] = { 1.0f, 1.0f };
    double sum[maxChannels] = { 1.0, 1.0 };
};
