#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour      { 0xff10141d };
    const juce::Colour panelColour   { 0xff171d2a };
    const juce::Colour silverColour  { 0xffc9d1de };
    const juce::Colour accentColour  { 0xff7fa6d6 };
    const juce::Colour redColour     { 0xffe0505a };

    // escala vertical do display: +12 dB (topo) a -42 dB (base)
    constexpr float dbTop = 12.0f, dbBottom = -42.0f;
}

//==============================================================================
SilverLookAndFeel::SilverLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, silverColour);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, accentColour);
    setColour (juce::Slider::thumbColourId, silverColour);
    setColour (juce::Label::textColourId, silverColour);
    setColour (juce::ComboBox::backgroundColourId, panelColour);
    setColour (juce::ComboBox::textColourId, silverColour);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2c3548));
    setColour (juce::ComboBox::arrowColourId, silverColour);
    setColour (juce::PopupMenu::backgroundColourId, panelColour);
    setColour (juce::PopupMenu::textColourId, silverColour);
    setColour (juce::ToggleButton::textColourId, silverColour);
    setColour (juce::ToggleButton::tickColourId, accentColour);
    setColour (juce::TextButton::buttonColourId, panelColour);
    setColour (juce::TextButton::textColourOffId, silverColour);
}

void SilverLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    const float r = d * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff1b2231));
    g.fillEllipse (c.x - r, c.y - r, d, d);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff3a455c));
    g.strokePath (track, juce::PathStrokeType (3.0f));

    juce::Path value;
    value.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, angle, true);
    g.setColour (silverColour);
    g.strokePath (value, juce::PathStrokeType (3.0f));

    juce::Path pointer;
    pointer.addRectangle (-1.5f, -r + 7.0f, 3.0f, r * 0.42f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
WaveformDisplay::WaveformDisplay (LproSilverProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();
    g.fillAll (juce::Colour (0xff0d111a));

    auto yDb = [&] (float db)
    {
        return juce::jmap (juce::jlimit (dbBottom, dbTop, db), dbTop, dbBottom, 0.0f, h);
    };

    // grade
    g.setFont (10.0f);
    for (int db = 6; db >= -36; db -= 6)
    {
        const float y = yDb ((float) db);
        g.setColour (juce::Colour (0x18ffffff));
        g.drawHorizontalLine ((int) y, 0.0f, w);
        g.setColour (juce::Colour (0x66c9d1de));
        g.drawText (juce::String (db) + " dB", getWidth() - 50, (int) y - 12, 46, 12,
                    juce::Justification::right);
    }

    const int cols = juce::jmax (1, (int) w);
    const int total = LproSilverProcessor::numFrames;
    const int oldest = proc.frameWrite.load (std::memory_order_acquire);

    juce::Path inPath, outPath;
    inPath.startNewSubPath (0.0f, h);
    outPath.startNewSubPath (0.0f, h);

    for (int x = 0; x < cols; ++x)
    {
        const int idx = (oldest + (int) ((double) x / (double) cols * total)) % total;
        const auto& f = proc.frames[(size_t) idx];
        inPath.lineTo ((float) x, yDb (f.inPeakDb));
        outPath.lineTo ((float) x, yDb (f.outPeakDb));
    }
    inPath.lineTo (w, h);
    inPath.closeSubPath();
    outPath.lineTo (w, h);
    outPath.closeSubPath();

    g.setColour (juce::Colour (0xff3b4a6b).withAlpha (0.85f));
    g.fillPath (inPath);
    g.setColour (juce::Colour (0xff9fb1d1).withAlpha (0.75f));
    g.fillPath (outPath);

    // redução de ganho, a partir do topo
    const float pxPerDb = h / (dbTop - dbBottom);
    g.setColour (redColour.withAlpha (0.55f));
    for (int x = 0; x < cols; ++x)
    {
        const int idx = (oldest + (int) ((double) x / (double) cols * total)) % total;
        const float gr = -proc.frames[(size_t) idx].grDb;
        if (gr > 0.01f)
            g.fillRect ((float) x, 0.0f, 1.0f, juce::jmin (gr * pxPerDb * 2.0f, h * 0.6f));
    }

    // linha do ceiling
    if (auto* ceil = proc.apvts.getRawParameterValue ("ceiling"))
    {
        g.setColour (redColour);
        g.drawHorizontalLine ((int) yDb (ceil->load()), 0.0f, w);
    }

    g.setColour (silverColour);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("GR " + juce::String (-proc.currentGrDb.load(), 1) + " dB", 8, 6, 120, 16,
                juce::Justification::left);
}

//==============================================================================
MeterPanel::MeterPanel (LproSilverProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void MeterPanel::timerCallback()
{
    level = proc.outTruePeakDb.load();
    hold = juce::jmax (level, hold - 0.7f);
    repaint();
}

void MeterPanel::paint (juce::Graphics& g)
{
    g.fillAll (panelColour);

    const auto bar = juce::Rectangle<float> (14.0f, 24.0f, 34.0f, (float) getHeight() - 70.0f);
    auto yFor = [&] (float db)
    {
        return juce::jmap (juce::jlimit (-36.0f, 0.0f, db), 0.0f, -36.0f, bar.getY(), bar.getBottom());
    };

    g.setColour (juce::Colour (0xff0d111a));
    g.fillRect (bar);

    const float yTop = yFor (level);
    juce::ColourGradient grad (redColour, 0.0f, bar.getY(),
                               juce::Colour (0xff4fb286), 0.0f, bar.getBottom(), false);
    grad.addColour (0.25, juce::Colour (0xffe3c34f));
    g.setGradientFill (grad);
    g.fillRect (bar.getX(), yTop, bar.getWidth(), bar.getBottom() - yTop);

    g.setColour (juce::Colours::white);
    g.fillRect (bar.getX(), yFor (hold), bar.getWidth(), 2.0f);

    g.setColour (juce::Colour (0x88c9d1de));
    g.setFont (juce::FontOptions (10.0f));
    for (int db = 0; db >= -36; db -= 6)
        g.drawText (juce::String (db), (int) bar.getRight() + 4, (int) yFor ((float) db) - 6, 28, 12,
                    juce::Justification::left);

    g.setColour (silverColour);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("OUTPUT TP", 8, 4, 100, 16, juce::Justification::left);
    g.drawText (juce::String (hold, 1) + " dBTP", 8, getHeight() - 42, 100, 16, juce::Justification::left);

    // LUFS
    const int x0 = 96;
    auto row = [&] (const juce::String& name, float v, int y)
    {
        g.setColour (juce::Colour (0x99c9d1de));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (name, x0, y, 80, 14, juce::Justification::left);
        g.setColour (silverColour);
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText (v <= -99.0f ? juce::String ("-inf") : juce::String (v, 1), x0, y + 14, 80, 24,
                    juce::Justification::left);
    };
    g.setColour (silverColour);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("LOUDNESS (LUFS)", x0, 4, 120, 16, juce::Justification::left);
    row ("Momentary", proc.loudness.momentary.load(), 30);
    row ("Short term", proc.loudness.shortTerm.load(), 90);
    row ("Integrated", proc.loudness.integrated.load(), 150);
}

//==============================================================================
LproSilverEditor::LproSilverEditor (LproSilverProcessor& p)
    : AudioProcessorEditor (&p), proc (p), wave (p), meters (p)
{
    setLookAndFeel (&laf);
    setSize (900, 540);

    addAndMakeVisible (wave);
    addAndMakeVisible (meters);

    setupKnob (gain, " dB");
    setupKnob (lookahead, " ms");
    setupKnob (attack, " %");
    setupKnob (release, " ms");
    setupKnob (linkT, " %");
    setupKnob (linkR, " %");

    ceiling.setSliderStyle (juce::Slider::LinearVertical);
    ceiling.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    ceiling.setTextValueSuffix (" dB");
    addAndMakeVisible (ceiling);

    setupCombo (style, "style");
    setupCombo (oversampling, "oversampling");
    setupCombo (dither, "dither");

    addAndMakeVisible (truePeak);
    addAndMakeVisible (resetLufs);
    resetLufs.onClick = [this] { proc.loudness.requestReset(); };

    auto& a = proc.apvts;
    gainAtt = std::make_unique<SliderAtt> (a, "gain", gain);
    lookAtt = std::make_unique<SliderAtt> (a, "lookahead", lookahead);
    attackAtt = std::make_unique<SliderAtt> (a, "attack", attack);
    releaseAtt = std::make_unique<SliderAtt> (a, "release", release);
    linkTAtt = std::make_unique<SliderAtt> (a, "linkTrans", linkT);
    linkRAtt = std::make_unique<SliderAtt> (a, "linkRel", linkR);
    ceilAtt = std::make_unique<SliderAtt> (a, "ceiling", ceiling);
    styleAtt = std::make_unique<ComboAtt> (a, "style", style);
    osAtt = std::make_unique<ComboAtt> (a, "oversampling", oversampling);
    ditherAtt = std::make_unique<ComboAtt> (a, "dither", dither);
    tpAtt = std::make_unique<ButtonAtt> (a, "truePeak", truePeak);
}

LproSilverEditor::~LproSilverEditor()
{
    setLookAndFeel (nullptr);
}

void LproSilverEditor::setupKnob (juce::Slider& s, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    s.setTextValueSuffix (suffix);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                           juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (s);
}

void LproSilverEditor::setupCombo (juce::ComboBox& c, const juce::String& paramId)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c.addItemList (p->choices, 1);
    addAndMakeVisible (c);
}

void LproSilverEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);

    g.setColour (panelColour);
    g.fillRect (0, 0, getWidth(), 36);
    g.setColour (silverColour);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("LPRO SILVER", 14, 4, 220, 28, juce::Justification::left);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colour (0x99c9d1de));
    g.drawText ("True Peak Limiter", 190, 4, 200, 28, juce::Justification::left);

    g.setColour (panelColour);
    g.fillRect (0, 384, getWidth(), getHeight() - 384);

    g.setColour (silverColour);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (auto& l : labels)
        g.drawText (l.first, l.second, juce::Justification::centred);
}

void LproSilverEditor::resized()
{
    wave.setBounds (10, 44, 640, 332);
    meters.setBounds (660, 44, 180, 332);
    ceiling.setBounds (848, 62, 44, 314);

    labels.clear();
    labels.emplace_back ("CEILING", juce::Rectangle<int> (840, 44, 60, 16));

    juce::Slider* knobs[] = { &gain, &lookahead, &attack, &release, &linkT, &linkR };
    const char* names[] = { "GAIN", "LOOKAHEAD", "ATTACK", "RELEASE", "LINK TRANSIENTS", "LINK RELEASE" };
    for (int i = 0; i < 6; ++i)
    {
        const int x = 14 + i * 106;
        labels.emplace_back (names[i], juce::Rectangle<int> (x, 394, 104, 16));
        knobs[i]->setBounds (x, 412, 104, 118);
    }

    labels.emplace_back ("STYLE", juce::Rectangle<int> (668, 394, 100, 16));
    style.setBounds (668, 412, 110, 26);
    labels.emplace_back ("OVERSAMPLING", juce::Rectangle<int> (786, 394, 110, 16));
    oversampling.setBounds (786, 412, 110, 26);
    labels.emplace_back ("DITHER", juce::Rectangle<int> (668, 446, 100, 16));
    dither.setBounds (668, 464, 110, 26);
    truePeak.setBounds (790, 464, 106, 26);
    resetLufs.setBounds (668, 502, 228, 24);
}
