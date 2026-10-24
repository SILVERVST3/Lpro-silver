#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int chunkSize = 256;
    float toDb (float lin) { return juce::Decibels::gainToDecibels (lin, -100.0f); }
}

juce::AudioProcessorValueTreeState::ParameterLayout LproSilverProcessor::createLayout()
{
    using Float = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Float> (juce::ParameterID { "gain", 1 }, "Gain",
                                         Range (0.0f, 30.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "ceiling", 1 }, "Output Ceiling",
                                         Range (-30.0f, 0.0f, 0.01f), -0.3f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "lookahead", 1 }, "Lookahead",
                                         Range (0.0f, 5.0f, 0.01f), 1.5f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "attack", 1 }, "Attack",
                                         Range (0.0f, 100.0f, 0.1f), 100.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "release", 1 }, "Release",
                                         Range (10.0f, 2000.0f, 0.1f, 0.4f), 200.0f));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "style", 1 }, "Style",
                                          juce::StringArray { "Transparent", "Modern", "Aggressive", "Punchy" }, 1));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "oversampling", 1 }, "Oversampling",
                                          juce::StringArray { "Off", "2x", "4x", "8x", "16x" }, 2));
    layout.add (std::make_unique<Float> (juce::ParameterID { "linkTrans", 1 }, "Link Transients",
                                         Range (0.0f, 100.0f, 0.1f), 100.0f));
    layout.add (std::make_unique<Float> (juce::ParameterID { "linkRel", 1 }, "Link Release",
                                         Range (0.0f, 100.0f, 0.1f), 100.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "truePeak", 1 },
                                                            "True Peak", true));
    layout.add (std::make_unique<Choice> (juce::ParameterID { "dither", 1 }, "Dither",
                                          juce::StringArray { "Off", "16 bit", "24 bit" }, 0));
    return layout;
}

LproSilverProcessor::LproSilverProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pGain = apvts.getRawParameterValue ("gain");
    pCeiling = apvts.getRawParameterValue ("ceiling");
    pLook = apvts.getRawParameterValue ("lookahead");
    pAttack = apvts.getRawParameterValue ("attack");
    pRelease = apvts.getRawParameterValue ("release");
    pStyle = apvts.getRawParameterValue ("style");
    pOs = apvts.getRawParameterValue ("oversampling");
    pLinkT = apvts.getRawParameterValue ("linkTrans");
    pLinkR = apvts.getRawParameterValue ("linkRel");
    pTp = apvts.getRawParameterValue ("truePeak");
    pDither = apvts.getRawParameterValue ("dither");
}

bool LproSilverProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void LproSilverProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    for (int k = 1; k < 5; ++k)
    {
        oversamplers[(size_t) k] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) k, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversamplers[(size_t) k]->initProcessing ((size_t) chunkSize);
    }

    const int maxWindow = (int) std::ceil (0.005 * sr + 1.0) * 16 + 16;
    limiter.prepare (maxWindow);
    loudness.prepare (sr);
    gainSmoothed.reset (sr, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    curLog2 = -1;
    curBaseN = -1;
}

void LproSilverProcessor::applyConfig (int log2Factor, int baseN)
{
    if (log2Factor == curLog2 && baseN == curBaseN)
        return;

    curLog2 = log2Factor;
    curBaseN = baseN;

    limiter.configure (baseN * (1 << log2Factor));

    int latency = baseN;
    if (log2Factor > 0)
    {
        oversamplers[(size_t) log2Factor]->reset();
        latency += (int) std::lround (oversamplers[(size_t) log2Factor]->getLatencyInSamples());
    }
    setLatencySamples (latency);
}

void LproSilverProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (2, getTotalNumInputChannels(), buffer.getNumChannels());
    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    if (numCh == 0)
        return;

    const bool tp = pTp->load() > 0.5f;
    const int log2Factor = juce::jmax ((int) pOs->load(), tp ? 2 : 0);
    const int baseN = (int) std::lround ((double) pLook->load() * 0.001 * sr);
    applyConfig (log2Factor, baseN);

    limiter.setNumChannels (numCh);
    limiter.setAttackFraction (pAttack->load() * 0.01f);
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    static const float styleMul[] = { 1.6f, 1.0f, 0.45f, 0.7f };
    const float mul = styleMul[juce::jlimit (0, 3, (int) pStyle->load())];

    const int total = buffer.getNumSamples();
    for (int done = 0; done < total; done += chunkSize)
        processChunk (buffer, done, juce::jmin (chunkSize, total - done), numCh, log2Factor,
                      pCeiling->load(), pLinkT->load() * 0.01f, pLinkR->load() * 0.01f,
                      pRelease->load(), mul, (int) pDither->load());
}

void LproSilverProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int n, int numCh,
                                        int log2Factor, float ceilingDb, float linkT, float linkR,
                                        float releaseMs, float styleMul, int ditherMode)
{
    float* ptrs[2] = { nullptr, nullptr };
    for (int c = 0; c < numCh; ++c)
        ptrs[c] = buffer.getWritePointer (c, start);

    // ganho de entrada
    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = gainSmoothed.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            ptrs[c][i] *= g;
            inPeak = juce::jmax (inPeak, std::abs (ptrs[c][i]));
        }
    }

    const float ceilingLin = juce::Decibels::decibelsToGain (ceilingDb);
    const double fsOS = sr * (double) (1 << log2Factor);
    const float relCoef = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * (double) styleMul * fsOS));

    juce::dsp::AudioBlock<float> block (ptrs, (size_t) numCh, (size_t) n);

    if (log2Factor > 0)
    {
        auto& os = *oversamplers[(size_t) log2Factor];
        auto up = os.processSamplesUp (block);
        float* upPtrs[2] = { nullptr, nullptr };
        for (int c = 0; c < numCh; ++c)
            upPtrs[c] = up.getChannelPointer ((size_t) c);

        limiter.process (upPtrs, (int) up.getNumSamples(), ceilingLin, linkT, linkR, relCoef);
        os.processSamplesDown (block);
    }
    else
    {
        limiter.process (ptrs, n, ceilingLin, linkT, linkR, relCoef);
    }

    // segurança final + dither + medição
    float outPeak = 0.0f;
    const float lsb = ditherMode == 1 ? 1.0f / 32768.0f : (ditherMode == 2 ? 1.0f / 8388608.0f : 0.0f);
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
        {
            float v = juce::jlimit (-ceilingLin, ceilingLin, ptrs[c][i]);
            if (lsb > 0.0f)
                v += (rnd.nextFloat() - rnd.nextFloat()) * lsb;
            ptrs[c][i] = v;
            outPeak = juce::jmax (outPeak, std::abs (v));
        }

    const float grDb = toDb (limiter.blockMinGain);
    currentGrDb.store (grDb);
    outTruePeakDb.store (toDb (juce::jmax (outPeak, limiter.blockPeak)));

    const int w = frameWrite.load (std::memory_order_relaxed);
    frames[(size_t) w] = { toDb (inPeak), toDb (outPeak), grDb };
    frameWrite.store ((w + 1) % numFrames, std::memory_order_release);

    loudness.process (ptrs, numCh, n);
}

juce::AudioProcessorEditor* LproSilverProcessor::createEditor()
{
    return new LproSilverEditor (*this);
}

void LproSilverProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LproSilverProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LproSilverProcessor();
}
