#pragma once
#include <JuceHeader.h>
#include <ymfm_opm.h>
#include <array>
#include <cstdint>
#include <atomic>
#include <memory>

// v0.3: real ymfm YM2151 chip core; editor parameter mappings remain experimental.
class CX5MProcessor final : public juce::AudioProcessor {
public:
    CX5MProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "CX5M Reborn"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Init OPM"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
    // Import a pre-decoded Yamaha 48 x 64-byte bank. WAV decoding is a separate step.
    bool importVoiceBank(const juce::File& file, juce::String& error);
    juce::String getImportedVoiceName(int index) const;
    bool hasImportedBank() const noexcept;
    void restoreBuiltInBank();
    void previewVoice(const uint8_t* record);
    void stopVoicePreview();
    void previewSilence();
    juce::String getBankDisplayName() const;

private:
    using BankData = std::array<uint8_t, 3072>;
    std::shared_ptr<const BankData> importedBank;
    std::shared_ptr<const BankData> previewBank;
    std::atomic<bool> silentPreview{false};
    juce::String importedBankName;
    std::shared_ptr<const BankData> currentAudioBank;
    std::atomic<unsigned> importRevision{0};
    unsigned seenImportRevision = 0;
    const BankData& bankBytes() const noexcept;
    struct Interface final : ymfm::ymfm_interface {};
    Interface interface;
    ymfm::ym2151 chip;
    struct Voice { int note = -1; bool released = false; uint64_t age = 0; float velocity = 1.f; int releaseSamples = 0; };
    std::array<Voice, 8> voices{};
    uint64_t counter = 0;
    int bend = 8192, modulation = 0;
    float lastModDepth = -1.f;
    double hostRate = 44100.0, nativeRate = 55930.0, fraction = 0.0;
    std::array<double, 2> prev{}, next{};
    std::array<int, 4> lastRatio{{-1,-1,-1,-1}}, lastLevel{{-1,-1,-1,-1}}, lastAttack{{-1,-1,-1,-1}}, lastRelease{{-1,-1,-1,-1}};
    int lastAlgorithm = -1;
    void write(uint8_t reg, uint8_t value);
    void setPitch(int channel, int note);
    void updateModulation();
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void updateParameters();
    void tick();
    void loadCassetteVoice(int index);
    int lastBank = -1;
    int activeBank = 0;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParams();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CX5MProcessor)
};
