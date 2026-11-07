#pragma once
#include "PluginProcessor.h"
#include <vector>
class CX5MEditor final : public juce::AudioProcessorEditor, private juce::KeyListener {
public:
    explicit CX5MEditor(CX5MProcessor&);
    ~CX5MEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
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
    juce::ComboBox bendRange;
    juce::Label bendLabel;
    juce::Slider modDepth;
    juce::Label modLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modAttachment;
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
    juce::TextButton synthTab{"SYNTH"}, librarianTab{"LIBRARIAN"}, editorTab{"BANK EDITOR"};
    juce::StringArray availableBankNames;
    juce::Array<juce::File> availableBankFiles;
    bool librarianVisible = false;
    bool bankEditorVisible = false;
    bool synchronisingLibrarianSelection = false;
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
    void synchroniseLibrarianToLoadedVoice();
    // Bank editor keeps an independent 48 x 64-byte destination bank.
    // No changes to the live synth or its saved project state until the user imports an exported BIN.
    std::array<uint8_t, 3072> draftBank{};
    std::array<uint8_t, 3072> sourceBank{};
    juce::ComboBox sourceBankChoice, sourceVoiceChoice;
    juce::Label editorInstructions, editorStatus, destinationBankLabel;
    juce::String destinationBankName{"Unsaved custom bank"};
    enum class AuditionMode { stopped, source, slot };
    AuditionMode auditionMode = AuditionMode::stopped;
    void auditionSelectedSource();
    void auditionSelectedSlot();
    void updateAuditionButtons();
    void updateDestinationLabel();
    static bool isEmptyVoice(const uint8_t* record);
    juce::TextButton copyVoice{"COPY TO SELECTED SLOT"};
    juce::TextButton previousSource{"<"}, nextSource{">"}, renameSlot{"RENAME SLOT"};
    juce::TextButton auditionSource{"AUDITION SOURCE"}, auditionDraft{"AUDITION SLOT"}, stopAudition{"STOP AUDITION"};
    juce::TextButton swapUp{"MOVE UP"}, swapDown{"MOVE DOWN"};
    juce::TextButton loadDraft{"OPEN BANK .BIN"}, saveDraft{"SAVE BANK .BIN"};
    std::unique_ptr<juce::FileChooser> draftChooser;
    struct DraftListModel : juce::ListBoxModel {
        CX5MEditor& owner;
        explicit DraftListModel(CX5MEditor& e) : owner(e) {}
        int getNumRows() override { return 48; }
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void selectedRowsChanged(int row) override;
    } draftModel{*this};
    juce::ListBox draftList{"Destination bank", &draftModel};
    void scanEditorSources();
    void loadEditorSource(int row);
    void refreshSourceVoices();
    void refreshDraftList();
    static juce::String voiceName(const uint8_t* bytes);
    static bool readDecodedBank(const juce::File& file, std::array<uint8_t,3072>& dest, juce::String& error);
    void setEditorStatus(const juce::String& message);


};
