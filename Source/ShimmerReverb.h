#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>

// ============================================================
// ShimmerReverb — Valhalla-inspired lush algorithmic reverb
// 8-line FDN, nested allpass diffusion, modulated delays,
// dual-band damping, shimmer pitch shifter
// ============================================================
class ShimmerReverb
{
public:
    // FDN tap energies for UI visualization
    std::atomic<float> tapEnergy[8] = {};

    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = static_cast<float>(sampleRate);
        srScale = sr / 44100.0f;

        // Pre-delay buffer (up to 500ms for big spaces)
        preDelaySize = static_cast<int>(sr * 0.5f) + 1;
        preDelayL.assign(static_cast<size_t>(preDelaySize), 0.0f);
        preDelayR.assign(static_cast<size_t>(preDelaySize), 0.0f);
        preDelayWritePos = 0;

        // Input diffusion: 4 allpass filters (series)
        static const int inputApLens[numInputAp] = { 142, 107, 379, 277 };
        for (int i = 0; i < numInputAp; ++i)
        {
            int len = std::max(1, static_cast<int>(inputApLens[i] * srScale));
            inputApBufL[i].assign(static_cast<size_t>(len), 0.0f);
            inputApBufR[i].assign(static_cast<size_t>(len), 0.0f);
            inputApLen[i] = len;
            inputApPos[i] = 0;
        }

        // 8-line FDN with long prime-length delays (50-140ms range)
        // These primes are chosen to avoid common factors → no metallic modes
        static const int fdnBaseLens[fdnCount] = {
            2473, 2767, 3217, 3557,   // ~56ms, 63ms, 73ms, 81ms at 44100
            3907, 4127, 4639, 5101    // ~89ms, 94ms, 105ms, 116ms at 44100
        };
        for (int i = 0; i < fdnCount; ++i)
        {
            int len = std::max(1, static_cast<int>(fdnBaseLens[i] * srScale));
            fdnBufL[i].assign(static_cast<size_t>(len), 0.0f);
            fdnBufR[i].assign(static_cast<size_t>(len), 0.0f);
            fdnLen[i] = len;
            fdnPos[i] = 0;
            dampHiL[i] = 0.0f;
            dampHiR[i] = 0.0f;
            dampLoL[i] = 0.0f;
            dampLoR[i] = 0.0f;
        }

        // Allpass diffusers inside feedback loop (2 per tap = 16 total)
        static const int tankApLens[fdnCount * 2] = {
            672, 908, 1452, 782, 1312, 556, 1072, 1622,
            448, 1168, 872, 632, 1532, 512, 952, 1282
        };
        for (int i = 0; i < fdnCount * 2; ++i)
        {
            int len = std::max(1, static_cast<int>(tankApLens[i] * srScale));
            tankApBufL[i].assign(static_cast<size_t>(len), 0.0f);
            tankApBufR[i].assign(static_cast<size_t>(len), 0.0f);
            tankApLen[i] = len;
            tankApPos[i] = 0;
        }

        // Shimmer grains (60ms grains for smoother pitch shift)
        grainSize = std::clamp(static_cast<int>(sr * 0.06f), 128, 4096);
        grainBufL.assign(static_cast<size_t>(grainSize * 2), 0.0f);
        grainBufR.assign(static_cast<size_t>(grainSize * 2), 0.0f);
        grainWritePos = 0;
        grainPhase = 0.0f;

        // LFOs for delay modulation (8 independent slow LFOs)
        for (int i = 0; i < fdnCount; ++i)
            lfoPhase[i] = static_cast<float>(i) * 0.785f; // spread phases

        // DC blocker
        dcL = dcR = dcPrevInL = dcPrevInR = 0.0f;

        // Smoothing
        smoothDecay.reset(sr, 0.05);
        smoothDampHi.reset(sr, 0.05);
        smoothMix.reset(sr, 0.02);
    }

    void setParameters(float mix, float decay, float damping, float width,
                       float shimmerAmount, float preDelayMs,
                       float modRate, float modDepth)
    {
        userModRate = modRate;
        userModDepth = modDepth;
        if (!frozen)
        {
            float targetDecay = 0.4f + std::min(decay, 1.0f) * 0.595f;
            smoothDecay.setTargetValue(targetDecay);
        }
        float targetDampHi = 1.0f - damping * 0.85f;
        smoothDampHi.setTargetValue(targetDampHi);
        stereoWidth = width;
        shimmer = shimmerAmount;
        preDelaySamples = std::min(preDelayMs * sr / 1000.0f, static_cast<float>(preDelaySize - 1));
        smoothMix.setTargetValue(mix);
    }

    void setFreeze(bool shouldFreeze)
    {
        if (shouldFreeze && !frozen)
        {
            storedDecay = smoothDecay.getTargetValue();
            smoothDecay.setTargetValue(0.998f);
            frozen = true;
        }
        else if (!shouldFreeze && frozen)
        {
            smoothDecay.setTargetValue(storedDecay);
            frozen = false;
        }
    }

    void processStereo(float* left, float* right, int numSamples)
    {
        float peakTapEnergy[fdnCount] = {};

        for (int s = 0; s < numSamples; ++s)
        {
            float currentDecay = smoothDecay.getNextValue();
            float currentDampHi = smoothDampHi.getNextValue();
            float currentMix = smoothMix.getNextValue();
            float currentDry = 1.0f - currentMix;

            float inL = left[s];
            float inR = right[s];

            // === Pre-delay ===
            float pdL, pdR;
            {
                preDelayL[static_cast<size_t>(preDelayWritePos)] = inL;
                preDelayR[static_cast<size_t>(preDelayWritePos)] = inR;

                float rp = static_cast<float>(preDelayWritePos) - preDelaySamples;
                if (rp < 0.0f) rp += static_cast<float>(preDelaySize);
                int ri = static_cast<int>(rp);
                float frac = rp - static_cast<float>(ri);
                int ri2 = (ri + 1) % preDelaySize;
                pdL = preDelayL[static_cast<size_t>(ri)] * (1.0f - frac) + preDelayL[static_cast<size_t>(ri2)] * frac;
                pdR = preDelayR[static_cast<size_t>(ri)] * (1.0f - frac) + preDelayR[static_cast<size_t>(ri2)] * frac;
                preDelayWritePos = (preDelayWritePos + 1) % preDelaySize;
            }

            // === Input diffusion (4 series allpass) ===
            float diffL = pdL, diffR = pdR;
            for (int ap = 0; ap < numInputAp; ++ap)
            {
                constexpr float g = 0.5f;
                float bL = inputApBufL[ap][static_cast<size_t>(inputApPos[ap])];
                float bR = inputApBufR[ap][static_cast<size_t>(inputApPos[ap])];
                float oL = bL - g * diffL;
                float oR = bR - g * diffR;
                inputApBufL[ap][static_cast<size_t>(inputApPos[ap])] = diffL + g * oL;
                inputApBufR[ap][static_cast<size_t>(inputApPos[ap])] = diffR + g * oR;
                inputApPos[ap] = (inputApPos[ap] + 1) % inputApLen[ap];
                diffL = oL;
                diffR = oR;
            }

            // === Read 8 FDN taps with modulated delay ===
            float tapL[fdnCount], tapR[fdnCount];
            for (int f = 0; f < fdnCount; ++f)
            {
                // Slow sinusoidal modulation on read position
                // User-controllable rate and depth
                float lfoRate = userModRate * (0.6f + static_cast<float>(f) * 0.34f);
                lfoPhase[f] += lfoRate * juce::MathConstants<float>::twoPi / sr;
                if (lfoPhase[f] >= juce::MathConstants<float>::twoPi)
                    lfoPhase[f] -= juce::MathConstants<float>::twoPi;
                float mod = std::sin(lfoPhase[f]) * userModDepth * 16.0f * srScale;

                // Calculate read position
                float rp = static_cast<float>(fdnPos[f]) - static_cast<float>(fdnLen[f]) + mod;
                while (rp < 0.0f) rp += static_cast<float>(fdnLen[f]);
                while (rp >= static_cast<float>(fdnLen[f])) rp -= static_cast<float>(fdnLen[f]);
                int ri = static_cast<int>(rp);
                float frac = rp - static_cast<float>(ri);
                int ri2 = (ri + 1) % fdnLen[f];

                float rawL = fdnBufL[f][static_cast<size_t>(ri)] * (1.0f - frac) + fdnBufL[f][static_cast<size_t>(ri2)] * frac;
                float rawR = fdnBufR[f][static_cast<size_t>(ri)] * (1.0f - frac) + fdnBufR[f][static_cast<size_t>(ri2)] * frac;

                // High frequency damping (1-pole LP in feedback path)
                dampHiL[f] += currentDampHi * (rawL - dampHiL[f]);
                dampHiR[f] += currentDampHi * (rawR - dampHiR[f]);

                // Low frequency damping (gentle 1-pole HP, ~80Hz)
                constexpr float loCoeff = 0.003f; // very gentle, preserves bass warmth
                dampLoL[f] += loCoeff * (dampHiL[f] - dampLoL[f]);
                dampLoR[f] += loCoeff * (dampHiR[f] - dampLoR[f]);
                tapL[f] = dampHiL[f] - dampLoL[f] * 0.3f; // subtle HP, keeps warmth
                tapR[f] = dampHiR[f] - dampLoR[f] * 0.3f;

                // Track peak energy per tap for visualization
                float e = std::abs(tapL[f]) + std::abs(tapR[f]);
                if (e > peakTapEnergy[f]) peakTapEnergy[f] = e;
            }

            // === 8x8 Householder mixing matrix ===
            // Householder: H = I - 2/N * ones(N,N) — energy preserving
            float sumL = 0.0f, sumR = 0.0f;
            for (int f = 0; f < fdnCount; ++f) { sumL += tapL[f]; sumR += tapR[f]; }
            float mixTermL = sumL * (2.0f / static_cast<float>(fdnCount));
            float mixTermR = sumR * (2.0f / static_cast<float>(fdnCount));
            float mixedL[fdnCount], mixedR[fdnCount];
            for (int f = 0; f < fdnCount; ++f)
            {
                mixedL[f] = tapL[f] - mixTermL;
                mixedR[f] = tapR[f] - mixTermR;
            }

            // === Shimmer: granular pitch shift +12 semitones ===
            float shimL = 0.0f, shimR = 0.0f;
            if (shimmer > 0.001f && !frozen)
            {
                // Feed shimmer from all taps (averaged)
                float feedL = sumL / static_cast<float>(fdnCount);
                float feedR = sumR / static_cast<float>(fdnCount);
                grainBufL[static_cast<size_t>(grainWritePos)] = feedL;
                grainBufR[static_cast<size_t>(grainWritePos)] = feedR;
                grainBufL[static_cast<size_t>(grainWritePos + grainSize)] = feedL;
                grainBufR[static_cast<size_t>(grainWritePos + grainSize)] = feedR;
                grainWritePos = (grainWritePos + 1) % grainSize;

                float gs = static_cast<float>(grainSize);
                float gp1 = grainPhase;
                float gp2 = std::fmod(grainPhase + gs * 0.5f, gs);

                // Hann windows
                float w1 = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * gp1 / gs);
                float w2 = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * gp2 / gs);

                auto readGrain = [&](float phase, float window, float& outL, float& outR)
                {
                    int gi = static_cast<int>(phase) % grainSize;
                    int gi2 = (gi + 1) % grainSize;
                    float gf = phase - std::floor(phase);
                    outL = (grainBufL[static_cast<size_t>(gi)] * (1.0f - gf) + grainBufL[static_cast<size_t>(gi2)] * gf) * window;
                    outR = (grainBufR[static_cast<size_t>(gi)] * (1.0f - gf) + grainBufR[static_cast<size_t>(gi2)] * gf) * window;
                };

                float s1L, s1R, s2L, s2R;
                readGrain(gp1, w1, s1L, s1R);
                readGrain(gp2, w2, s2L, s2R);
                shimL = s1L + s2L;
                shimR = s1R + s2R;

                grainPhase += 2.0f; // +12 semitones
                if (grainPhase >= gs) grainPhase -= gs;
            }

            // === Write back into FDN ===
            for (int f = 0; f < fdnCount; ++f)
            {
                float fbL = mixedL[f] * currentDecay;
                float fbR = mixedR[f] * currentDecay;

                // Inject shimmer into feedback (distributed)
                if (shimmer > 0.001f)
                {
                    fbL += shimL * shimmer * 0.30f;
                    fbR += shimR * shimmer * 0.30f;
                }

                // Inject input into all lines (spread evenly for density)
                if (!frozen)
                {
                    float inputGain = 0.32f;
                    fbL += diffL * inputGain;
                    fbR += diffR * inputGain;
                }

                // === Tank allpass diffusion (2 per line) ===
                for (int a = 0; a < 2; ++a)
                {
                    int apIdx = f * 2 + a;
                    constexpr float g = 0.4f;
                    float bL = tankApBufL[apIdx][static_cast<size_t>(tankApPos[apIdx])];
                    float bR = tankApBufR[apIdx][static_cast<size_t>(tankApPos[apIdx])];
                    float oL = bL - g * fbL;
                    float oR = bR - g * fbR;
                    tankApBufL[apIdx][static_cast<size_t>(tankApPos[apIdx])] = fbL + g * oL;
                    tankApBufR[apIdx][static_cast<size_t>(tankApPos[apIdx])] = fbR + g * oR;
                    tankApPos[apIdx] = (tankApPos[apIdx] + 1) % tankApLen[apIdx];
                    fbL = oL;
                    fbR = oR;
                }

                // Soft-limit: generous headroom (linear below ±4.5, soft above)
                auto softLimit = [](float x) -> float {
                    if (x > 4.5f) return 4.5f + std::tanh(x - 4.5f);
                    if (x < -4.5f) return -4.5f - std::tanh(-x - 4.5f);
                    return x;
                };
                fbL = softLimit(fbL);
                fbR = softLimit(fbR);

                fdnBufL[f][static_cast<size_t>(fdnPos[f])] = fbL;
                fdnBufR[f][static_cast<size_t>(fdnPos[f])] = fbR;
                fdnPos[f] = (fdnPos[f] + 1) % fdnLen[f];
            }

            // === Output: weighted sum of all taps, stereo decorrelation ===
            // Different tap weights L/R for natural stereo image
            float outWetL = (tapL[0]        + tapL[1] * 0.7f + tapL[2]        + tapL[3] * 0.6f
                           + tapL[4] * 0.9f + tapL[5] * 0.5f + tapL[6] * 0.8f + tapL[7] * 0.4f) * 0.42f;
            float outWetR = (tapR[0] * 0.6f + tapR[1]        + tapR[2] * 0.5f + tapR[3]
                           + tapR[4] * 0.4f + tapR[5] * 0.9f + tapR[6] * 0.5f + tapR[7] * 0.8f) * 0.42f;

            // DC blocking (standard 1-pole HP, ~5Hz)
            float dcAlpha = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f / sr);
            float hpL = outWetL - dcPrevInL + dcAlpha * dcL;
            float hpR = outWetR - dcPrevInR + dcAlpha * dcR;
            dcPrevInL = outWetL;
            dcPrevInR = outWetR;
            dcL = hpL;
            dcR = hpR;

            // Stereo width (mid-side)
            float mid = (hpL + hpR) * 0.5f;
            float side = (hpL - hpR) * 0.5f * stereoWidth;
            float finalWetL = mid + side;
            float finalWetR = mid - side;

            left[s]  = inL * currentDry + finalWetL * currentMix;
            right[s] = inR * currentDry + finalWetR * currentMix;
        }

        // Store tap energies for UI
        for (int f = 0; f < fdnCount; ++f)
            tapEnergy[f].store(peakTapEnergy[f], std::memory_order_relaxed);
    }

    void processMono(float* data, int numSamples)
    {
        std::vector<float> temp(data, data + numSamples);
        processStereo(data, temp.data(), numSamples);
        for (int i = 0; i < numSamples; ++i)
            data[i] = (data[i] + temp[i]) * 0.5f;
    }

    void reset()
    {
        for (int i = 0; i < numInputAp; ++i)
        {
            std::fill(inputApBufL[i].begin(), inputApBufL[i].end(), 0.0f);
            std::fill(inputApBufR[i].begin(), inputApBufR[i].end(), 0.0f);
            inputApPos[i] = 0;
        }
        for (int i = 0; i < fdnCount; ++i)
        {
            std::fill(fdnBufL[i].begin(), fdnBufL[i].end(), 0.0f);
            std::fill(fdnBufR[i].begin(), fdnBufR[i].end(), 0.0f);
            fdnPos[i] = 0;
            dampHiL[i] = dampHiR[i] = 0.0f;
            dampLoL[i] = dampLoR[i] = 0.0f;
            lfoPhase[i] = static_cast<float>(i) * 0.785f;
        }
        for (int i = 0; i < fdnCount * 2; ++i)
        {
            std::fill(tankApBufL[i].begin(), tankApBufL[i].end(), 0.0f);
            std::fill(tankApBufR[i].begin(), tankApBufR[i].end(), 0.0f);
            tankApPos[i] = 0;
        }
        std::fill(preDelayL.begin(), preDelayL.end(), 0.0f);
        std::fill(preDelayR.begin(), preDelayR.end(), 0.0f);
        std::fill(grainBufL.begin(), grainBufL.end(), 0.0f);
        std::fill(grainBufR.begin(), grainBufR.end(), 0.0f);
        preDelayWritePos = 0;
        grainWritePos = 0;
        grainPhase = 0.0f;
        dcL = dcR = dcPrevInL = dcPrevInR = 0.0f;
    }

private:
    float sr = 44100.0f;
    float srScale = 1.0f;

    // Parameters (smoothed)
    juce::SmoothedValue<float> smoothDecay { 0.5f };
    juce::SmoothedValue<float> smoothDampHi { 0.65f };
    juce::SmoothedValue<float> smoothMix { 0.4f };
    float stereoWidth = 1.0f;
    float shimmer = 0.0f;
    float preDelaySamples = 0.0f;
    float userModRate = 0.5f;
    float userModDepth = 0.2f;
    bool frozen = false;
    float storedDecay = 0.5f;

    // Pre-delay
    std::vector<float> preDelayL, preDelayR;
    int preDelaySize = 0;
    int preDelayWritePos = 0;

    // Input allpass diffusion
    static constexpr int numInputAp = 4;
    std::vector<float> inputApBufL[numInputAp], inputApBufR[numInputAp];
    int inputApLen[numInputAp] = {};
    int inputApPos[numInputAp] = {};

    // 8-line FDN
    static constexpr int fdnCount = 8;
    std::vector<float> fdnBufL[fdnCount], fdnBufR[fdnCount];
    int fdnLen[fdnCount] = {};
    int fdnPos[fdnCount] = {};
    float dampHiL[fdnCount] = {};
    float dampHiR[fdnCount] = {};
    float dampLoL[fdnCount] = {};
    float dampLoR[fdnCount] = {};

    // Tank allpass diffusion (2 per FDN line = 16)
    std::vector<float> tankApBufL[16], tankApBufR[16];
    int tankApLen[16] = {};
    int tankApPos[16] = {};

    // LFOs for delay modulation
    float lfoPhase[fdnCount] = {};

    // Shimmer grains
    std::vector<float> grainBufL, grainBufR;
    int grainSize = 0;
    int grainWritePos = 0;
    float grainPhase = 0.0f;

    // DC blocker
    float dcL = 0.0f, dcR = 0.0f;
    float dcPrevInL = 0.0f, dcPrevInR = 0.0f;
};
