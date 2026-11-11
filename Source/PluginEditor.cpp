#include "PluginEditor.h"
#include "CassetteBank.h"
CX5MEditor::CX5MEditor(CX5MProcessor& p) : AudioProcessorEditor(&p), processor(p) {
    for (int i = 1; i <= 4; ++i) {
        auto s = juce::String(i);
        addControl("ratio" + s, "RATIO"); addControl("level" + s, "LEVEL");
        addControl("attack" + s, "ATTACK"); addControl("release" + s, "RELEASE");
    }
    addControl("output", "OUTPUT");
    algorithm.addItemList({"Algorithm 1", "Algorithm 2", "Algorithm 3", "Algorithm 4", "Algorithm 5", "Algorithm 6", "Algorithm 7", "Algorithm 8"}, 1);
    addAndMakeVisible(algorithm);
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state, "algorithm", algorithm);
    bank.addItem("Prototype (v0.3)",1);
    for(int i=0;i<48;++i) {
        juce::String name;
        // Name from the original 7-byte cassette record header.
        const auto& r = cassetteBank::raw;
        for(int j=0;j<7;++j) { auto c=r[static_cast<size_t>(i*64+j)]; if(c>=32 && c<=126) name += juce::String::charToString(static_cast<juce_wchar>(c)); }
        bank.addItem(juce::String(i+1).paddedLeft('0',2)+"  "+name.trim(),i+2);
    }
    addAndMakeVisible(bank);
    bankAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,"bank",bank);
    setWantsKeyboardFocus(true);
    addKeyListener(this);
    bank.addKeyListener(this);
    previousPatch.addKeyListener(this);
    nextPatch.addKeyListener(this);
    bendRange.addItemList({"±2", "±5", "±7", "±12"}, 1);
    addAndMakeVisible(bendRange);
    bendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state, "bendRange", bendRange);
    bendLabel.setText("BEND", juce::dontSendNotification);
    bendLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bendLabel);
    modDepth.setSliderStyle(juce::Slider::LinearHorizontal);
    modDepth.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
    addAndMakeVisible(modDepth);
    modAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "modDepth", modDepth);
    modLabel.setText("MOD DEPTH", juce::dontSendNotification);
    modLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modLabel);
    addAndMakeVisible(previousPatch);
    addAndMakeVisible(nextPatch);
    previousPatch.onClick = [this] {
        const int current = juce::jlimit(2, 49, bank.getSelectedId());
        bank.setSelectedId(current == 2 ? 49 : current - 1, juce::sendNotificationSync);
    };
    nextPatch.onClick = [this] {
        const int current = juce::jlimit(1, 49, bank.getSelectedId());
        bank.setSelectedId(current >= 49 ? 2 : current + 1, juce::sendNotificationSync);
    };
    bankStatus.setColour(juce::Label::textColourId, juce::Colour(0xff80c8dc));
    bankStatus.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bankStatus);
    refreshBankNames();
    juce::PropertiesFile::Options options;
    options.applicationName = "CX5MReborn";
    options.filenameSuffix = "settings";
    options.folderName = "CX5MReborn";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    preferences = std::make_unique<juce::PropertiesFile>(options);

    addAndMakeVisible(settings);
    settings.onClick = [this] {
        folderChooser = std::make_unique<juce::FileChooser>(
            "Choose your default CX5M bank folder", defaultBankFolder());
        juce::Component::SafePointer<CX5MEditor> safeThis(this);
        folderChooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectDirectories,
            [safeThis](const juce::FileChooser& fc) {
                if (safeThis == nullptr) return;
                const auto folder = fc.getResult();
                if (!folder.isDirectory()) return;
                safeThis->preferences->setValue("defaultBankFolder", folder.getFullPathName());
                safeThis->preferences->saveIfNeeded();
                safeThis->refreshFolderStatus();
            });
    };
    folderStatus.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    folderStatus.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(folderStatus);
    refreshFolderStatus();

    addAndMakeVisible(cassette);
    cassette.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>(
            "Choose a decoded 48-voice CX5M bank", defaultBankFolder(), "*.bin");
        juce::Component::SafePointer<CX5MEditor> safeThis(this);
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& fc) {
                if (safeThis == nullptr) return;
                const auto file = fc.getResult();
                if (!file.existsAsFile()) return;
                juce::String error;
                if (!safeThis->processor.importVoiceBank(file, error)) {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Bank import failed", error);
                } else {
                    safeThis->refreshBankNames();
                }
            });
    };
    addAndMakeVisible(synthTab);
    addAndMakeVisible(librarianTab);
    addAndMakeVisible(editorTab);
    synthTab.onClick = [this] { processor.stopVoicePreview(); bankEditorVisible = false; showTab(false); };
    librarianTab.onClick = [this] { processor.stopVoicePreview(); bankEditorVisible = false; showTab(true); scanBankFolder(); };
    editorTab.onClick = [this] { bankEditorVisible = true; showTab(false); scanEditorSources(); };
    addAndMakeVisible(bankList);
    addAndMakeVisible(voiceList);
    addAndMakeVisible(rescanBanks);
    addAndMakeVisible(librarianInfo);
    librarianInfo.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    librarianInfo.setText("Select a bank to load, then a voice to audition with MIDI.", juce::dontSendNotification);
    rescanBanks.onClick = [this] { scanBankFolder(); };
    // Draft starts as a valid, fully populated bank; source is independent of the live synth.
    draftBank = cassetteBank::raw;
    sourceBank = cassetteBank::raw;
    addAndMakeVisible(sourceBankChoice);
    addAndMakeVisible(sourceList);
    sourceList.setWantsKeyboardFocus(true);
    sourceList.addKeyListener(this);
    previousSource.addKeyListener(this);
    nextSource.addKeyListener(this);
    addAndMakeVisible(previousSource);
    addAndMakeVisible(nextSource);
    previousSource.onClick = [this] { stepSourceSelection(-1); };
    nextSource.onClick = [this] { stepSourceSelection(1); };
    addAndMakeVisible(renameSlot);
    renameSlot.onClick = [this] {
        const int row = draftList.getSelectedRow();
        if (!juce::isPositiveAndBelow(row, 48)) { setEditorStatus("Select a destination slot to rename."); return; }
        auto* dialog = new juce::AlertWindow("Rename destination voice", "CX5M voice names contain up to 7 ASCII characters.", juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("voiceName", voiceName(draftBank.data() + row * 64), "Voice name:");
        dialog->addButton("RENAME", 1, juce::KeyPress(juce::KeyPress::returnKey));
        dialog->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<CX5MEditor> safe(this);
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe, dialog, row](int result) {
            if (safe != nullptr && result == 1) {
                const auto input = dialog->getTextEditorContents("voiceName");
                bool valid = input.isNotEmpty() && input.length() <= 7;
                for (int i = 0; i < input.length(); ++i) if (input[i] < 32 || input[i] > 126) valid = false;
                if (!valid) { safe->setEditorStatus("Name must be 1-7 printable ASCII characters."); }
                else {
                    auto* record = safe->draftBank.data() + row * 64;
                    for (int i = 0; i < 7; ++i) record[i] = static_cast<uint8_t>(i < input.length() ? input[i] : ' ');
                    safe->markDraftDirty();
                    safe->refreshDraftList();
                    if (safe->auditionMode == AuditionMode::slot) safe->auditionSelectedSlot();
                    safe->setEditorStatus("Renamed destination slot " + juce::String(row + 1) + " to " + input + ". Save BIN to keep changes.");
                }
            }
            delete dialog;
        }), false);
    };
    addAndMakeVisible(destinationBankLabel);
    destinationBankLabel.setColour(juce::Label::textColourId, juce::Colour(0xff80c8dc));
    destinationBankLabel.setJustificationType(juce::Justification::centredLeft);
    updateDestinationLabel();
    addAndMakeVisible(draftList);
    addAndMakeVisible(copyVoice);
    addAndMakeVisible(swapUp);
    addAndMakeVisible(swapDown);
    addAndMakeVisible(loadDraft);
    addAndMakeVisible(newDraft);
    addAndMakeVisible(saveDraft);
    addAndMakeVisible(editorInstructions);
    addAndMakeVisible(editorStatus);
    editorInstructions.setText("SOURCE BANK (48 VOICES)                              DESTINATION BANK (48 VOICES)", juce::dontSendNotification);
    editorInstructions.setColour(juce::Label::textColourId, juce::Colour(0xff80c8dc));
    editorStatus.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    editorStatus.setText("Select a source voice and a destination slot, then COPY.", juce::dontSendNotification);
    sourceBankChoice.onChange = [this] { loadEditorSource(sourceBankChoice.getSelectedId() - 1); };
    sourceVoiceChoice.onChange = [this] {
        const int row = sourceVoiceChoice.getSelectedId() - 1;
        if (juce::isPositiveAndBelow(row,48)) {
            synchronisingSourceSelection = true;
            sourceList.selectRow(row);
            synchronisingSourceSelection = false;
        }
        if (bankEditorVisible) auditionSelectedSource();
    };
    auditionSource.onClick = [this] { auditionSelectedSource(); };
    auditionDraft.onClick = [this] { auditionSelectedSlot(); };
    stopAudition.onClick = [this] {
        auditionMode = AuditionMode::stopped;
        processor.stopVoicePreview();
        updateAuditionButtons();
        setEditorStatus("Audition stopped; original project sound restored.");
    };
    updateAuditionButtons();
    addAndMakeVisible(auditionSource);
    addAndMakeVisible(auditionDraft);
    addAndMakeVisible(stopAudition);
    copyVoice.onClick = [this] {
        const int from = sourceList.getSelectedRow();
        const int to = draftList.getSelectedRow();
        if (!juce::isPositiveAndBelow(from,48) || !juce::isPositiveAndBelow(to,48)) {
            setEditorStatus("Select a source voice and destination slot first."); return;
        }
        const auto sourceName = voiceName(sourceBank.data() + from * 64);
        const auto destName = voiceName(draftBank.data() + to * 64);
        juce::Component::SafePointer<CX5MEditor> safe(this);
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
            "Replace destination voice?",
            "Replace destination slot " + juce::String(to + 1) + " (" + destName + ") with source slot "
                + juce::String(from + 1) + " (" + sourceName + ")?",
            "REPLACE", "CANCEL", this,
            juce::ModalCallbackFunction::create([safe, from, to](int result) {
                if (safe == nullptr || result == 0) return;
                std::copy_n(safe->sourceBank.data() + from * 64, 64, safe->draftBank.data() + to * 64);
                safe->markDraftDirty();
                safe->refreshDraftList();
                if (safe->auditionMode == AuditionMode::slot) safe->auditionSelectedSlot();
                safe->setEditorStatus("Copied source slot " + juce::String(from+1) + " into destination slot " + juce::String(to+1) + ".");
            }));
    };
    auto swapAdjacent = [this](int delta) {
        const int row = draftList.getSelectedRow();
        const int other = row + delta;
        if (!juce::isPositiveAndBelow(row,48) || !juce::isPositiveAndBelow(other,48)) return;
        for (int i = 0; i < 64; ++i) std::swap(draftBank[static_cast<size_t>(row*64+i)], draftBank[static_cast<size_t>(other*64+i)]);
        markDraftDirty();
        refreshDraftList();
        draftList.selectRow(other);
        setEditorStatus("Swapped slots " + juce::String(row + 1) + " and " + juce::String(other + 1) + ".");
    };
    swapUp.onClick = [swapAdjacent] { swapAdjacent(-1); };
    swapDown.onClick = [swapAdjacent] { swapAdjacent(1); };
    loadDraft.onClick = [this] {
        draftChooser = std::make_unique<juce::FileChooser>("Open a decoded 48-voice BIN bank", defaultBankFolder(), "*.bin");
        juce::Component::SafePointer<CX5MEditor> safe(this);
        draftChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& fc) {
                if (safe == nullptr) return;
                const auto file = fc.getResult();
                if (!file.existsAsFile()) return;
                std::array<uint8_t,3072> candidate{};
                juce::String error;
                if (!readDecodedBank(file,candidate,error)) { safe->setEditorStatus(error); return; }
                if (safe->draftDirty) {
                    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,
                        "Replace unsaved destination?", "Loading this bank discards unsaved destination edits.",
                        "LOAD BANK", "CANCEL", safe.getComponent(),
                        juce::ModalCallbackFunction::create([safe, candidate, file](int result) {
                            if (safe == nullptr || result == 0) return;
                            safe->draftBank = candidate;
                            safe->destinationBankName = file.getFileName();
                            safe->draftDirty = false;
                            safe->updateDestinationLabel();
                            safe->refreshDraftList();
                            if (safe->auditionMode == AuditionMode::slot) safe->auditionSelectedSlot();
                            safe->setEditorStatus("Loaded destination: " + file.getFileName());
                        }));
                    return;
                }
                safe->draftBank = candidate;
                safe->destinationBankName = file.getFileName();
                safe->draftDirty = false;
                safe->updateDestinationLabel();
                safe->refreshDraftList();
                if (safe->auditionMode == AuditionMode::slot) safe->auditionSelectedSlot();
                safe->setEditorStatus("Opened draft: " + file.getFileName());
            });
    };
    newDraft.onClick = [this] {
        juce::Component::SafePointer<CX5MEditor> safe(this);
        auto reset = [safe](int result) {
            if (safe == nullptr || result == 0) return;
            safe->draftBank = cassetteBank::raw;
            safe->destinationBankName = "Unsaved custom bank";
            safe->draftDirty = false;
            safe->updateDestinationLabel();
            safe->refreshDraftList();
            if (safe->auditionMode == AuditionMode::slot) safe->auditionSelectedSlot();
            safe->setEditorStatus("New destination bank created from built-in voices.");
        };
        if (draftDirty) juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,
            "Discard unsaved destination edits?", "NEW BANK will discard unsaved changes in the destination bank.",
            "DISCARD AND NEW", "CANCEL", this, juce::ModalCallbackFunction::create(reset));
        else reset(1);
    };
    saveDraft.onClick = [this] {
        draftChooser = std::make_unique<juce::FileChooser>("Save your custom 48-voice BIN bank",
                        defaultBankFolder().getChildFile("CX5M-Custom-48.bin"), "*.bin");
        juce::Component::SafePointer<CX5MEditor> safe(this);
        draftChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe](const juce::FileChooser& fc) {
                if (safe == nullptr) return;
                auto file = fc.getResult();
                if (file == juce::File()) return;
                file = file.withFileExtension(".bin");
                if (!file.replaceWithData(safe->draftBank.data(), safe->draftBank.size())) {
                    safe->setEditorStatus("Could not write BIN file: " + file.getFullPathName()); return;
                }
                safe->destinationBankName = file.getFileName();
                safe->draftDirty = false;
                safe->updateDestinationLabel();
                safe->draftDirty = false;
                safe->updateDestinationLabel();
                safe->setEditorStatus("Saved 48 voices to " + file.getFileName());
            });
    };
    refreshDraftList();
    // Do not scan the disk while the plugin editor is being constructed.
    // The user can scan on opening the Librarian tab or pressing Refresh.
    showTab(false);
    setSize(860, 520);
}
void CX5MEditor::addControl(const juce::String& id, const juce::String& name) {
    Control c;
    c.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    c.slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe9ae5a));
    c.label = std::make_unique<juce::Label>(); c.label->setText(name, juce::dontSendNotification);
    c.label->setJustificationType(juce::Justification::centred);
    c.label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(*c.slider); addAndMakeVisible(*c.label);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, id, *c.slider);
    controls.push_back(std::move(c));
}
void CX5MEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff171e28));
    g.setColour(juce::Colour(0xff80c8dc)); g.setFont(juce::Font(27.f, juce::Font::bold));
    g.drawText("CX5M  /  REBORN", 22, 12, 550, 42, juce::Justification::centredLeft);
    g.setColour(juce::Colours::lightgrey); g.setFont(13.f);
    g.drawText("YM2151 CORE  |  48 RECOVERED CASSETTE VOICES (EXPERIMENTAL)  |  v0.5.4a", 24, 54, 800, 24, juce::Justification::centredLeft);
    if (!librarianVisible && !bankEditorVisible) for (int i = 0; i < 4; ++i) {
        auto r = juce::Rectangle<float>(20.f + i * 210.f, 105.f, 198.f, 250.f);
        g.setColour(juce::Colour(0xff263342)); g.fillRoundedRectangle(r, 8.f);
        g.setColour(juce::Colour(0xff80c8dc)); g.setFont(17.f);
        g.drawText("OPERATOR " + juce::String(i + 1), r.toNearestInt().removeFromTop(32), juce::Justification::centred);
    }
}
CX5MEditor::~CX5MEditor() { processor.stopVoicePreview(); }

void CX5MEditor::resized() {
    synthTab.setBounds(465, 16, 110, 35);
    librarianTab.setBounds(585, 16, 115, 35);
    editorTab.setBounds(710, 16, 130, 35);
    editorInstructions.setBounds(25, 84, 810, 22);
    sourceBankChoice.setBounds(25, 109, 370, 30);
    sourceList.setBounds(25, 144, 370, 250);
    draftList.setBounds(465, 109, 370, 285);
    previousSource.setBounds(25, 399, 75, 24);
    nextSource.setBounds(105, 399, 75, 24);
    auditionSource.setBounds(25, 429, 180, 28);
    stopAudition.setBounds(215, 429, 180, 28);
    copyVoice.setBounds(398, 218, 64, 64);
    swapUp.setBounds(465, 399, 90, 27);
    swapDown.setBounds(560, 399, 105, 27);
    renameSlot.setBounds(675, 399, 160, 27);
    auditionDraft.setBounds(465, 429, 170, 28);
    loadDraft.setBounds(465, 463, 165, 28);
    newDraft.setBounds(635, 463, 90, 28);
    saveDraft.setBounds(730, 463, 105, 28);
    destinationBankLabel.setBounds(465, 78, 370, 26);
    editorStatus.setBounds(25, 492, 810, 24);
    bankList.setBounds(25, 115, 355, 315);
    voiceList.setBounds(400, 115, 435, 315);
    rescanBanks.setBounds(25, 448, 190, 35);
    librarianInfo.setBounds(230, 448, 610, 35);
    for (int i = 0; i < 16; ++i) {
        int op = i / 4, n = i % 4;
        int x = 27 + op * 210 + (n % 2) * 94;
        int y = 145 + (n / 2) * 105;
        controls[i].slider->setBounds(x, y, 84, 75);
        controls[i].label->setBounds(x, y + 74, 84, 20);
    }
    controls[16].slider->setBounds(24, 390, 110, 90);
    controls[16].label->setBounds(24, 477, 110, 20);
    algorithm.setBounds(190, 412, 190, 32);
    bendLabel.setBounds(143, 460, 48, 22);
    bendRange.setBounds(190, 460, 65, 25);
    modLabel.setBounds(258, 460, 82, 22);
    modDepth.setBounds(338, 460, 138, 25);
    previousPatch.setBounds(402, 392, 38, 34);
    bank.setBounds(445, 392, 332, 34);
    nextPatch.setBounds(782, 392, 38, 34);
    bankStatus.setBounds(445, 426, 375, 22);
    cassette.setBounds(480, 450, 345, 36);
    settings.setBounds(390, 488, 85, 28);
    folderStatus.setBounds(480, 488, 350, 24);
}

void CX5MEditor::refreshBankNames() {
    const int selected = bank.getSelectedId();
    bank.clear(juce::dontSendNotification);
    bank.addItem("Prototype (v0.3)", 1);
    for (int i=0;i<48;++i)
        bank.addItem(juce::String(i+1).paddedLeft('0',2) + "  " + processor.getImportedVoiceName(i), i+2);
    bank.setSelectedId(selected, juce::dontSendNotification);
    bankStatus.setText(processor.hasImportedBank() ? "LOADED: " + processor.getBankDisplayName()
                                                   : "ORIGINAL BANK", juce::dontSendNotification);
}

juce::File CX5MEditor::defaultBankFolder() const {
    const juce::File fallback = juce::File::getSpecialLocation(
        juce::File::userDocumentsDirectory);
    if (!preferences) return fallback;
    const auto path = preferences->getValue("defaultBankFolder");
    if (path.isEmpty()) return fallback;
    const juce::File chosen(path);
    return chosen.isDirectory() ? chosen : fallback;
}

void CX5MEditor::refreshFolderStatus() {
    folderStatus.setText("Bank folder: " + defaultBankFolder().getFullPathName(),
                         juce::dontSendNotification);
    folderStatus.setTooltip(defaultBankFolder().getFullPathName());
}

void CX5MEditor::BankListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (selected) g.fillAll(juce::Colour(0xff354c62));
    g.setColour(juce::Colours::white);
    if (juce::isPositiveAndBelow(row, owner.availableBankNames.size()))
        g.drawText(owner.availableBankNames[row], 8, 0, w - 12, h, juce::Justification::centredLeft);
}
void CX5MEditor::BankListModel::selectedRowsChanged(int row) {
    if (!owner.synchronisingLibrarianSelection) owner.selectSourceBank(row);
}
void CX5MEditor::VoiceListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (selected) g.fillAll(juce::Colour(0xff354c62));
    g.setColour(juce::Colours::white);
    if (row >= 0 && row < 48)
        g.drawText(juce::String(row + 1).paddedLeft('0', 2) + "   " + owner.processor.getImportedVoiceName(row),
                   8, 0, w - 12, h, juce::Justification::centredLeft);
}
void CX5MEditor::VoiceListModel::selectedRowsChanged(int row) {
    if (!owner.synchronisingLibrarianSelection && row >= 0 && row < 48)
        owner.bank.setSelectedId(row + 2, juce::sendNotificationSync);
}
void CX5MEditor::scanBankFolder() {
    availableBankNames.clear();
    availableBankFiles.clear();
    availableBankNames.add("Built-in original bank");
    availableBankFiles.add(juce::File());
    juce::Array<juce::File> files;
    defaultBankFolder().findChildFiles(files, juce::File::findFiles, true, "*.bin");
    for (const auto& file : files) {
        if (file.getSize() != 3072) continue;
        availableBankNames.add(file.getFileNameWithoutExtension());
        availableBankFiles.add(file);
    }
    bankList.updateContent();
    bankList.repaint();
    // Reflect the restored sound in the GUI without importing or selecting a new bank.
    synchroniseLibrarianToLoadedVoice();
}
void CX5MEditor::synchroniseLibrarianToLoadedVoice() {
    // JUCE ListBox::selectRow invokes selectedRowsChanged; suppress both
    // callbacks so that synchronisation never changes the processor's bank.
    const juce::ScopedValueSetter<bool> guard(synchronisingLibrarianSelection, true);
    int matchingRow = -1;
    if (!processor.hasImportedBank()) {
        matchingRow = 0;
    } else {
        // Saved project state stores the original filename, not the full path.
        // Only highlight a file when the match is unambiguous.
        const auto loadedName = processor.getBankDisplayName();
        for (int i = 1; i < availableBankFiles.size(); ++i) {
            if (availableBankFiles.getReference(i).getFileName().equalsIgnoreCase(loadedName)) {
                if (matchingRow != -1) { matchingRow = -1; break; }
                matchingRow = i;
            }
        }
    }
    bankList.deselectAllRows();
    if (matchingRow >= 0) bankList.selectRow(matchingRow);
    // The voice list reads the current bank directly from the processor,
    // including a bank embedded in a saved project but no longer on disk.
    voiceList.updateContent();
    const auto* choice = processor.state.getRawParameterValue("bank");
    const int selectedVoice = choice != nullptr ? static_cast<int>(choice->load()) - 1 : -1;
    voiceList.deselectAllRows();
    if (juce::isPositiveAndBelow(selectedVoice, 48)) voiceList.selectRow(selectedVoice);
    voiceList.repaint();
    librarianInfo.setText("Current: " + processor.getBankDisplayName()
                          + "  |  Choose a bank to load a different sound set.",
                          juce::dontSendNotification);
}

void CX5MEditor::selectSourceBank(int row) {
    if (!juce::isPositiveAndBelow(row, availableBankFiles.size())) return;
    const auto file = availableBankFiles.getReference(row);
    if (row == 0) {
        processor.restoreBuiltInBank();
    } else {
        juce::String error;
        if (!processor.importVoiceBank(file, error)) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                    "Bank import failed", error);
            return;
        }
    }
    refreshBankNames();
    voiceList.updateContent();
    voiceList.repaint();
    voiceList.selectRow(0);
    librarianInfo.setText("Loaded: " + availableBankNames[row], juce::dontSendNotification);
}
void CX5MEditor::showTab(bool librarian) {
    // SYNTH and LIBRARIAN callers reset bank-editor mode; editorTab sets it explicitly after calling this.
    librarianVisible = librarian;
    const bool synthVisible = !librarian && !bankEditorVisible;
    bankList.setVisible(librarian);
    voiceList.setVisible(librarian);
    rescanBanks.setVisible(librarian);
    librarianInfo.setVisible(librarian);
    for (auto& c : controls) { c.slider->setVisible(synthVisible); c.label->setVisible(synthVisible); }
    algorithm.setVisible(synthVisible);
    bank.setVisible(synthVisible);
    bendRange.setVisible(synthVisible);
    bendLabel.setVisible(synthVisible);
    modDepth.setVisible(synthVisible);
    modLabel.setVisible(synthVisible);
    previousPatch.setVisible(synthVisible);
    nextPatch.setVisible(synthVisible);
    bankStatus.setVisible(synthVisible);
    cassette.setVisible(synthVisible);
    settings.setVisible(synthVisible);
    folderStatus.setVisible(synthVisible);
    const bool edit = bankEditorVisible;
    sourceBankChoice.setVisible(edit);
    sourceVoiceChoice.setVisible(false);
    sourceList.setVisible(edit);
    previousSource.setVisible(edit);
    nextSource.setVisible(edit);
    renameSlot.setVisible(edit);
    destinationBankLabel.setVisible(edit);
    auditionSource.setVisible(edit);
    auditionDraft.setVisible(edit);
    stopAudition.setVisible(edit);
    draftList.setVisible(edit);
    copyVoice.setVisible(edit);
    swapUp.setVisible(edit);
    swapDown.setVisible(edit);
    loadDraft.setVisible(edit);
    newDraft.setVisible(edit);
    saveDraft.setVisible(edit);
    editorInstructions.setVisible(edit);
    editorStatus.setVisible(edit);
    repaint();
}

juce::String CX5MEditor::voiceName(const uint8_t* bytes) {
    juce::String name;
    for (int i=0; i<7; ++i) {
        const auto c = bytes[i];
        if (c >= 32 && c <= 126) name += juce::String::charToString(static_cast<juce_wchar>(c));
    }
    return name.trim();
}

bool CX5MEditor::readDecodedBank(const juce::File& file, std::array<uint8_t,3072>& dest, juce::String& error) {
    if (!file.existsAsFile() || file.getSize() != 3072) {
        error = "BIN must contain exactly 48 x 64-byte voices (3072 bytes)."; return false;
    }
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() != dest.size()) {
        error = "Unable to read bank."; return false;
    }
    int plausible = 0;
    for (int i=0; i<48; ++i) {
        const auto* rec = static_cast<const uint8_t*>(data.getData()) + i*64;
        bool printable = true;
        for (int j=0; j<7; ++j) if (rec[j]<32 || rec[j]>126) printable = false;
        if (printable && rec[11] == 0x78) ++plausible;
    }
    if (plausible < 40) { error = "Bank records do not match the expected decoded CX5M format."; return false; }
    std::memcpy(dest.data(), data.getData(), dest.size());
    return true;
}

void CX5MEditor::setEditorStatus(const juce::String& message) {
    editorStatus.setText(message, juce::dontSendNotification);
}

void CX5MEditor::SourceListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (!juce::isPositiveAndBelow(row,48)) return;
    if (selected) g.fillAll(juce::Colour(0xff354c62));
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(row+1).paddedLeft('0',2) + "    "
               + CX5MEditor::voiceName(owner.sourceBank.data()+row*64),
               8,0,w-12,h,juce::Justification::centredLeft);
}
void CX5MEditor::SourceListModel::selectedRowsChanged(int row) {
    if (owner.synchronisingSourceSelection || !juce::isPositiveAndBelow(row,48)) return;
    owner.sourceVoiceChoice.setSelectedId(row+1, juce::sendNotificationSync);
    owner.sourceList.scrollToEnsureRowIsOnscreen(row);
}

void CX5MEditor::DraftListModel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (!juce::isPositiveAndBelow(row,48)) return;
    if (selected) g.fillAll(juce::Colour(0xff354c62));
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(row+1).paddedLeft('0',2) + "    "
               + CX5MEditor::voiceName(owner.draftBank.data()+row*64),
               8,0,w-12,h,juce::Justification::centredLeft);
}

void CX5MEditor::DraftListModel::selectedRowsChanged(int row) {
    if (owner.bankEditorVisible && juce::isPositiveAndBelow(row, 48)) owner.auditionSelectedSlot();
}

bool CX5MEditor::isEmptyVoice(const uint8_t* record) {
    bool blank = true;
    for (int i = 0; i < 64; ++i) if (record[i] != 0 && record[i] != 32) { blank = false; break; }
    return blank || voiceName(record).isEmpty();
}
void CX5MEditor::updateAuditionButtons() {
    const auto cyan = juce::Colour(0xff00bcd4);
    const auto inactive = juce::Colour(0xff354c62);
    auditionSource.setColour(juce::TextButton::buttonColourId, auditionMode == AuditionMode::source ? cyan : inactive);
    auditionDraft.setColour(juce::TextButton::buttonColourId, auditionMode == AuditionMode::slot ? cyan : inactive);
}
void CX5MEditor::markDraftDirty() { draftDirty = true; updateDestinationLabel(); }
void CX5MEditor::updateDestinationLabel() {
    destinationBankLabel.setText("DESTINATION: " + destinationBankName + (draftDirty ? " *" : ""), juce::dontSendNotification);
    destinationBankLabel.setTooltip("Working destination bank; disk changes occur only after SAVE BANK .BIN.");
}
void CX5MEditor::auditionSelectedSource() {
    const int row = sourceVoiceChoice.getSelectedId() - 1;
    if (!juce::isPositiveAndBelow(row, 48)) return;
    auditionMode = AuditionMode::source;
    processor.previewVoice(sourceBank.data() + row * 64);
    updateAuditionButtons();
    setEditorStatus("AUDITION SOURCE " + juce::String(row + 1) + ": " + voiceName(sourceBank.data() + row * 64));
}
void CX5MEditor::auditionSelectedSlot() {
    const int row = draftList.getSelectedRow();
    if (!juce::isPositiveAndBelow(row, 48)) { setEditorStatus("Select a destination slot first."); return; }
    auditionMode = AuditionMode::slot;
    const auto* record = draftBank.data() + row * 64;
    if (isEmptyVoice(record)) {
        processor.previewSilence();
        setEditorStatus("AUDITION SLOT " + juce::String(row + 1) + ": empty (silent)");
    } else {
        processor.previewVoice(record);
        setEditorStatus("AUDITION SLOT " + juce::String(row + 1) + ": " + voiceName(record));
    }
    updateAuditionButtons();
}

void CX5MEditor::refreshDraftList() {
    draftList.updateContent();
    draftList.repaint();
}

void CX5MEditor::scanEditorSources() {
    // Reuse the already-proven recursive bank enumeration; do not touch the live processor.
    scanBankFolder();
    const int previous = sourceBankChoice.getSelectedId();
    sourceBankChoice.clear(juce::dontSendNotification);
    for (int i=0; i<availableBankNames.size(); ++i)
        sourceBankChoice.addItem(availableBankNames[i],i+1);
    sourceBankChoice.setSelectedId(juce::isPositiveAndBelow(previous-1,availableBankNames.size()) ? previous : 1,
                                   juce::dontSendNotification);
    loadEditorSource(sourceBankChoice.getSelectedId()-1);
}

void CX5MEditor::loadEditorSource(int row) {
    if (!juce::isPositiveAndBelow(row,availableBankFiles.size())) return;
    if (row == 0) {
        sourceBank = cassetteBank::raw;
    } else {
        std::array<uint8_t,3072> candidate{};
        juce::String error;
        if (!readDecodedBank(availableBankFiles.getReference(row),candidate,error)) {
            setEditorStatus(error); return;
        }
        sourceBank = candidate;
    }
    refreshSourceVoices();
    if (bankEditorVisible && auditionMode == AuditionMode::source) auditionSelectedSource();
}

void CX5MEditor::refreshSourceVoices() {
    const int oldSelection = sourceVoiceChoice.getSelectedId();
    sourceVoiceChoice.clear(juce::dontSendNotification);
    for (int i=0;i<48;++i)
        sourceVoiceChoice.addItem(juce::String(i+1).paddedLeft('0',2) + "  " + voiceName(sourceBank.data()+i*64), i+1);
    sourceVoiceChoice.setSelectedId(juce::isPositiveAndBelow(oldSelection-1,48) ? oldSelection : 1,
                                    juce::dontSendNotification);
    sourceList.updateContent();
    synchronisingSourceSelection = true;
    sourceList.selectRow(sourceVoiceChoice.getSelectedId()-1);
    sourceList.scrollToEnsureRowIsOnscreen(sourceList.getSelectedRow());
    synchronisingSourceSelection = false;
    sourceList.repaint();
}

void CX5MEditor::stepSourceSelection(int delta) {
    int row = sourceList.getSelectedRow();
    if (!juce::isPositiveAndBelow(row, 48)) row = 0;
    else row = (row + delta + 48) % 48;
    sourceList.selectRow(row);
    sourceList.scrollToEnsureRowIsOnscreen(row);
    sourceList.grabKeyboardFocus();
}

bool CX5MEditor::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) {
    const auto code = key.getKeyCode();
    if (bankEditorVisible) {
        // Only capture arrows when the source list or its navigation buttons have focus.
        // This keeps host/DAW shortcuts available elsewhere.
        if (originatingComponent == &sourceList || (originatingComponent != nullptr && sourceList.isParentOf(originatingComponent))
            || originatingComponent == &previousSource
            || originatingComponent == &nextSource) {
            if (code == juce::KeyPress::upKey || code == juce::KeyPress::leftKey) {
                stepSourceSelection(-1); return true;
            }
            if (code == juce::KeyPress::downKey || code == juce::KeyPress::rightKey) {
                stepSourceSelection(1); return true;
            }
        }
        return false;
    }
    if (librarianVisible) return false;
    if (code == juce::KeyPress::leftKey) { previousPatch.triggerClick(); return true; }
    if (code == juce::KeyPress::rightKey) { nextPatch.triggerClick(); return true; }
    return false;
}
