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
    juce::TextButton discoverBtn { "Discover MoonPad \xe2\x86\x92" };

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

    void setupKnob(juce::Slider& knob, juce::Label& label, const juce::String& text);
    void paintStarfield(juce::Graphics& g, juce::Rectangle<float> area);
    void paintMoon(juce::Graphics& g, float cx, float cy, float radius, float energy);
    void paintRings(juce::Graphics& g, float cx, float cy, float maxRadius);
    void paintParticles(juce::Graphics& g, float cx, float cy);
    void updateAnimation();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoonVerbEditor)
};
