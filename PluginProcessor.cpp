#include "PluginProcessor.h"
#include "PluginEditor.h"

SpectralWavefolderAudioProcessor::SpectralWavefolderAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SpectralWavefolderAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Drive: 0.1 to 10.0, default 0.5, skewed toward lower values
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drive", 1),
        "Drive",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f),
        0.5f));

    // Threshold: 0.1 to 1.0, default 0.5, controls fold boundary
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("threshold", 1),
        "Threshold",
        juce::NormalisableRange<float>(0.1f, 1.0f, 0.001f, 1.0f),
        0.5f));

    // Mix: 0.0 to 1.0
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1),
        "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f, 1.0f),
        1.0f));

    // Mode selection
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("mode", 1),
        "Mode",
        0, 5, 0));

    // Oversampling: 0 = none, 1 = 2x, 2 = 4x
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("oversample", 1),
        "Oversample",
        0, 2, 0));

    return layout;
}

void SpectralWavefolderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    // initialize per-channel DC filters
    dcFilters.assign((int)getTotalNumOutputChannels(), 0.0f);
    // initialize oversampling object lazily in processBlock when needed
    oversampling.reset();
}

bool SpectralWavefolderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != mainOut)
        return false;
    return true;
}

float SpectralWavefolderAudioProcessor::foldSample(float x, float thr) noexcept
{
    if (thr <= 0.0f) return x;

    // Fold non-negative magnitudes into [0, thr] using a triangular fold
    // period = 2 * thr, fold back into range
    const float period = 2.0f * thr;
    float y = std::fmod(x, period);
    if (y < 0.0f) y += period;

    if (y <= thr)
        return y;
    else
        return 2.0f * thr - y;
}

void SpectralWavefolderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const float drive = *parameters.getRawParameterValue("drive");
    const float threshold = *parameters.getRawParameterValue("threshold");
    const int mode = int(*parameters.getRawParameterValue("mode"));
    const float mix = *parameters.getRawParameterValue("mix");

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // check oversample parameter (0 = none, 1 = 2x, 2 = 4x)
    const int overs = int(*parameters.getRawParameterValue("oversample"));
    const int factor = (overs == 1) ? 2 : (overs == 2) ? 4 : 1;

    // keep a dry copy for mixing
    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer);

    if (factor <= 1)
    {
        // no oversampling: process in-place
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            float& dcFilter = dcFilters[size_t(ch)];

            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                x *= drive;

                float y = x;

                // folding modes (same as before)
                switch (mode)
                {
                    case 0: // Buchla-style parallel
                    {
                        const int stages = 3;
                        float acc = 0.0f;
                        float s = 1.0f;
                        for (int st = 0; st < stages; ++st)
                        {
                            const float thr = threshold * s;
                            float folded = x;
                            if (thr > 0.0f)
                            {
                                float pos = std::fmod(folded + thr, 2.0f * thr);
                                if (pos < 0.0f) pos += 2.0f * thr;
                                if (pos <= thr) folded = pos - thr;
                                else folded = 3.0f * thr - pos;
                            }
                            acc += folded;
                            s *= 2.0f;
                        }
                        y = acc / float(stages);
                        break;
                    }
                    case 1: // Serge-style series
                    {
                        const int stages = 4;
                        float v = x;
                        for (int st = 0; st < stages; ++st)
                        {
                            if (threshold > 0.0f)
                            {
                                float pos = std::fmod(v + threshold, 2.0f * threshold);
                                if (pos < 0.0f) pos += 2.0f * threshold;
                                if (pos <= threshold) v = pos - threshold;
                                else v = 3.0f * threshold - pos;
                            }
                        }
                        y = v;
                        break;
                    }
                    case 2: // Peak-clipping
                        y = juce::jlimit(-threshold, threshold, x);
                        break;
                    case 3: // Center-clipping
                    {
                        float off = std::fabs(x);
                        y = (off > threshold) ? x : -x;
                        break;
                    }
                    case 4: // Fractal folding
                    {
                        float v = x;
                        float s = 1.0f;
                        for (int st = 0; st < 5; ++st)
                        {
                            float thr = threshold * s;
                            if (thr > 0.0f)
                            {
                                float pos = std::fmod(v + thr, 2.0f * thr);
                                if (pos < 0.0f) pos += 2.0f * thr;
                                if (pos <= thr) v = pos - thr;
                                else v = 3.0f * thr - pos;
                            }
                            s *= 0.5f;
                        }
                        y = v;
                        break;
                    }
                    case 5: // Wavewrapper
                    {
                        if (threshold > 0.0f)
                        {
                            float m = std::fmod(x + threshold, 2.0f * threshold);
                            if (m < 0.0f) m += 2.0f * threshold;
                            y = m - threshold;
                        }
                        else y = x;
                        break;
                    }
                    default:
                    {
                        if (threshold > 0.0f)
                        {
                            float pos = std::fmod(x + threshold, 2.0f * threshold);
                            if (pos < 0.0f) pos += 2.0f * threshold;
                            if (pos <= threshold) y = pos - threshold;
                            else y = 3.0f * threshold - pos;
                        }
                    }
                }

                float out = (1.0f - mix) * dry.getSample(ch, i) + mix * y;

                const float dcCoeff = 0.995f;
                dcFilter = dcCoeff * dcFilter + out;
                out -= dcFilter * (1.0f - dcCoeff);

                data[i] = out;
            }
        }
    }
    else
    {
        // oversampling path: simple linear upsampling + fold processing + box downsampling
        const int upSamples = numSamples * factor;
        juce::AudioBuffer<float> upBuf(numChannels, upSamples);

        // upsample (linear interp)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* in = dry.getReadPointer(ch);
            float* up = upBuf.getWritePointer(ch);

            for (int i = 0; i < numSamples - 1; ++i)
            {
                const float a = in[i];
                const float b = in[i + 1];
                for (int k = 0; k < factor; ++k)
                {
                    float t = float(k) / float(factor);
                    up[i * factor + k] = a * (1.0f - t) + b * t;
                }
            }
            // last sample: hold
            const float last = in[numSamples - 1];
            for (int k = 0; k < factor; ++k)
                up[(numSamples - 1) * factor + k] = last;
        }

        // process upsampled buffer with same folding algorithm
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* up = upBuf.getWritePointer(ch);

            for (int i = 0; i < upSamples; ++i)
            {
                float x = up[i];
                x *= drive;

                float y = x;
                switch (mode)
                {
                    case 0:
                    {
                        const int stages = 3;
                        float acc = 0.0f;
                        float s = 1.0f;
                        for (int st = 0; st < stages; ++st)
                        {
                            const float thr = threshold * s;
                            float folded = x;
                            if (thr > 0.0f)
                            {
                                float pos = std::fmod(folded + thr, 2.0f * thr);
                                if (pos < 0.0f) pos += 2.0f * thr;
                                if (pos <= thr) folded = pos - thr;
                                else folded = 3.0f * thr - pos;
                            }
                            acc += folded;
                            s *= 2.0f;
                        }
                        y = acc / float(stages);
                        break;
                    }
                    case 1:
                    {
                        const int stages = 4;
                        float v = x;
                        for (int st = 0; st < stages; ++st)
                        {
                            if (threshold > 0.0f)
                            {
                                float pos = std::fmod(v + threshold, 2.0f * threshold);
                                if (pos < 0.0f) pos += 2.0f * threshold;
                                if (pos <= threshold) v = pos - threshold;
                                else v = 3.0f * threshold - pos;
                            }
                        }
                        y = v;
                        break;
                    }
                    case 2:
                        y = juce::jlimit(-threshold, threshold, x);
                        break;
                    case 3:
                    {
                        if (std::fabs(x) < threshold)
                            y = 0.0f;
                        else
                            y = x;
                        break;
                    }
                    case 4:
                    {
                        float v = x;
                        float s = 1.0f;
                        for (int st = 0; st < 5; ++st)
                        {
                            float thr = threshold * s;
                            if (thr > 0.0f)
                            {
                                float pos = std::fmod(v + thr, 2.0f * thr);
                                if (pos < 0.0f) pos += 2.0f * thr;
                                if (pos <= thr) v = pos - thr;
                                else v = 3.0f * thr - pos;
                            }
                            s *= 0.5f;
                        }
                        y = v;
                        break;
                    }
                    case 5:
                    {
                        if (threshold > 0.0f)
                        {
                            float m = std::fmod(x + threshold, 2.0f * threshold);
                            if (m < 0.0f) m += 2.0f * threshold;
                            y = m - threshold;
                        }
                        else y = x;
                        break;
                    }
                    default:
                    {
                        if (threshold > 0.0f)
                        {
                            float pos = std::fmod(x + threshold, 2.0f * threshold);
                            if (pos < 0.0f) pos += 2.0f * threshold;
                            if (pos <= threshold) y = pos - threshold;
                            else y = 3.0f * threshold - pos;
                        }
                    }
                }
                up[i] = y;
            }
        }

        // downsample by simple averaging (box filter)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* out = buffer.getWritePointer(ch);
            const float* up = upBuf.getReadPointer(ch);
            float& dcFilter = dcFilters[size_t(ch)];

            for (int i = 0; i < numSamples; ++i)
            {
                float sum = 0.0f;
                for (int k = 0; k < factor; ++k)
                    sum += up[i * factor + k];
                float proc = sum / float(factor);

                float outv = (1.0f - mix) * dry.getSample(ch, i) + mix * proc;

                const float dcCoeff = 0.995f;
                dcFilter = dcCoeff * dcFilter + outv;
                outv -= dcFilter * (1.0f - dcCoeff);

                out[i] = outv;
            }
        }
    }
}

void SpectralWavefolderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SpectralWavefolderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* SpectralWavefolderAudioProcessor::createEditor()
{
    return new SpectralWavefolderAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralWavefolderAudioProcessor();
}
