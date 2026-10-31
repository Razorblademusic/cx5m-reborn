#pragma once
#include <JuceHeader.h>
#include <array>

// Initial playable four-operator FM prototype.
// IMPORTANT: This is NOT yet YM2151 hardware emulation.
class CX5MProcessor final : public juce::AudioProcessor {
public:
    CX5MProcessor();
    void prepareToPlay(double sampleRate, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "CX5M Reborn"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init FM"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
private:
    struct Voice {
        int note = -1;
        double phase[4]{};
        double env[4]{};
        bool released = false;
        uint64_t age = 0;
        double velocity = 1.0;
    };
    std::array<Voice, 8> voices{};
    double sampleRateHz = 44100.0;
    uint64_t counter = 0;
    int bend = 8192;
    void noteOn(int note, float velocity);
    void noteOff(int note);
    float render(Voice& voice);
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParams();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CX5MProcessor)
};
