#pragma once
#include <array>
#include <atomic>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Limiter.h"
#include "LoudnessMeter.h"

struct DisplayFrame
{
    float inPeakDb = -100.0f;
    float outPeakDb = -100.0f;
    float grDb = 0.0f;
};

class LproSilverProcessor : public juce::AudioProcessor
{
public:
    LproSilverProcessor();
    ~LproSilverProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LPRO SILVER"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // dados para a interface
    static constexpr int numFrames = 1024;
    std::array<DisplayFrame, numFrames> frames {};
    std::atomic<int> frameWrite { 0 };
    std::atomic<float> outTruePeakDb { -100.0f };
    std::atomic<float> currentGrDb { 0.0f };
    LoudnessMeter loudness;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyConfig (int log2Factor, int baseLookaheadSamples);
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int n, int numCh,
                       int log2Factor, float ceilingDb, float linkT, float linkR,
                       float releaseMs, float styleMul, int ditherMode);

    double sr = 44100.0;
    int curLog2 = -1, curBaseN = -1;
    Limiter limiter;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 5> oversamplers;
    juce::SmoothedValue<float> gainSmoothed;
    juce::Random rnd;

    std::atomic<float>* pGain = nullptr;
    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pLook = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pStyle = nullptr;
    std::atomic<float>* pOs = nullptr;
    std::atomic<float>* pLinkT = nullptr;
    std::atomic<float>* pLinkR = nullptr;
    std::atomic<float>* pTp = nullptr;
    std::atomic<float>* pDither = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LproSilverProcessor)
};
