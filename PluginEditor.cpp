#include "PluginEditor.h"
#include "PluginProcessor.h"

SpectralWavefolderAudioProcessorEditor::SpectralWavefolderAudioProcessorEditor(
    SpectralWavefolderAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p),
    laf(std::make_unique<SWLookAndFeel>())
{
    setLookAndFeel(laf.get());
    setResizable(true, false);
    setResizeLimits(480, 200, 1400, 600);

    addAndMakeVisible(panelFold);
    addAndMakeVisible(panelOutput);

    drive.init(this, *laf, "Drive");
    threshold.init(this, *laf, "Threshold");
    mix.init(this, *laf, "Mix");
    tilt.init(this, *laf, "Tilt");
    dynTrack.init(this, *laf, "Dynamics");
    outGain.init(this, *laf, "Out Gain");
    width.init(this, *laf, "Stereo Width");

    modeLabel.setText("MODE", juce::dontSendNotification);
    modeLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    modeLabel.setColour(juce::Label::textColourId, SWC::TEXTDIM);
    modeBox.setLookAndFeel(laf.get());
    modeBox.addItem("Buchla (parallel)", 1);
    modeBox.addItem("Serge (series)", 2);
    modeBox.addItem("Peak clip", 3);
    modeBox.addItem("Center fold", 4);   // renamed, behaviour unchanged
    modeBox.addItem("Fractal fold", 5);
    modeBox.addItem("Wavewrap", 6);
    modeBox.addItem("Tri fold", 7);   // new mode 6 (0-indexed)
    addAndMakeVisible(modeLabel);
    addAndMakeVisible(modeBox);

    // Delta toggle button
    deltaBtn.setButtonText("Delta");
    deltaBtn.setLookAndFeel(laf.get());
    addAndMakeVisible(deltaBtn);

    auto& ap = processor.parameters;
    attDrive = std::make_unique<SA>(ap, "drive", drive.slider);
    attThresh = std::make_unique<SA>(ap, "threshold", threshold.slider);
    attMix = std::make_unique<SA>(ap, "mix", mix.slider);
    attTilt = std::make_unique<SA>(ap, "tilt", tilt.slider);
    attDyn = std::make_unique<SA>(ap, "dyntrack", dynTrack.slider);
    attOutGain = std::make_unique<SA>(ap, "outgain", outGain.slider);
    attWidth = std::make_unique<SA>(ap, "width", width.slider);
    attMode = std::make_unique<CA>(ap, "mode", modeBox);
    attDelta = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(ap, "delta", deltaBtn);

    setSize(620, 220);
}

SpectralWavefolderAudioProcessorEditor::~SpectralWavefolderAudioProcessorEditor()
{
    attDrive.reset(); attThresh.reset(); attMix.reset();
    attTilt.reset();  attDyn.reset();    attOutGain.reset();
    attWidth.reset(); attMode.reset();   attDelta.reset();
    setLookAndFeel(nullptr);
}

void SpectralWavefolderAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(SWC::BG);
    g.setColour(SWC::PANEL);
    g.fillRect(0, 0, getWidth(), 30);
    g.setColour(SWC::ACCENT.withAlpha(0.6f));
    g.fillRect(0, 28, getWidth(), 2);
    g.setColour(SWC::TEXT);
    g.setFont(juce::Font(juce::FontOptions().withHeight(13.f).withStyle("Bold")));
    g.drawText("SPECTRAL WAVEFOLDER", 12, 0, 300, 30, juce::Justification::centredLeft);
    g.setColour(SWC::TEXTDIM);
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f)));
    g.drawText("Lazirko Records", getWidth() - 110, 0, 102, 30,
        juce::Justification::centredRight);
}

void SpectralWavefolderAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(32);
    area.reduce(6, 6);

    //--- Right: output panel ----------------------------------------------------
    auto outCol = area.removeFromRight(140);
    panelOutput.setBounds(outCol);
    {
        auto inner = outCol.reduced(8, 18);
        modeLabel.setBounds(inner.removeFromTop(13));
        modeBox.setBounds(inner.removeFromTop(22).reduced(0, 1));
        inner.removeFromTop(4);
        deltaBtn.setBounds(inner.removeFromTop(20));   // Delta checkbox
        inner.removeFromTop(4);
        const int halfH = inner.getHeight() / 2;
        outGain.setBounds(inner.removeFromTop(halfH));
        width.setBounds(inner);
    }
    area.removeFromRight(4);

    //--- Left/centre: fold panel ------------------------------------------------
    panelFold.setBounds(area);
    {
        auto inner = area.reduced(8, 18);
        const int w = inner.getWidth() / 5;
        drive.setBounds(inner.removeFromLeft(w));
        threshold.setBounds(inner.removeFromLeft(w));
        mix.setBounds(inner.removeFromLeft(w));
        tilt.setBounds(inner.removeFromLeft(w));
        dynTrack.setBounds(inner);
    }
}
