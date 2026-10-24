#pragma once
#include <memory>
#include <utility>
#include <vector>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class SilverLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SilverLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};

// Display de forma de onda: entrada, saída e redução de ganho (vermelho, a partir do topo)
class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    explicit WaveformDisplay (LproSilverProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    LproSilverProcessor& proc;
};

// Medidor de saída (true peak) e leituras LUFS
class MeterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MeterPanel (LproSilverProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    LproSilverProcessor& proc;
    float level = -100.0f, hold = -100.0f;
};

class LproSilverEditor : public juce::AudioProcessorEditor
{
public:
    explicit LproSilverEditor (LproSilverProcessor&);
    ~LproSilverEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void setupKnob (juce::Slider&, const juce::String& suffix);
    void setupCombo (juce::ComboBox&, const juce::String& paramId);

    LproSilverProcessor& proc;
    SilverLookAndFeel laf;

    WaveformDisplay wave;
    MeterPanel meters;

    juce::Slider gain, lookahead, attack, release, linkT, linkR, ceiling;
    juce::ComboBox style, oversampling, dither;
    juce::ToggleButton truePeak { "True Peak" };
    juce::TextButton resetLufs { "Reset LUFS integrado" };

    std::vector<std::pair<juce::String, juce::Rectangle<int>>> labels;

    std::unique_ptr<SliderAtt> gainAtt, lookAtt, attackAtt, releaseAtt, linkTAtt, linkRAtt, ceilAtt;
    std::unique_ptr<ComboAtt> styleAtt, osAtt, ditherAtt;
    std::unique_ptr<ButtonAtt> tpAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LproSilverEditor)
};
