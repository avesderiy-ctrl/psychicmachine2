#include "PluginEditor.h"
#include <BinaryData.h>

namespace col
{
inline juce::Colour pink()   { return juce::Colour(0xffff1050); } // Psychick cross red-pink
inline juce::Colour bone()   { return juce::Colour(0xffe9e2d0); }
inline juce::Colour black()  { return juce::Colour(0xff050405); }
inline juce::Colour acid()   { return juce::Colour(0xffb6ff00); }
inline juce::Colour crystal(){ return juce::Colour(0xff7df9ff); }
} // namespace col

//==============================================================================
// Thee Psychick cross: vertical bar + three horizontal bars (wide / short / wide).
static void drawPsychickCross(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    g.setColour(c);
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    g.fillRect(x + w * 0.40f, y, w * 0.20f, h);                                 // spine
    g.fillRect(x,             y + h * 0.137f, w,        h * 0.149f);            // top bar
    g.fillRect(x + w * 0.20f, y + h * 0.429f, w * 0.60f, h * 0.137f);           // middle bar
    g.fillRect(x,             y + h * 0.714f, w,        h * 0.143f);            // bottom bar
}

static void drawPentagram(juce::Graphics& g, juce::Point<float> c, float R, float rot, float thickness)
{
    juce::Path p;
    for (int i = 0; i < 5; ++i)
    {
        const float a = rot + (float) ((i * 2) % 5) * juce::MathConstants<float>::twoPi / 5.f
                        - juce::MathConstants<float>::halfPi;
        const juce::Point<float> pt(c.x + R * std::cos(a), c.y + R * std::sin(a));
        if (i == 0) p.startNewSubPath(pt); else p.lineTo(pt);
    }
    p.closeSubPath();
    g.strokePath(p, juce::PathStrokeType(thickness));
    g.drawEllipse(c.x - R, c.y - R, 2.f * R, 2.f * R, thickness);
}

// Eight-arrow chaos star (chaos magick)
static void drawChaosStar(juce::Graphics& g, juce::Point<float> c, float R, float rot, float thickness)
{
    for (int k = 0; k < 8; ++k)
    {
        const float a = rot + (float) k * juce::MathConstants<float>::twoPi / 8.f;
        const juce::Point<float> tip(c.x + R * std::cos(a), c.y + R * std::sin(a));
        const juce::Point<float> base(c.x + R * 0.18f * std::cos(a), c.y + R * 0.18f * std::sin(a));
        g.drawLine(base.x, base.y, tip.x, tip.y, thickness);
        juce::Path head;
        const float hs = R * 0.14f;
        head.addTriangle(tip.x, tip.y,
                         tip.x - hs * std::cos(a - 0.45f), tip.y - hs * std::sin(a - 0.45f),
                         tip.x - hs * std::cos(a + 0.45f), tip.y - hs * std::sin(a + 0.45f));
        g.fillPath(head);
    }
}

static void drawEyeInTriangle(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float pupil)
{
    juce::Path tri;
    tri.addTriangle(r.getCentreX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getBottom());
    g.setColour(c);
    g.strokePath(tri, juce::PathStrokeType(1.6f));
    const auto ec = juce::Point<float>(r.getCentreX(), r.getY() + r.getHeight() * 0.62f);
    const float ew = r.getWidth() * 0.26f, eh = r.getHeight() * 0.11f;
    juce::Path eye;
    eye.startNewSubPath(ec.x - ew, ec.y);
    eye.quadraticTo(ec.x, ec.y - eh * 2.f, ec.x + ew, ec.y);
    eye.quadraticTo(ec.x, ec.y + eh * 2.f, ec.x - ew, ec.y);
    g.strokePath(eye, juce::PathStrokeType(1.4f));
    const float pr = eh * (0.7f + 0.9f * pupil);
    g.fillEllipse(ec.x - pr, ec.y - pr, pr * 2.f, pr * 2.f);
}

//==============================================================================
PsychickLookAndFeel::PsychickLookAndFeel()
{
    setColour(juce::Label::textColourId, col::bone());
    setColour(juce::Slider::textBoxTextColourId, col::bone());
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, col::bone());
    setColour(juce::ToggleButton::tickColourId, col::pink());
    setColour(juce::ToggleButton::tickDisabledColourId, col::bone().withAlpha(0.4f));
}

juce::Font PsychickLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(11.5f));
}

void PsychickLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.f);
    const float R = std::min(b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const auto accent = s.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = startAngle + pos * (endAngle - startAngle);

    auto pt = [&](float a, float r) { return juce::Point<float>(c.x + r * std::sin(a), c.y - r * std::cos(a)); };

    // outer ring + ticks
    g.setColour(col::bone().withAlpha(0.28f));
    g.drawEllipse(c.x - R, c.y - R, 2.f * R, 2.f * R, 1.f);
    for (int i = 0; i <= 12; ++i)
    {
        const float a = startAngle + (float) i / 12.f * (endAngle - startAngle);
        const bool major = (i % 3) == 0;
        g.setColour(col::bone().withAlpha(major ? 0.7f : 0.35f));
        g.drawLine({ pt(a, R * (major ? 0.84f : 0.89f)), pt(a, R * 0.96f) }, major ? 1.6f : 1.f);
    }

    // value arc
    juce::Path arc;
    arc.addCentredArc(c.x, c.y, R * 0.76f, R * 0.76f, 0.f, startAngle, angle, true);
    g.setColour(accent.withAlpha(0.25f));
    g.strokePath(arc, juce::PathStrokeType(7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(accent);
    g.strokePath(arc, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // sigil body
    const float rb = R * 0.62f;
    juce::ColourGradient body(juce::Colour(0xff2a2024), c.x, c.y - rb, juce::Colour(0xff060506), c.x, c.y + rb, false);
    g.setGradientFill(body);
    g.fillEllipse(c.x - rb, c.y - rb, rb * 2.f, rb * 2.f);
    g.setColour(col::bone().withAlpha(0.5f));
    g.drawEllipse(c.x - rb, c.y - rb, rb * 2.f, rb * 2.f, 1.2f);

    // pointer: needle with diamond tip
    g.setColour(accent);
    g.drawLine({ pt(angle, rb * 0.18f), pt(angle, rb * 0.92f) }, 2.2f);
    juce::Path tip;
    const auto t = pt(angle, rb * 0.92f);
    tip.addEllipse(t.x - 2.6f, t.y - 2.6f, 5.2f, 5.2f);
    g.fillPath(tip);
    g.setColour(col::bone().withAlpha(0.8f));
    g.fillEllipse(c.x - 2.f, c.y - 2.f, 4.f, 4.f);
}

//==============================================================================
PsychickEditor::PsychickEditor(PsychickProcessor& p) : juce::AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);
    setSize(960, 640);

    skull = juce::ImageCache::getFromMemory(BinaryData::skull_png, BinaryData::skull_pngSize);

    const auto pk = col::pink(), ac = col::acid(), cr = col::crystal();

    // knob indices in creation order
    int i = 0;
    auto K = [&](const char* id, const char* name, juce::Colour c) { addKnob(id, name, c); return i++; };

    // Row 1
    const int pitch = K("pitch", "PITCH", pk),  grain = K("grain", "GRAIN", pk);
    const int rf = K("ringHz", "FREQ", pk),     rm = K("ringMix", "MIX", pk);
    const int dr = K("drive", "DRIVE", pk),     fo = K("fold", "FOLD", pk);
    const int wi = K("width", "WIDTH", cr),     sp = K("spread", "SPREAD", cr);
    // Row 2
    const int ac1 = K("acidCut", "CUTOFF", ac), ac2 = K("acidRes", "RESO", ac), ac3 = K("acidRate", "RATE", ac),
              ac4 = K("acidDepth", "DEPTH", ac), ac5 = K("acidMix", "AMOUNT", ac);
    const int sw1 = K("swirlRate", "RATE", pk), sw2 = K("swirlDepth", "DEPTH", pk), sw3 = K("swirlFb", "FEEDBACK", pk),
              sw4 = K("swirlMix", "AMOUNT", pk);
    // Row 3
    const int c1 = K("crysMix", "AMOUNT", cr), c2 = K("crysFb", "FEEDBACK", cr), c3 = K("crysPitch", "PITCH", cr),
              c4 = K("shatter", "SHATTER", cr);
    const int tr = K("trip", "TRIP", pk);
    const int mx = K("mix", "DRY/WET", pk), ou = K("out", "OUTPUT", pk);

    rows = {
        { { "TRANSMUTE", "pitch shifter", { pitch, grain }, pk, 1.f, false, {} },
          { "SIGIL", "ring modulator", { rf, rm }, pk, 1.f, false, {} },
          { "FIRE", "drive + wavefold", { dr, fo }, pk, 1.f, false, {} },
          { "ASTRAL", "width + spread", { wi, sp }, cr, 1.f, false, {} } },
        { { "ELIXIR", "acid filter", { ac1, ac2, ac3, ac4, ac5 }, ac, 1.f, false, {} },
          { "VORTEX", "phase swirl", { sw1, sw2, sw3, sw4 }, pk, 1.f, false, {} } },
        { { "SCRYING", "crystal shimmer", { c1, c2, c3, c4 }, cr, 1.f, false, {} },
          { "THEE TRIP", "chaos macro", { tr }, pk, 1.f, true, {} },
          { "THE GATE", "mix + output", { mx, ou }, pk, 1.f, false, {} } },
    };
    // weights proportional to knob count (TRIP gets a bit extra for its big seal)
    for (auto& row : rows)
        for (auto& g : row)
            g.weight = (float) g.knobs.size() + (g.eye ? 1.3f : 0.f);

    limitAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "limit", limitButton);
    addAndMakeVisible(limitButton);

    startTimerHz(30);
}

PsychickEditor::~PsychickEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

PsychickEditor::Knob& PsychickEditor::addKnob(const char* paramId, const juce::String& name, juce::Colour accent)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 16);
    k->slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k->slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    k->slider.setPopupDisplayEnabled(false, false, nullptr);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, paramId, k->slider);
    if (auto* prm = proc.apvts.getParameter(paramId))
        k->slider.setDoubleClickReturnValue(true, (double) prm->convertFrom0to1(prm->getDefaultValue()));

    k->label.setText(name, juce::dontSendNotification);
    k->label.setJustificationType(juce::Justification::centred);
    k->label.setInterceptsMouseClicks(false, false);
    k->label.setColour(juce::Label::textColourId, col::bone().withAlpha(0.85f));

    addAndMakeVisible(k->slider);
    addAndMakeVisible(k->label);
    knobs.push_back(std::move(k));
    return *knobs.back();
}

void PsychickEditor::timerCallback()
{
    phase += 0.012f;
    const float target = juce::jlimit(0.f, 1.f, std::max(proc.inLevel.load(), proc.outLevel.load()));
    level += (target - level) * (target > level ? 0.5f : 0.12f);
    repaint();
}

void PsychickEditor::resized()
{
    const int W = getWidth(), H = getHeight();

    limitButton.setBounds(W - 172, 76, 150, 22);

    auto area = getLocalBounds().withTrimmedTop(106).withTrimmedBottom(28).reduced(14, 2);
    const int rowH = area.getHeight() / (int) rows.size();

    for (auto& row : rows)
    {
        auto rowArea = area.removeFromTop(rowH);
        float total = 0.f;
        for (auto& g : row) total += g.weight;
        float x = (float) rowArea.getX();
        for (auto& g : row)
        {
            const int gw = (int) std::round(rowArea.getWidth() * g.weight / total);
            g.bounds = juce::Rectangle<int>((int) x, rowArea.getY(), gw, rowArea.getHeight()).reduced(5);
            x += (float) gw;

            auto inner = g.bounds.withTrimmedTop(30).reduced(4, 2);
            const int cellW = inner.getWidth() / (int) g.knobs.size();
            for (int idx : g.knobs)
            {
                auto cell = inner.removeFromLeft(cellW);
                knobs[(size_t) idx]->label.setBounds(cell.removeFromTop(15));
                knobs[(size_t) idx]->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, std::min(76, cellW), 16);
                knobs[(size_t) idx]->slider.setBounds(cell);
            }
        }
    }

    // scanline overlay, rendered once per size
    scanlines = juce::Image(juce::Image::ARGB, W, H, true);
    juce::Graphics gs(scanlines);
    gs.setColour(juce::Colours::black.withAlpha(0.20f));
    for (int y = 0; y < H; y += 3)
        gs.fillRect(0, y, W, 1);
}

void PsychickEditor::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto centre = b.getCentre().translated(0.f, 24.f);

    g.fillAll(col::black());

    // --- skull test-card, breathing with the audio
    if (skull.isValid())
    {
        const float s = b.getHeight() / (float) skull.getHeight();
        const float w = (float) skull.getWidth() * s;
        g.setOpacity(0.10f + 0.34f * level);
        g.drawImage(skull, juce::Rectangle<float>(b.getCentreX() - w * 0.5f, 0.f, w, b.getHeight()),
                    juce::RectanglePlacement::stretchToFit);
    }

    // --- glow
    juce::ColourGradient glow(col::pink().withAlpha(0.08f + 0.22f * level), centre.x, centre.y,
                              juce::Colours::transparentBlack, 0.f, 0.f, true);
    g.setGradientFill(glow);
    g.fillAll();

    // --- test-card rings
    for (int i = 1; i <= 6; ++i)
    {
        const float r = 60.f * (float) i + 10.f * level * (float) i;
        g.setColour(col::bone().withAlpha(0.035f + 0.02f * level));
        g.drawEllipse(centre.x - r, centre.y - r, 2.f * r, 2.f * r, 1.f);
    }

    // --- rotating sigils
    g.setColour(col::pink().withAlpha(0.16f + 0.35f * level));
    drawPentagram(g, centre, 290.f + 25.f * level, phase * 0.6f, 1.4f);
    g.setColour(col::crystal().withAlpha(0.08f + 0.25f * level));
    drawChaosStar(g, centre, 170.f + 40.f * level, -phase * 1.1f, 1.2f);

    // --- header
    drawPsychickCross(g, { 38.f, 16.f, 36.f, 50.f }, col::pink());
    drawPsychickCross(g, { b.getRight() - 74.f, 16.f, 36.f, 50.f }, col::pink());

    const float split = 1.f + level * 5.f;
    juce::Font title(juce::FontOptions("Georgia", 38.f, juce::Font::bold));
    title = title.withExtraKerningFactor(0.18f);
    g.setFont(title);
    const auto tr = juce::Rectangle<int>(90, 8, getWidth() - 180, 52);
    g.setColour(col::pink().withAlpha(0.75f));
    g.drawText("THEE PSYCHICK MACHINE", tr.translated((int) -split, 0), juce::Justification::centred);
    g.setColour(col::crystal().withAlpha(0.55f));
    g.drawText("THEE PSYCHICK MACHINE", tr.translated((int) split, 0), juce::Justification::centred);
    g.setColour(col::bone());
    g.drawText("THEE PSYCHICK MACHINE", tr, juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(12.f)).withExtraKerningFactor(0.35f));
    g.setColour(col::pink());
    g.drawText("TEST CARD 23  //  SPECTRAL TRANSMUTATION DEVICE", 90, 62, getWidth() - 180, 18, juce::Justification::centred);

    g.setColour(col::bone().withAlpha(0.35f));
    g.drawHorizontalLine(100, 14.f, b.getRight() - 14.f);

    // --- panels
    for (auto& row : rows)
        for (auto& grp : row)
        {
            const auto r = grp.bounds.toFloat();
            g.setColour(juce::Colours::black.withAlpha(0.58f));
            g.fillRoundedRectangle(r, 4.f);
            g.setColour(grp.accent.withAlpha(0.45f));
            g.drawRoundedRectangle(r, 4.f, 1.f);

            // corner ticks
            g.setColour(grp.accent);
            const float t = 9.f;
            g.drawLine(r.getX(), r.getY() + t, r.getX(), r.getY(), 2.f);
            g.drawLine(r.getX(), r.getY(), r.getX() + t, r.getY(), 2.f);
            g.drawLine(r.getRight(), r.getBottom() - t, r.getRight(), r.getBottom(), 2.f);
            g.drawLine(r.getRight(), r.getBottom(), r.getRight() - t, r.getBottom(), 2.f);

            g.setFont(juce::Font(juce::FontOptions("Georgia", 15.f, juce::Font::bold)).withExtraKerningFactor(0.3f));
            g.setColour(col::bone());
            g.drawText(grp.title, grp.bounds.withHeight(20).withY(grp.bounds.getY() + 3), juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(10.f)).withExtraKerningFactor(0.2f));
            g.setColour(grp.accent.withAlpha(0.9f));
            g.drawText(grp.sub.toUpperCase(), grp.bounds.withHeight(12).withY(grp.bounds.getY() + 20), juce::Justification::centred);

            // small cross glyphs flanking the title
            const float cx0 = r.getX() + 12.f, cx1 = r.getRight() - 12.f - 7.f;
            drawPsychickCross(g, { cx0, r.getY() + 7.f, 7.f, 10.f }, grp.accent.withAlpha(0.8f));
            drawPsychickCross(g, { cx1, r.getY() + 7.f, 7.f, 10.f }, grp.accent.withAlpha(0.8f));

            if (grp.eye)
            {
                const auto er = juce::Rectangle<float>(r.getCentreX() - 70.f, r.getY() + 36.f, 140.f, r.getHeight() - 56.f)
                                    .withSizeKeepingCentre(120.f, 104.f);
                drawEyeInTriangle(g, er, col::pink().withAlpha(0.28f + 0.5f * level), level);
            }
        }

    // --- footer
    g.setFont(juce::Font(juce::FontOptions(10.5f)).withExtraKerningFactor(0.3f));
    g.setColour(col::bone().withAlpha(0.55f));
    g.drawText("E  23  -  PITCH * RING * FOLD * ACID * SWIRL * SCRY * SHATTER * ASTRAL",
               20, getHeight() - 24, getWidth() - 40, 16, juce::Justification::centredLeft);
    g.drawText("v1.0 - VST3", 20, getHeight() - 24, getWidth() - 40, 16, juce::Justification::centredRight);
}

void PsychickEditor::paintOverChildren(juce::Graphics& g)
{
    if (scanlines.isValid())
        g.drawImageAt(scanlines, 0, 0);

    // vignette
    const auto b = getLocalBounds().toFloat();
    juce::ColourGradient vg(juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                            juce::Colours::black.withAlpha(0.55f), 0.f, 0.f, true);
    g.setGradientFill(vg);
    g.fillAll();
}
