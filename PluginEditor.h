#pragma once

#include <JuceHeader.h>

class SpectralWavefolderAudioProcessor;

class SpectralWavefolderAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SpectralWavefolderAudioProcessorEditor (SpectralWavefolderAudioProcessor&);
    ~SpectralWavefolderAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SpectralWavefolderAudioProcessor& processor;

    juce::Slider drive;
    juce::Slider threshold;
    juce::Slider mix;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    juce::ComboBox oversampleBox;
    juce::Label oversampleLabel;
    // ... keep existing members

    juce::Label driveLabel;
    juce::Label thresholdLabel;
    juce::Label mixLabel;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> driveAttachment;
    std::unique_ptr<Attachment> thresholdAttachment;
    std::unique_ptr<Attachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversampleAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralWavefolderAudioProcessorEditor)
};

