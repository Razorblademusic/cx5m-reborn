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
    juce::TextButton previousPatch{"<"}, nextPatch{">"};
    juce::Label bankStatus;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bankAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment;
    juce::TextButton cassette{"LOAD DECODED BANK (.BIN)"};
    juce::TextButton settings{"SETTINGS"};
    juce::Label folderStatus;
    std::unique_ptr<juce::PropertiesFile> preferences;
    std::unique_ptr<juce::FileChooser> folderChooser;
    juce::File defaultBankFolder() const;
    void refreshFolderStatus();
    std::unique_ptr<juce::FileChooser> chooser;
    void refreshBankNames();
    void addControl(const juce::String& id, const juce::String& label);
    juce::TextButton synthTab{"SYNTH"}, librarianTab{"LIBRARIAN"};
    juce::StringArray availableBankNames;
    juce::Array<juce::File> availableBankFiles;
    bool librarianVisible = false;
    struct BankListModel : juce::ListBoxModel {
        CX5MEditor& owner;
        explicit BankListModel(CX5MEditor& e) : owner(e) {}
        int getNumRows() override { return owner.availableBankNames.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void selectedRowsChanged(int row) override;
    } bankModel{*this};
    struct VoiceListModel : juce::ListBoxModel {
        CX5MEditor& owner;
        explicit VoiceListModel(CX5MEditor& e) : owner(e) {}
        int getNumRows() override { return 48; }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void selectedRowsChanged(int row) override;
    } voiceModel{*this};
    // Models must be constructed before the ListBoxes that reference them.
    juce::ListBox bankList{"Available banks", &bankModel};
    juce::ListBox voiceList{"Voices", &voiceModel};
    juce::TextButton rescanBanks{"REFRESH BANKS"};
    juce::Label librarianInfo;
    void scanBankFolder();
    void showTab(bool librarian);
    void selectSourceBank(int row);

};
