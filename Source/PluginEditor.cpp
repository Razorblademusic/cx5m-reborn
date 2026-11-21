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
    bendRange.addItemList({juce::String::fromUTF8("\xC2\xB1") + "2",
                           juce::String::fromUTF8("\xC2\xB1") + "5",
                           juce::String::fromUTF8("\xC2\xB1") + "7",
                           juce::String::fromUTF8("\xC2\xB1") + "12"}, 1);
    addAndMakeVisible(bendRange);
    bendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state, "bendRange", bendRange);
    bendLabel.setText("BEND", juce::dontSendNotification);
    bendLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bendLabel);
    modDepth.setSliderStyle(juce::Slider::LinearHorizontal);
    modDepth.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
    modDepth.setNumDecimalPlacesToDisplay(2);
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
    editorInstructions.setText("SOURCE BANK (48 VOICES)", juce::dontSendNotification);
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
    // v0.5.5: Yamaha-inspired cosmetic palette; no DSP, MIDI or bank logic changes.
    const auto teal = juce::Colour(0xff08bdb7);
    const auto button = juce::Colour(0xff34343a);
    const auto edge = juce::Colour(0xff65656b);
    for (auto* b : { &synthTab, &librarianTab, &editorTab, &previousPatch,
                     &nextPatch, &cassette, &settings, &rescanBanks,
                     &previousSource, &nextSource, &copyVoice, &swapUp,
                     &swapDown, &renameSlot, &loadDraft, &newDraft, &saveDraft,
                     &stopAudition }) {
        b->setColour(juce::TextButton::buttonColourId, button);
        b->setColour(juce::TextButton::buttonOnColourId, teal);
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    for (auto* list : { &bankList, &voiceList, &sourceList, &draftList }) {
        list->setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a2028));
        list->setColour(juce::ListBox::outlineColourId, edge);
        list->setOutlineThickness(1);
    }
    for (auto* combo : { &algorithm, &bank, &bendRange, &sourceBankChoice, &sourceVoiceChoice }) {
        combo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff20242b));
        combo->setColour(juce::ComboBox::textColourId, juce::Colours::white);
        combo->setColour(juce::ComboBox::outlineColourId, edge);
        combo->setColour(juce::ComboBox::arrowColourId, teal);
    }
    setLookAndFeel(&yamahaStyle);
    setSize(1060, 680);
}
void CX5MEditor::addControl(const juce::String& id, const juce::String& name) {
    Control c;
    c.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    c.slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff08bdb7));
    c.slider->setNumDecimalPlacesToDisplay(id.startsWith("attack") || id.startsWith("release") ? 3 : 2);
    c.slider->textFromValueFunction = [id](double value) {
        const int decimals = id.startsWith("attack") || id.startsWith("release") ? 3 : 2;
        return juce::String(value, decimals);
    };
    c.label = std::make_unique<juce::Label>(); c.label->setText(name, juce::dontSendNotification);
    c.label->setJustificationType(juce::Justification::centred);
    c.label->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(*c.slider); addAndMakeVisible(*c.label);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, id, *c.slider);
    controls.push_back(std::move(c));
}
namespace {
// Vector lettering inspired by the angular, extended CX5M computer badge.
// Drawn in JUCE so the plugin needs no external font or image resources.
void drawCX5MBadge(juce::Graphics& g, juce::Rectangle<float> area) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::translation(area.getX(), area.getY())
                       .scaled(area.getWidth() / 234.f, area.getHeight() / 46.f));
    const auto turquoise = juce::Colour(0xff00d3c5);
    const auto dark = juce::Colour(0xff076e71);
    g.setColour(dark.withAlpha(0.32f));
    g.fillRoundedRectangle(0.f, 2.f, 234.f, 44.f, 2.f);
    g.setColour(turquoise);
    // The extended C and X are cut from angular MSX-era geometry.
    juce::Path c;
    c.startNewSubPath(49.f, 3.f); c.lineTo(17.f, 3.f); c.lineTo(0.f, 23.f);
    c.lineTo(17.f, 43.f); c.lineTo(49.f, 43.f); c.lineTo(60.f, 31.f);
    c.lineTo(24.f, 31.f); c.lineTo(17.f, 23.f); c.lineTo(24.f, 15.f);
    c.lineTo(60.f, 15.f); c.closeSubPath(); g.fillPath(c);
    juce::Path x;
    x.startNewSubPath(59.f, 3.f); x.lineTo(82.f, 3.f); x.lineTo(100.f, 16.f);
    x.lineTo(118.f, 3.f); x.lineTo(141.f, 3.f); x.lineTo(113.f, 23.f);
    x.lineTo(141.f, 43.f); x.lineTo(118.f, 43.f); x.lineTo(100.f, 30.f);
    x.lineTo(82.f, 43.f); x.lineTo(59.f, 43.f); x.lineTo(87.f, 23.f);
    x.closeSubPath(); g.fillPath(x);
    // Striped digital 5, characteristic of the original CX5M mark.
    g.fillRect(146.f, 3.f, 43.f, 8.f);
    g.fillRect(146.f, 3.f, 9.f, 22.f);
    g.fillRect(146.f, 19.f, 42.f, 7.f);
    g.fillRect(179.f, 19.f, 9.f, 24.f);
    g.fillRect(146.f, 35.f, 42.f, 8.f);
    g.setColour(juce::Colour(0xff101c20));
    for (float y : { 5.f, 8.f, 21.f, 24.f, 37.f, 40.f })
        g.drawLine(159.f, y, 176.f, y, 0.7f);
    g.setColour(turquoise);
    juce::Path m;
    m.startNewSubPath(195.f,43.f);m.lineTo(195.f,3.f);m.lineTo(205.f,3.f);
    m.lineTo(215.f,21.f);m.lineTo(225.f,3.f);m.lineTo(234.f,3.f);
    m.lineTo(234.f,43.f);m.lineTo(225.f,43.f);m.lineTo(225.f,21.f);
    m.lineTo(216.f,37.f);m.lineTo(212.f,37.f);m.lineTo(204.f,21.f);
    m.lineTo(204.f,43.f);m.closeSubPath();g.fillPath(m);
}
void hardwarePanel(juce::Graphics& g, juce::Rectangle<float> r, float corner = 7.f) {
    g.setColour(juce::Colour(0xff101116));
    g.fillRoundedRectangle(r.translated(0.f, 2.f), corner);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff393a41), r.getX(),r.getY(),
                                         juce::Colour(0xff292a31),r.getX(),r.getBottom(),false));
    g.fillRoundedRectangle(r, corner);
    g.setColour(juce::Colour(0xff84858b).withAlpha(0.7f));
    g.drawRoundedRectangle(r.reduced(0.5f),corner,1.f);
    g.setColour(juce::Colour(0xff121318));
    g.drawLine(r.getX()+7.f,r.getBottom()-2.f,r.getRight()-7.f,r.getBottom()-2.f,1.f);
}
}
void CX5MEditor::paint(juce::Graphics& g) {
    const auto teal = juce::Colour(0xff16d9cf);
    const float w = (float) getWidth(), h = (float) getHeight();
    // Brushed graphite enclosure: fine machining lines and edge lighting.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff42434a), 0.f, 0.f,
                                          juce::Colour(0xff18191f), w, h, false));
    g.fillAll();
    for (int y = 0; y < getHeight(); y += 3) {
        g.setColour(juce::Colours::white.withAlpha((y % 9 == 0) ? 0.016f : 0.008f));
        g.drawHorizontalLine(y, 0.f, w);
    }
    g.setColour(juce::Colour(0xff0b0d10));
    g.fillRoundedRectangle(2.f, 2.f, w-4.f, 112.f, 10.f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff63646b), 4.f, 4.f,
                                          juce::Colour(0xff292a30), 4.f, 106.f, false));
    g.fillRoundedRectangle(4.f, 4.f, w-8.f, 103.f, 9.f);
    g.setColour(juce::Colour(0xffa0a1a7).withAlpha(0.5f));
    g.drawRoundedRectangle(4.5f, 4.5f, w-9.f, 102.f, 9.f, 1.f);
    // Deep ventilation apertures with a highlight on the near edge.
    for (int x = 24; x < getWidth()-22; x += 13) {
        g.setColour(juce::Colour(0xff090a0e));
        g.fillRoundedRectangle((float)x, 11.f, 6.f, 17.f, 2.3f);
        g.setColour(juce::Colour(0xff98999e).withAlpha(0.32f));
        g.drawLine((float)x+1.f, 10.5f, (float)x+6.f, 10.5f, 0.7f);
    }
    g.setColour(juce::Colour(0xfff0f0ef));
    g.setFont(juce::Font(18.f, juce::Font::bold));
    g.drawText("YAMAHA", 26, 47, 91, 27, juce::Justification::centredLeft);
    drawCX5MBadge(g, {126.f, 42.f, 204.f, 43.f});
    g.setColour(juce::Colour(0xffe6e6e9));
    g.setFont(juce::Font(18.f, juce::Font::italic));
    g.drawText("REBORN", 340, 51, 110, 24, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffb8bac2));
    g.setFont(juce::Font(10.5f));
    g.drawText("FM MUSIC COMPUTER  /  YM2151", 27, 85, 310, 14, juce::Justification::centredLeft);
    g.drawText("v0.6.0  |  HARDWARE EDITION", 347, 85, 210, 14, juce::Justification::centredLeft);
    // Small embossed MSX emblem, deliberately separated from the navigation.
    g.setColour(juce::Colour(0xff13151b));
    g.fillRoundedRectangle(w-78.f, 77.f, 57.f, 22.f, 2.f);
    g.setColour(juce::Colour(0xffaeb0b5));
    g.drawRoundedRectangle(w-77.5f, 77.5f, 56.f, 21.f, 2.f, 0.8f);
    g.setFont(juce::Font(15.f, juce::Font::bold | juce::Font::italic));
    g.drawText("MSX", (int)w-74, 77, 50, 22, juce::Justification::centred);
    // Machined faceplate and inset mounting screws.
    g.setColour(juce::Colour(0xff111217));
    g.drawHorizontalLine(111, 6.f, w-6.f);
    auto screw = [&g](float x, float y) {
        g.setColour(juce::Colour(0xff111217)); g.fillEllipse(x-4.f,y-4.f,8.f,8.f);
        g.setColour(juce::Colour(0xff878991)); g.drawEllipse(x-3.5f,y-3.5f,7.f,7.f,0.8f);
        g.setColour(juce::Colour(0xff1b1c21)); g.drawLine(x-2.f,y+1.f,x+2.f,y-1.f,1.f);
    };
    screw(13.f, 122.f); screw(w-13.f, 122.f);
    screw(13.f, h-12.f); screw(w-13.f, h-12.f);
    if (!librarianVisible && !bankEditorVisible) {
        // Upper synth control deck: individually machined operator modules.
        for (int i = 0; i < 4; ++i) {
            const float x = 22.f + (float)i * 258.f;
            auto r = juce::Rectangle<float>(x, 136.f, 246.f, 326.f);
            hardwarePanel(g, r, 8.f);
            g.setColour(juce::Colour(0xff11151b));
            g.fillRoundedRectangle(x+12.f, 147.f, 222.f, 32.f, 4.f);
            g.setColour(juce::Colour(0xff747881));
            g.drawRoundedRectangle(x+12.5f,147.5f,221.f,31.f,4.f,0.6f);
            g.setColour(teal);
            g.setFont(juce::Font(14.f, juce::Font::bold));
            g.drawText("OPERATOR " + juce::String(i+1), (int)x+16, 149, 214, 27, juce::Justification::centred);
            g.setColour(juce::Colour(0xff8e9199).withAlpha(0.35f));
            g.drawHorizontalLine(313, x+13.f, x+233.f);
        }
        hardwarePanel(g, {22.f, 474.f, 470.f, 185.f});
        hardwarePanel(g, {503.f, 474.f, 535.f, 185.f});
        g.setColour(teal); g.setFont(juce::Font(12.f, juce::Font::bold));
        g.drawText("GLOBAL  /  CONTROLLERS", 36, 484, 300, 20, juce::Justification::centredLeft);
        g.drawText("VOICE  /  BANK", 517, 484, 270, 20, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff11151b));
        g.fillRoundedRectangle(177.f, 531.f, 250.f, 47.f, 4.f);
        g.setColour(juce::Colour(0xff606a72));
        g.drawRoundedRectangle(177.5f,531.5f,249.f,46.f,4.f,0.7f);
    } else if (bankEditorVisible) {
        hardwarePanel(g, {22.f, 136.f, 473.f, 520.f});
        hardwarePanel(g, {563.f, 136.f, 475.f, 520.f});
        g.setColour(teal); g.setFont(juce::Font(14.f, juce::Font::bold));
        g.drawText("DESTINATION BANK  /  48 VOICES", 580, 146, 442, 23, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff11151b));
        g.fillRoundedRectangle(502.f, 323.f, 54.f, 66.f, 5.f);
    } else {
        hardwarePanel(g, {22.f, 136.f, 486.f, 520.f});
        hardwarePanel(g, {520.f, 136.f, 518.f, 520.f});
        g.setColour(teal); g.setFont(juce::Font(14.f, juce::Font::bold));
        g.drawText("AVAILABLE BANKS", 40, 147, 450, 23, juce::Justification::centredLeft);
        g.drawText("VOICES  /  48 SLOTS", 538, 147, 475, 23, juce::Justification::centredLeft);
    }
}
CX5MEditor::~CX5MEditor() { setLookAndFeel(nullptr); processor.stopVoicePreview(); }

void CX5MEditor::resized() {
    synthTab.setBounds(607, 40, 125, 34);
    librarianTab.setBounds(738, 40, 135, 34);
    editorTab.setBounds(879, 40, 155, 34);
    // Bank editor: two balanced wells, central transfer control, footer actions.
    editorInstructions.setBounds(39, 145, 443, 25);
    sourceBankChoice.setBounds(39, 177, 443, 31);
    sourceList.setBounds(39, 216, 443, 292);
    destinationBankLabel.setBounds(580, 177, 440, 31);
    draftList.setBounds(580, 216, 440, 292);
    copyVoice.setBounds(501, 335, 56, 40);
    previousSource.setBounds(39, 518, 86, 29);
    nextSource.setBounds(132, 518, 86, 29);
    auditionSource.setBounds(39, 559, 196, 33);
    stopAudition.setBounds(246, 559, 236, 33);
    swapUp.setBounds(580, 518, 111, 29);
    swapDown.setBounds(699, 518, 126, 29);
    renameSlot.setBounds(835, 518, 185, 29);
    auditionDraft.setBounds(580, 559, 190, 33);
    loadDraft.setBounds(580, 602, 188, 34);
    newDraft.setBounds(779, 602, 106, 34);
    saveDraft.setBounds(895, 602, 125, 34);
    editorStatus.setBounds(39, 611, 435, 30);
    // Librarian: generous list space and no clipped actions.
    bankList.setBounds(39, 180, 451, 388);
    voiceList.setBounds(538, 180, 480, 388);
    rescanBanks.setBounds(39, 586, 200, 38);
    librarianInfo.setBounds(253, 586, 765, 38);
    // Four operator modules, two controls per row.
    for (int i = 0; i < 16; ++i) {
        const int op = i / 4, n = i % 4;
        const int x = 40 + op * 258 + (n % 2) * 114;
        const int y = 190 + (n / 2) * 132;
        controls[i].slider->setBounds(x, y, 99, 91);
        controls[i].label->setBounds(x, y + 89, 99, 23);
    }
    controls[16].slider->setBounds(41, 510, 124, 108);
    controls[16].label->setBounds(41, 613, 124, 22);
    algorithm.setBounds(183, 541, 237, 33);
    bendLabel.setBounds(180, 589, 52, 23);
    bendRange.setBounds(234, 589, 72, 26);
    modLabel.setBounds(311, 589, 89, 23);
    modDepth.setBounds(399, 589, 98, 26);
    previousPatch.setBounds(520, 517, 45, 37);
    bank.setBounds(573, 517, 397, 37);
    nextPatch.setBounds(978, 517, 45, 37);
    bankStatus.setBounds(573, 558, 447, 22);
    cassette.setBounds(650, 586, 370, 35);
    settings.setBounds(517, 626, 110, 29);
    folderStatus.setBounds(640, 627, 378, 27);
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
