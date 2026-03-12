#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectralWavefolderAudioProcessor::SpectralWavefolderAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
SpectralWavefolderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "drive", 1 }, "Drive",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.33f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "threshold", 1 }, "Threshold",
        juce::NormalisableRange<float>(0.05f, 1.0f, 0.001f, 1.0f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "mix", 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 1.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "tilt", 1 }, "Tilt",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dyntrack", 1 }, "Dynamics",
        juce::NormalisableRange<float>(0.0f, 0.7f, 0.001f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "outgain", 1 }, "Out Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "width", 1 }, "Stereo Width",
        juce::NormalisableRange<float>(-0.5f, 0.5f, 0.001f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "mode", 1 }, "Mode", 0, 6, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "delta", 1 }, "Delta", false));

    return layout;
}

void SpectralWavefolderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    fs = static_cast<float>(sampleRate);
    const int numCh = juce::jmax(1, getTotalNumOutputChannels());

    dcCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 5.0f / fs);
    rmsCoeff = std::exp(-1.0f / (fs * 0.020f));
    tiltG = std::tan(juce::MathConstants<float>::pi * 1000.0f / fs);

    ch.assign(static_cast<size_t>(numCh), ChannelState{});
    dryBuf.setSize(numCh, samplesPerBlock, false, true, true);

    const double ramp = 0.020;
    for (auto* s : { &smDrive, &smThresh, &smMix, &smTilt, &smDynTrack,
                     &smOutGain, &smWidth })
        s->reset(sampleRate, ramp);

    auto latch = [&](SV& sv, const char* id)
        { sv.setCurrentAndTargetValue(*parameters.getRawParameterValue(id)); };
    latch(smDrive, "drive");
    latch(smThresh, "threshold");
    latch(smMix, "mix");
    latch(smTilt, "tilt");
    latch(smDynTrack, "dyntrack");
    latch(smOutGain, "outgain");
    latch(smWidth, "width");
}

void SpectralWavefolderAudioProcessor::releaseResources() {}

bool SpectralWavefolderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;
    return true;
}

float SpectralWavefolderAudioProcessor::applyFoldMode(float x, float thr, int mode) noexcept
{
    if (thr < 1.0e-6f) return 0.0f;
    switch (mode)
    {
    case 0:
        return buchlaFold(x, thr);

    case 1:
    {
        float v = sergeCell(x, thr);
        v = iterFold(v, thr);
        v = sergeCell(v, thr);
        v = iterFold(v, thr);
        v = sergeCell(v, thr);
        v = iterFold(v, thr);
        v = sergeCell(v, thr);
        return iterFold(v, thr);
    }

    case 2:
        return juce::jlimit(-thr, thr, x);

    case 3:
        if (std::fabsf(x) <= thr) return 0.0f;
        return (x > 0.0f) ? (x - thr) : (x + thr);

    case 4:
    {
        float acc = 0.0f, t = thr;
        for (int st = 0; st < 5; ++st) { acc += iterFold(x, t); t *= 0.5f; }
        return acc * 0.2f;
    }

    case 5:
        return (x > thr) ? (2.0f * thr - x) : x;

    case 6: // Tri fold — gentle piecewise-linear fold, triangle-ish spectrum
    {
        float acc = 0.0f, t = thr;
        for (int st = 0; st < 3; ++st) { acc += iterFold(x, t); t *= 0.5f; }
        acc /= 3.0f;
        return 0.6f * x + 0.4f * acc;
    }

    default:
        return iterFold(x, thr);
    }
}

static inline void tpt1LP(float x, float g, float& s1,
    float& yLp, float& yHp) noexcept
{
    const float v = g * (x - s1) / (1.0f + g);
    yLp = v + s1;
    s1 += 2.0f * v;
    yHp = x - yLp;
}

void SpectralWavefolderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamp = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), totalOut);

    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear(i, 0, numSamp);
    if (numCh == 0 || numSamp == 0) return;

    smDrive.setTargetValue(*parameters.getRawParameterValue("drive"));
    smThresh.setTargetValue(*parameters.getRawParameterValue("threshold"));
    smMix.setTargetValue(*parameters.getRawParameterValue("mix"));
    smTilt.setTargetValue(*parameters.getRawParameterValue("tilt"));
    smDynTrack.setTargetValue(*parameters.getRawParameterValue("dyntrack"));
    smOutGain.setTargetValue(*parameters.getRawParameterValue("outgain"));
    smWidth.setTargetValue(*parameters.getRawParameterValue("width"));

    const int   mode = static_cast<int>  (*parameters.getRawParameterValue("mode"));
    const bool  deltaOn = *parameters.getRawParameterValue("delta") > 0.5f;
    const float drive0 = smDrive.getCurrentValue();
    const float thr0 = smThresh.getCurrentValue();
    const float dynTrk0 = smDynTrack.getCurrentValue();
    const float modeNorm = kModeRmsInv[juce::jlimit(0, 6, mode)] / juce::jmax(0.01f, drive0);
    const bool  stereo = (numCh == 2);

    if (dryBuf.getNumSamples() < numSamp || dryBuf.getNumChannels() < numCh)
        dryBuf.setSize(numCh, numSamp, false, true, true);

    // STEP 1 — capture dry
    for (int c = 0; c < numCh; ++c)
        dryBuf.copyFrom(c, 0, buffer, c, 0, numSamp);

    // STEP 2 — RMS for dynamics
    for (int c = 0; c < numCh; ++c)
    {
        auto& cs = ch[static_cast<size_t>(c)];
        const float* src = buffer.getReadPointer(c);
        for (int i = 0; i < numSamp; ++i)
            cs.rmsEnv = rmsCoeff * cs.rmsEnv + (1.0f - rmsCoeff) * (src[i] * src[i]);
    }

    auto effDrive = [&](size_t ci) -> float
        {
            const float amp = std::sqrtf(juce::jmin(ch[ci].rmsEnv, 1.0f));
            return drive0 * (1.0f + dynTrk0 * amp * 3.0f);
        };

    // STEP 3 — M/S encode
    if (stereo)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numSamp; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            const float s = (L[i] - R[i]) * 0.5f;
            L[i] = m; R[i] = s;
        }
    }

    // STEP 4 — wideband fold
    for (int c = 0; c < numCh; ++c)
    {
        float* data = buffer.getWritePointer(c);
        const float eff = effDrive(static_cast<size_t>(c));
        for (int i = 0; i < numSamp; ++i)
            data[i] = applyFoldMode(data[i] * eff, thr0, mode) * modeNorm;
    }

    // STEP 5 — tilt, DC block, soft-limit, output gain
    for (int c = 0; c < numCh; ++c)
    {
        float* data = buffer.getWritePointer(c);
        auto& cs = ch[static_cast<size_t>(c)];
        const bool isLead = (c == 0);
        for (int i = 0; i < numSamp; ++i)
        {
            const float tv = isLead ? smTilt.getNextValue() : smTilt.getCurrentValue();
            const float gLin = juce::Decibels::decibelsToGain(
                isLead ? smOutGain.getNextValue() : smOutGain.getCurrentValue());
            if (isLead)
            {
                smDrive.getNextValue(); smThresh.getNextValue();
                smMix.getNextValue();   smDynTrack.getNextValue();
                smWidth.getNextValue();
            }
            float out = data[i];
            float yLp, yHp;
            tpt1LP(out, tiltG, cs.tilt.s1, yLp, yHp);
            out = yLp * (1.0f - tv) + yHp * (1.0f + tv);
            const float dcY = dcCoeff * (cs.dcOut + out - cs.dcPrev);
            cs.dcPrev = out;
            cs.dcOut = dcY;
            out = dcY;
            data[i] = softLimit(out) * gLin;
        }
    }

    // STEP 6 — M/S decode + width
    if (stereo)
    {
        const float sGain = 1.0f + smWidth.getCurrentValue();
        float* M = buffer.getWritePointer(0);
        float* S = buffer.getWritePointer(1);
        for (int i = 0; i < numSamp; ++i)
        {
            const float m = M[i];
            const float s = S[i] * sGain;
            M[i] = m + s;
            S[i] = m - s;
        }
    }

    // STEP 7 — wet/dry blend (skipped when delta active)
    if (!deltaOn)
    {
        const float mv = smMix.getCurrentValue();
        const float dg = 1.0f - mv;
        for (int c = 0; c < numCh; ++c)
        {
            float* wet = buffer.getWritePointer(c);
            const float* dry = dryBuf.getReadPointer(c);
            for (int i = 0; i < numSamp; ++i)
                wet[i] = dg * dry[i] + mv * wet[i];
        }
    }
    else
    {
        // STEP 8 — delta: output only the difference (processed - dry)
        // buffer currently holds fully processed signal pre-blend
        for (int c = 0; c < numCh; ++c)
        {
            float* wet = buffer.getWritePointer(c);
            const float* dry = dryBuf.getReadPointer(c);
            for (int i = 0; i < numSamp; ++i)
                wet[i] -= dry[i];
        }
    }
}

void SpectralWavefolderAudioProcessor::getStateInformation(juce::MemoryBlock& d)
{
    if (auto x = parameters.copyState().createXml())
        copyXmlToBinary(*x, d);
}

void SpectralWavefolderAudioProcessor::setStateInformation(const void* d, int n)
{
    if (auto x = getXmlFromBinary(d, n))
        parameters.replaceState(juce::ValueTree::fromXml(*x));
}

juce::AudioProcessorEditor* SpectralWavefolderAudioProcessor::createEditor()
{
    return new SpectralWavefolderAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralWavefolderAudioProcessor();
}
