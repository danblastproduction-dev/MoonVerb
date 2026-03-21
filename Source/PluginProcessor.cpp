#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

MoonVerbProcessor::MoonVerbProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

MoonVerbProcessor::~MoonVerbProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout MoonVerbProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("damping", 1), "Damping", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("width", 1), "Width", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("shimmer", 1), "Shimmer", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("preDelay", 1), "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("modRate", 1), "Mod Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("modDepth", 1), "Mod Depth", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("freeze", 1), "Freeze", false));

    return { params.begin(), params.end() };
}

void MoonVerbProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock);
}

void MoonVerbProcessor::releaseResources() { reverb.reset(); }

void MoonVerbProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mix      = apvts.getRawParameterValue("mix")->load();
    auto decay    = apvts.getRawParameterValue("decay")->load();
    auto damping  = apvts.getRawParameterValue("damping")->load();
    auto width    = apvts.getRawParameterValue("width")->load();
    auto shimAmt  = apvts.getRawParameterValue("shimmer")->load();
    auto preDelay = apvts.getRawParameterValue("preDelay")->load();
    auto modRate  = apvts.getRawParameterValue("modRate")->load();
    auto modDepth = apvts.getRawParameterValue("modDepth")->load();
    auto freeze   = apvts.getRawParameterValue("freeze")->load() > 0.5f;

    reverb.setParameters(mix, decay, damping, width, shimAmt, preDelay, modRate, modDepth);
    reverb.setFreeze(freeze);

    if (buffer.getNumChannels() >= 2)
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    else if (buffer.getNumChannels() == 1)
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
}

float MoonVerbProcessor::getReverbEnergy() const
{
    float sum = 0.0f;
    for (int i = 0; i < 8; ++i)
        sum += reverb.tapEnergy[i].load(std::memory_order_relaxed);
    return sum / 8.0f;
}

int MoonVerbProcessor::getNumPrograms() { return MoonVerbPresets::count; }
int MoonVerbProcessor::getCurrentProgram() { return currentPreset; }

void MoonVerbProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= MoonVerbPresets::count) return;
    currentPreset = index;
    const auto& p = MoonVerbPresets::presets[index];

    apvts.getParameter("mix")->setValueNotifyingHost(p.mix);
    apvts.getParameter("decay")->setValueNotifyingHost(p.decay);
    apvts.getParameter("damping")->setValueNotifyingHost(p.damping);
    apvts.getParameter("width")->setValueNotifyingHost(p.width);
    apvts.getParameter("shimmer")->setValueNotifyingHost(p.shimmer);
    apvts.getParameter("preDelay")->setValueNotifyingHost(
        apvts.getParameterRange("preDelay").convertTo0to1(p.preDelayMs));
    apvts.getParameter("modRate")->setValueNotifyingHost(
        apvts.getParameterRange("modRate").convertTo0to1(p.modRate));
    apvts.getParameter("modDepth")->setValueNotifyingHost(p.modDepth);
    apvts.getParameter("freeze")->setValueNotifyingHost(p.freeze ? 1.0f : 0.0f);
}

const juce::String MoonVerbProcessor::getProgramName(int index)
{
    if (index < 0 || index >= MoonVerbPresets::count) return {};
    return MoonVerbPresets::presets[index].name;
}

void MoonVerbProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("currentPreset", currentPreset, nullptr);
    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void MoonVerbProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        auto newState = juce::ValueTree::fromXml(*xml);
        currentPreset = newState.getProperty("currentPreset", 0);
        apvts.replaceState(newState);
    }
}

juce::AudioProcessorEditor* MoonVerbProcessor::createEditor()
{
    return new MoonVerbEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoonVerbProcessor();
}
