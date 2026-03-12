#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class SpectralWavefolderAudioProcessor;

//==============================================================================
namespace SWC
{
    static const juce::Colour BG{ 0xff141420 };
    static const juce::Colour PANEL{ 0xff1e1e2e };
    static const juce::Colour BORDER{ 0xff2a2a40 };
    static const juce::Colour ACCENT{ 0xff6c8ef5 };
    static const juce::Colour TEXT{ 0xffc8d0f0 };
    static const juce::Colour TEXTDIM{ 0xff6870a0 };
    static const juce::Colour KNOBTRACK{ 0xff303050 };
}

//==============================================================================
class SWLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SWLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, SWC::ACCENT);
        setColour(juce::Slider::rotarySliderOutlineColourId, SWC::KNOBTRACK);
        setColour(juce::Slider::thumbColourId, SWC::ACCENT);
        setColour(juce::Slider::textBoxTextColourId, SWC::TEXT);
        setColour(juce::Slider::textBoxBackgroundColourId, SWC::BG);
        setColour(juce::Slider::textBoxOutlineColourId, SWC::BORDER);
        setColour(juce::Label::textColourId, SWC::TEXT);
        setColour(juce::ComboBox::backgroundColourId, SWC::PANEL);
        setColour(juce::ComboBox::textColourId, SWC::TEXT);
        setColour(juce::ComboBox::outlineColourId, SWC::BORDER);
        setColour(juce::ComboBox::arrowColourId, SWC::ACCENT);
        setColour(juce::PopupMenu::backgroundColourId, SWC::PANEL);
        setColour(juce::PopupMenu::textColourId, SWC::TEXT);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, SWC::ACCENT.withAlpha(0.35f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::ToggleButton::textColourId, SWC::TEXT);
        setColour(juce::ToggleButton::tickColourId, SWC::ACCENT);
        setColour(juce::ToggleButton::tickDisabledColourId, SWC::TEXTDIM);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float startAng, float endAng,
        juce::Slider& s) override
    {
        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        const float r = juce::jmin(w, h) * 0.5f - 4.0f;
        if (r < 2.0f) return;
        const juce::Colour fillCol = s.findColour(juce::Slider::rotarySliderFillColourId);
        const juce::Colour trackCol = s.findColour(juce::Slider::rotarySliderOutlineColourId);
        juce::Path track;
        track.addCentredArc(cx, cy, r, r, 0.0f, startAng, endAng, true);
        g.setColour(trackCol);
        g.strokePath(track, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
        const float curAng = startAng + sliderPos * (endAng - startAng);
        juce::Path fill;
        fill.addCentredArc(cx, cy, r, r, 0.0f, startAng, curAng, true);
        g.setColour(fillCol);
        g.strokePath(fill, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
        const float px = cx + r * 0.68f * std::sin(curAng);
        const float py = cy - r * 0.68f * std::cos(curAng);
        g.setColour(fillCol.brighter(0.5f));
        g.drawLine(cx, cy, px, py, 2.0f);
        g.setColour(trackCol.brighter(0.3f));
        g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool,
        int, int, int, int, juce::ComboBox& box) override
    {
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(0.f, 0.f, (float)w, (float)h, 4.f);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(0.5f, 0.5f, w - 1.f, h - 1.f, 4.f, 1.f);
        const float ax = w - 16.f, ay = h * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(ax, ay - 3.f, ax + 6.f, ay - 3.f, ax + 3.f, ay + 3.f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.fillPath(arrow);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
        const juce::Colour&, bool, bool) override
    {
        const bool on = btn.getToggleState();
        g.setColour(on ? SWC::ACCENT.withAlpha(0.35f) : SWC::PANEL);
        g.fillRoundedRectangle(btn.getLocalBounds().toFloat().reduced(1.f), 4.f);
        g.setColour(on ? SWC::ACCENT : SWC::BORDER);
        g.drawRoundedRectangle(btn.getLocalBounds().toFloat().reduced(1.f), 4.f, 1.f);
    }
};

//==============================================================================
class SectionPanel : public juce::Component
{
public:
    juce::String title;
    juce::Colour accentCol;

    SectionPanel(const juce::String& t, juce::Colour ac = SWC::ACCENT)
        : title(t), accentCol(ac)
    {
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(SWC::PANEL);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
        g.setColour(SWC::BORDER);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.f, 1.f);
        g.setColour(accentCol.withAlpha(0.7f));
        g.fillRoundedRectangle(4.f, 0.f, getWidth() - 8.f, 3.f, 1.5f);
        g.setColour(SWC::TEXTDIM);
        g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
        g.drawText(title.toUpperCase(), 8, 5, getWidth() - 16, 14, juce::Justification::left);
    }
};

//==============================================================================
class LabelledKnob
{
public:
    juce::Slider slider;
    juce::Label  label;

    void init(juce::Component* parent, juce::LookAndFeel& laf,
        const juce::String& labelText, juce::Colour accentCol = SWC::ACCENT)
    {
        slider.setLookAndFeel(&laf);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 15);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentCol);
        parent->addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(juce::FontOptions().withHeight(10.5f)));
        label.setColour(juce::Label::textColourId, SWC::TEXT);
        parent->addAndMakeVisible(label);
    }

    void setBounds(juce::Rectangle<int> area)
    {
        label.setBounds(area.removeFromTop(14));
        slider.setBounds(area);
    }
};

//==============================================================================
class SpectralWavefolderAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SpectralWavefolderAudioProcessorEditor(SpectralWavefolderAudioProcessor&);
    ~SpectralWavefolderAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SpectralWavefolderAudioProcessor& processor;
    std::unique_ptr<SWLookAndFeel>    laf;

    SectionPanel panelFold{ "Wavefold" };
    SectionPanel panelOutput{ "Output" };

    LabelledKnob drive, threshold, mix, tilt, dynTrack;
    LabelledKnob outGain, width;

    juce::ComboBox modeBox;
    juce::Label    modeLabel;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> attDrive, attThresh, attMix, attTilt, attDyn;
    std::unique_ptr<SA> attOutGain, attWidth;
    std::unique_ptr<CA> attMode;
    juce::ToggleButton deltaBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attDelta;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralWavefolderAudioProcessorEditor)
};
