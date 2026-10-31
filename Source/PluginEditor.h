#pragma once
#include "PluginProcessor.h"
#include <vector>
class CX5MEditor final : public juce::AudioProcessorEditor {
public:
    explicit CX5MEditor(CX5MProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    CX5MProcessor& processor;
    struct Control {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<Control> controls;
    juce::ComboBox algorithm;
    juce::ComboBox bank;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bankAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment;
    juce::TextButton cassette{"LOAD DECODED BANK (.BIN)"};
    std::unique_ptr<juce::FileChooser> chooser;
    void refreshBankNames();
    void addControl(const juce::String& id, const juce::String& label);
};
