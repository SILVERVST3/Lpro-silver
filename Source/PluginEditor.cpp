#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    // paleta prata / cinza claro
    const juce::Colour bgColour      { 0xffc9ced6 };
    const juce::Colour panelColour   { 0xffdfe3e8 };
    const juce::Colour displayBg     { 0xffeceff3 };
    const juce::Colour controlBg     { 0xfff3f5f7 };
    const juce::Colour textColour    { 0xff262c36 };
    const juce::Colour subTextColour { 0xff5b6472 };
    const juce::Colour accentColour  { 0xff3f4958 };
    const juce::Colour lineColour    { 0xff9aa3b0 };
    const juce::Colour redColour     { 0xffd24b55 };

    // escala vertical do display: +12 dB (topo) a -42 dB (base)
    constexpr float dbTop = 12.0f, dbBottom = -42.0f;

    // suavização triangular repetida (equivale a um filtro gaussiano aproximado)
    std::vector<float> smoothValues (std::vector<float> a, int radius, int passes)
    {
        const int n = (int) a.size();
        if (n < 3)
            return a;

        std::vector<float> b (a.size());
        for (int p = 0; p < passes; ++p)
        {
            for (int i = 0; i < n; ++i)
            {
                float sum = 0.0f, weights = 0.0f;
                for (int k = -radius; k <= radius; ++k)
                {
                    const int j = std::clamp (i + k, 0, n - 1);
                    const float w = (float) (radius + 1 - std::abs (k));
                    sum += a[(size_t) j] * w;
                    weights += w;
                }
                b[(size_t) i] = sum / weights;
            }
            a.swap (b);
        }
        return a;
    }

    // monta uma área preenchida com curvas suaves (pontos médios + curvas quadráticas)
    void addSmoothArea (juce::Path& path, const std::vector<float>& ys, float step, float width, float baseY)
    {
        const int n = (int) ys.size();
        if (n < 2)
            return;

        path.startNewSubPath (0.0f, baseY);
        path.lineTo (0.0f, ys[0]);

        for (int i = 1; i < n - 1; ++i)
        {
            const float x = (float) i * step;
            const float xn = (float) (i + 1) * step;
            path.quadraticTo (x, ys[(size_t) i], (x + xn) * 0.5f, (ys[(size_t) i] + ys[(size_t) i + 1]) * 0.5f);
        }

        path.lineTo (width, ys[(size_t) n - 1]);
        path.lineTo (width, baseY);
        path.closeSubPath();
    }
}

//==============================================================================
SilverLookAndFeel::SilverLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, accentColour);
    setColour (juce::Slider::backgroundColourId, lineColour);
    setColour (juce::Slider::thumbColourId, accentColour);
    setColour (juce::Label::textColourId, textColour);
    setColour (juce::ComboBox::backgroundColourId, controlBg);
    setColour (juce::ComboBox::textColourId, textColour);
    setColour (juce::ComboBox::outlineColourId, lineColour);
    setColour (juce::ComboBox::arrowColourId, accentColour);
    setColour (juce::PopupMenu::backgroundColourId, controlBg);
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, bgColour);
    setColour (juce::PopupMenu::highlightedTextColourId, textColour);
    setColour (juce::ToggleButton::textColourId, textColour);
    setColour (juce::ToggleButton::tickColourId, accentColour);
    setColour (juce::ToggleButton::tickDisabledColourId, lineColour);
    setColour (juce::TextButton::buttonColourId, controlBg);
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::TextButton::textColourOnId, textColour);
}

void SilverLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    const float r = d * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // corpo metálico
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafbfc), c.x, c.y - r,
                                             juce::Colour (0xffb4bbc7), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, d, d);
    g.setColour (lineColour);
    g.drawEllipse (c.x - r, c.y - r, d, d, 1.2f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xffa4acb9));
    g.strokePath (track, juce::PathStrokeType (3.0f));

    juce::Path value;
    value.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, angle, true);
    g.setColour (accentColour);
    g.strokePath (value, juce::PathStrokeType (3.0f));

    juce::Path pointer;
    pointer.addRectangle (-1.5f, -r + 8.0f, 3.0f, r * 0.40f);
    g.setColour (juce::Colour (0xff1f242d));
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

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f7f9), 0.0f, 0.0f,
                                             juce::Colour (0xffdfe3e8), 0.0f, h, false));
    g.fillRect (b);

    auto yDb = [&] (float db)
    {
        return juce::jmap (juce::jlimit (dbBottom, dbTop, db), dbTop, dbBottom, 0.0f, h);
    };

    // grade
    g.setFont (juce::FontOptions (10.0f));
    for (int db = 6; db >= -36; db -= 6)
    {
        const float y = yDb ((float) db);
        g.setColour (juce::Colour (0x22000000));
        g.drawHorizontalLine ((int) y, 0.0f, w);
        g.setColour (subTextColour);
        g.drawText (juce::String (db) + " dB", getWidth() - 50, (int) y - 12, 46, 12,
                    juce::Justification::right);
    }

    // pontos do desenho: um a cada 2 px, já suavizados
    constexpr float step = 2.0f;
    const int points = juce::jmax (3, (int) (w / step) + 1);
    const int total = LproSilverProcessor::numFrames;
    const int oldest = proc.frameWrite.load (std::memory_order_acquire);
    const float pxPerDb = h / (dbTop - dbBottom);

    std::vector<float> inDb ((size_t) points), outDb ((size_t) points), grPx ((size_t) points);
    for (int i = 0; i < points; ++i)
    {
        const int idx = (oldest + (int) ((double) i / (double) (points - 1) * (total - 1))) % total;
        const auto& f = proc.frames[(size_t) idx];
        inDb[(size_t) i] = juce::jlimit (dbBottom, dbTop, f.inPeakDb);
        outDb[(size_t) i] = juce::jlimit (dbBottom, dbTop, f.outPeakDb);
        grPx[(size_t) i] = juce::jmin (-f.grDb * pxPerDb * 2.0f, h * 0.6f);
    }

    inDb = smoothValues (inDb, 5, 3);
    outDb = smoothValues (outDb, 5, 3);
    grPx = smoothValues (grPx, 4, 2);

    for (auto& v : inDb)  v = yDb (v);
    for (auto& v : outDb) v = yDb (v);

    juce::Path inPath, outPath, grPath;
    addSmoothArea (inPath, inDb, step, w, h);
    addSmoothArea (outPath, outDb, step, w, h);
    addSmoothArea (grPath, grPx, step, w, 0.0f);

    g.setColour (juce::Colour (0xff9aa5b8).withAlpha (0.75f));
    g.fillPath (inPath);
    g.setColour (juce::Colour (0xff4d596b).withAlpha (0.80f));
    g.fillPath (outPath);
    g.setColour (redColour.withAlpha (0.55f));
    g.fillPath (grPath);

    // linha do ceiling
    if (auto* ceil = proc.apvts.getRawParameterValue ("ceiling"))
    {
        g.setColour (redColour);
        g.drawHorizontalLine ((int) yDb (ceil->load()), 0.0f, w);
    }

    g.setColour (lineColour);
    g.drawRect (b, 1.0f);

    g.setColour (textColour);
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
    g.fillAll (displayBg);
    g.setColour (lineColour);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);

    const auto bar = juce::Rectangle<float> (14.0f, 24.0f, 34.0f, (float) getHeight() - 70.0f);
    auto yFor = [&] (float db)
    {
        return juce::jmap (juce::jlimit (-36.0f, 0.0f, db), 0.0f, -36.0f, bar.getY(), bar.getBottom());
    };

    g.setColour (juce::Colour (0xffc3c9d2));
    g.fillRect (bar);

    const float yTop = yFor (level);
    juce::ColourGradient grad (redColour, 0.0f, bar.getY(),
                               juce::Colour (0xff4fa37c), 0.0f, bar.getBottom(), false);
    grad.addColour (0.25, juce::Colour (0xffd9b43c));
    g.setGradientFill (grad);
    g.fillRect (bar.getX(), yTop, bar.getWidth(), bar.getBottom() - yTop);

    g.setColour (textColour);
    g.fillRect (bar.getX(), yFor (hold), bar.getWidth(), 2.0f);

    g.setColour (subTextColour);
    g.setFont (juce::FontOptions (10.0f));
    for (int db = 0; db >= -36; db -= 6)
        g.drawText (juce::String (db), (int) bar.getRight() + 4, (int) yFor ((float) db) - 6, 28, 12,
                    juce::Justification::left);

    g.setColour (textColour);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("OUTPUT TP", 8, 4, 100, 16, juce::Justification::left);
    g.drawText (juce::String (hold, 1) + " dBTP", 8, getHeight() - 42, 100, 16, juce::Justification::left);

    // LUFS
    const int x0 = 96;
    auto row = [&] (const juce::String& name, float v, int y)
    {
        g.setColour (subTextColour);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (name, x0, y, 80, 14, juce::Justification::left);
        g.setColour (textColour);
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText (v <= -99.0f ? juce::String ("-inf") : juce::String (v, 1), x0, y + 14, 80, 24,
                    juce::Justification::left);
    };
    g.setColour (textColour);
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
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd5dae1), 0.0f, 0.0f,
                                             bgColour, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // barra de título em prata escovado
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1f3f6), 0.0f, 0.0f,
                                             juce::Colour (0xffbfc5ce), 0.0f, 36.0f, false));
    g.fillRect (0, 0, getWidth(), 36);
    g.setColour (lineColour);
    g.drawHorizontalLine (36, 0.0f, (float) getWidth());

    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawText ("LPRO SILVER", 15, 5, 220, 28, juce::Justification::left);
    g.setColour (textColour);
    g.drawText ("LPRO SILVER", 14, 4, 220, 28, juce::Justification::left);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (subTextColour);
    g.drawText ("True Peak Limiter", 190, 4, 200, 28, juce::Justification::left);

    // painel inferior
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9ecf0), 0.0f, 384.0f,
                                             panelColour, 0.0f, (float) getHeight(), false));
    g.fillRect (0, 384, getWidth(), getHeight() - 384);
    g.setColour (lineColour);
    g.drawHorizontalLine (384, 0.0f, (float) getWidth());

    g.setColour (textColour);
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
