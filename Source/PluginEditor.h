#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MoonVerbLookAndFeel.h"

class MoonVerbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MoonVerbEditor(MoonVerbProcessor&);
    ~MoonVerbEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    MoonVerbProcessor& processor;
    MoonVerbLookAndFeel moonLnf;

    // Knobs
    juce::Slider mixKnob, decayKnob, dampKnob, widthKnob, shimmerKnob, preDelayKnob;
    juce::Slider modRateKnob, modDepthKnob;
    juce::Label mixLabel, decayLabel, dampLabel, widthLabel, shimmerLabel, preDelayLabel;
    juce::Label modRateLabel, modDepthLabel;

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        mixAtt, decayAtt, dampAtt, widthAtt, shimmerAtt, preDelayAtt, modRateAtt, modDepthAtt;

    // Freeze button
    juce::TextButton freezeBtn { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAtt;

    // Preset navigation
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

    // Footer link
    juce::TextButton discoverBtn { "Discover MoonPad >" };

    // Animation state
    float animTime = 0.0f;
    float smoothEnergy = 0.0f;

    int ringCounter = 0;

    struct Star { float x, y, brightness, speed; };
    std::vector<Star> stars;

    struct Ring { float radius, alpha, speed; };
    std::vector<Ring> rings;

    struct Particle { float x, y, vx, vy, alpha, life; };
    std::vector<Particle> particles;

    // Moon texture cache
    juce::Image moonTexture;
    int cachedMoonSize = 0;
    void generateMoonTexture(int size);

    static float gradHash(int ix, int iy)
    {
        unsigned int h = static_cast<unsigned int>(ix * 374761393 + iy * 668265263);
        h = (h ^ (h >> 13)) * 1274126177u;
        return static_cast<float>(h & 0x7FFFFFFF) / 2147483647.0f;
    }

    static float gradNoise(float x, float y)
    {
        int ix = static_cast<int>(std::floor(x));
        int iy = static_cast<int>(std::floor(y));
        float fx = x - ix, fy = y - iy;
        float ux = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
        float uy = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);
        float a = gradHash(ix, iy), b = gradHash(ix + 1, iy);
        float c = gradHash(ix, iy + 1), d = gradHash(ix + 1, iy + 1);
        return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
    }

    static float fbm(float x, float y, int octaves)
    {
        float val = 0.0f, amp = 0.5f, freq = 1.0f;
        for (int i = 0; i < octaves; ++i)
        {
            val += gradNoise(x * freq, y * freq) * amp;
            amp *= 0.5f;
            freq *= 2.07f;
        }
        return val;
    }

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);
    void paintStarfield(juce::Graphics& g, juce::Rectangle<float> area);
    void paintMoon(juce::Graphics& g, float cx, float cy, float radius, float energy);
    void paintRings(juce::Graphics& g, float cx, float cy, float maxRadius);
    void paintParticles(juce::Graphics& g, float cx, float cy);
    void updateAnimation();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoonVerbEditor)
};
