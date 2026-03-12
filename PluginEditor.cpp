#include "PluginEditor.h"
#include "PluginProcessor.h"

static void configureSlider (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
}

SpectralWavefolderAudioProcessorEditor::SpectralWavefolderAudioProcessorEditor (SpectralWavefolderAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    configureSlider (drive);
    configureSlider (threshold);
    configureSlider (mix);

    modeBox.addItem("Buchla-style (parallel)", 1);
    modeBox.addItem("Serge-style (series)", 2);
    modeBox.addItem("Peak-clipping", 3);
    modeBox.addItem("Center-clipping", 4);
    modeBox.addItem("Fractal folding", 5);
    modeBox.addItem("Wavewrapper", 6);

    oversampleBox.addItem("None", 1);
    oversampleBox.addItem("2x", 2);
    oversampleBox.addItem("4x", 3);

    oversampleLabel.setText("Oversample", juce::dontSendNotification);

    modeLabel.setText("Mode", juce::dontSendNotification);

    driveLabel.setText ("Drive", juce::dontSendNotification);
    thresholdLabel.setText ("Threshold", juce::dontSendNotification);
    mixLabel.setText ("Mix", juce::dontSendNotification);

    for (auto* l : { &driveLabel, &thresholdLabel, &mixLabel })
        l->setJustificationType (juce::Justification::centred);

    addAndMakeVisible (drive);
    addAndMakeVisible (threshold);
    addAndMakeVisible (mix);
    addAndMakeVisible (modeBox);
    addAndMakeVisible (oversampleBox);

    addAndMakeVisible (driveLabel);
    addAndMakeVisible (thresholdLabel);
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (modeLabel);
    addAndMakeVisible (oversampleLabel);

    driveAttachment     = std::make_unique<Attachment> (processor.parameters, "drive", drive);
    thresholdAttachment = std::make_unique<Attachment> (processor.parameters, "threshold", threshold);
    mixAttachment       = std::make_unique<Attachment> (processor.parameters, "mix", mix);
    modeAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "mode", modeBox);
    oversampleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "oversample", oversampleBox);

    setSize (480, 220);
}

void SpectralWavefolderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Spectral Wavefolder", getLocalBounds().removeFromTop (26), juce::Justification::centred, 1);
}

void SpectralWavefolderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (28);

    auto row = area.removeFromTop (area.getHeight());
    auto w = row.getWidth() / 4;

    auto c1 = row.removeFromLeft (w);
    auto c2 = row.removeFromLeft (w);
    auto c3 = row.removeFromLeft (w);
    auto c4 = row;

    auto place = [] (juce::Rectangle<int> r, juce::Slider& s, juce::Label& l)
    {
        l.setBounds (r.removeFromTop (20));
        s.setBounds (r.reduced (6));
    };

    place (c1, drive, driveLabel);
    place (c2, threshold, thresholdLabel);
    place (c3, mix, mixLabel);
    // place mode combobox and oversample combobox under their labels manually
    auto top = c4.removeFromTop (c4.getHeight() / 2);
    modeLabel.setBounds (top.removeFromTop (20));
    modeBox.setBounds (top.reduced (6));

    auto bot = c4;
    oversampleLabel.setBounds (bot.removeFromTop (20));
    oversampleBox.setBounds (bot.reduced (6));
}

