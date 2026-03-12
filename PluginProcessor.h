#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

class SpectralWavefolderAudioProcessor final : public juce::AudioProcessor
{
public:
    SpectralWavefolderAudioProcessor();
    ~SpectralWavefolderAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static inline float iterFold(float x, float thr) noexcept
    {
        if (thr < 1.0e-6f) return 0.0f;
        for (int n = 0; n < 24; ++n)
        {
            if (x > thr) x = 2.0f * thr - x;
            else if (x < -thr) x = -2.0f * thr - x;
            else break;
        }
        return x;
    }

    static inline float buchlaFold(float x, float thr) noexcept
    {
        if (thr < 1.0e-6f) return 0.0f;
        float sum = 0.0f;
        for (int k = 1; k <= 6; ++k)
        {
            const float Vk = thr * static_cast<float>(k) / 6.0f;
            sum += juce::jlimit(-Vk, Vk, x);
        }
        return (sum / 6.0f + x * 0.25f) * (1.0f / 1.25f);
    }

    static inline float sergeCell(float x, float thr) noexcept
    {
        if (thr < 1.0e-6f) return 0.0f;
        return x - 2.0f * thr * std::tanh(x / thr);
    }

    static float applyFoldMode(float x, float thr, int mode) noexcept;

    static inline float softLimit(float x) noexcept
    {
        x = juce::jlimit(-4.0f, 4.0f, x);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    struct TiltState { float s1 = 0.0f; };

    struct ChannelState
    {
        float dcPrev{ 0.0f };
        float dcOut{ 0.0f };
        TiltState tilt;
        float rmsEnv{ 0.0f };
        float deltaPrev{ 0.0f };   // for delta output mode
    };
    std::vector<ChannelState> ch;

    juce::AudioBuffer<float> dryBuf;

    using SV = juce::SmoothedValue<float>;
    SV smDrive, smThresh, smMix, smTilt, smDynTrack;
    SV smOutGain, smWidth;

    float dcCoeff{ 0.995f };
    float rmsCoeff{ 0.999f };
    float tiltG{ 0.0f };
    float fs{ 44100.0f };

    // mode 6 (tri fold) shares Buchla's RMS estimate
    static constexpr float kModeRmsInv[7] = { 1.41f, 0.85f, 1.00f, 2.45f, 1.73f, 1.15f, 1.41f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralWavefolderAudioProcessor)
};
