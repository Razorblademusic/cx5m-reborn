#include "PluginEditor.h"
#include "CassetteBank.h"
#include <BinaryData.h>
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
    modDepth.setTextBoxStyle(juce::Slider::TextBoxRight, false, 34, 18);
    addAndMakeVisible(modDepth);
    modAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, "modDepth", modDepth);
    modDepth.textFromValueFunction = [](double value) { return juce::String(value, 2); };
    modDepth.updateText();
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
    synthTab.onClick = [this] {
        if (bankEditorVisible) stopAudition.onClick();
        bankEditorVisible = false;
        showTab(false);
    };
    librarianTab.onClick = [this] {
        if (bankEditorVisible) stopAudition.onClick();
        bankEditorVisible = false;
        showTab(true);
        scanBankFolder();
    };
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
    previousSource.setButtonText("PREV");
    nextSource.setButtonText("NEXT");
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
    addAndMakeVisible(editorStatus);
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
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffeee1c6));
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xff081d20));
    }
    for (auto* list : { &bankList, &voiceList, &sourceList, &draftList }) {
        list->setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0a1012));
        list->setColour(juce::ListBox::outlineColourId, edge);
        list->setOutlineThickness(1);
        list->setRowHeight(22);
    }
    for (auto* combo : { &algorithm, &bank, &bendRange, &sourceBankChoice, &sourceVoiceChoice }) {
        combo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff20242b));
        combo->setColour(juce::ComboBox::textColourId, juce::Colours::white);
        combo->setColour(juce::ComboBox::outlineColourId, edge);
        combo->setColour(juce::ComboBox::arrowColourId, teal);
    }
    for (auto* display : { &bank, &algorithm }) {
        display->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff59b9bd));
        display->setColour(juce::ComboBox::textColourId, juce::Colour(0xff081d20));
        display->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff081d20));
    }
    setLookAndFeel(&yamahaStyle);
}
void CX5MEditor::addControl(const juce::String& id, const juce::String& name) {
    Control c;
    c.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    c.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
    c.label = std::make_unique<juce::Label>(); c.label->setText(name, juce::dontSendNotification);
    c.label->setJustificationType(juce::Justification::centred);
    c.label->setColour(juce::Label::textColourId, juce::Colour(0xffeee1c6));
    c.label->setFont(juce::Font(11.f, juce::Font::bold));
    addAndMakeVisible(*c.slider); addAndMakeVisible(*c.label);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state, id, *c.slider);
    const int decimals = id.startsWith("attack") || id.startsWith("release") ? 3 : 2;
    c.slider->textFromValueFunction = [decimals](double value) {
        return juce::String(value, decimals);
    };
    c.slider->updateText();
    controls.push_back(std::move(c));
}
namespace {
constexpr auto cream = 0xffeee1c6;
constexpr auto cyan = 0xff26d9df;

void drawPanel(juce::Graphics& g, juce::Rectangle<float> area, bool recessed = false) {
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRoundedRectangle(area.translated(0.f, 2.f), 6.f);
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(recessed ? 0xff0a0d0e : 0xff353434), area.getTopLeft(),
        juce::Colour(recessed ? 0xff222525 : 0xff212222), area.getBottomLeft(), false));
    g.fillRoundedRectangle(area, 6.f);
    g.setColour(juce::Colour(recessed ? 0xff575653 : 0xff77726a));
    g.drawRoundedRectangle(area.reduced(0.5f), 6.f, 1.f);
}

void drawLegend(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area) {
    g.setColour(juce::Colour(cream));
    g.setFont(juce::Font(15.f, juce::Font::bold));
    g.drawText(text, area, juce::Justification::centredLeft);
}

void drawStripe(juce::Graphics& g, int x, int y, int width) {
    g.setColour(juce::Colour(0xff178f9a));
    g.fillRect(x, y, width, 2);
}
}

void CX5MEditor::paint(juce::Graphics& g) {
    const auto texture = juce::ImageCache::getFromMemory(BinaryData::charcoal_png,
                                                         BinaryData::charcoal_pngSize);
    const auto logo = juce::ImageCache::getFromMemory(BinaryData::wordmark_png,
                                                      BinaryData::wordmark_pngSize);
    g.fillAll(juce::Colour(0xff252525));
    if (texture.isValid()) {
        g.setTiledImageFill(texture, 0, 0, 1.f);
        g.fillAll();
    }
    g.setColour(juce::Colour(0xff77716a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3.f), 10.f, 1.f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(5.f), 8.f, 1.f);

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImageWithin(logo, 24, 20, 337, 39, juce::RectanglePlacement::centred);
    drawStripe(g, 23, 64, 337);
    drawStripe(g, 23, 69, 337);
    g.setColour(juce::Colour(cream));
    g.setFont(juce::Font(12.f));
    g.drawText("4 OPERATOR FM SYNTHESIZER  /  v" + juce::String(JucePlugin_VersionString),
               25, 76, 337, 20, juce::Justification::centredLeft);

    if (!librarianVisible) {
        drawPanel(g, {375.f, 17.f, 380.f, 77.f}, true);
        g.setColour(juce::Colour(cyan));
        g.setFont(juce::Font(11.f, juce::Font::bold));
        g.drawText(bankEditorVisible ? "SYNTH PATCH" : "PATCH", 391, 23, 220, 18,
                   juce::Justification::centredLeft);
        g.drawText("ALGORITHM", 618, 23, 123, 18, juce::Justification::centredLeft);

        for (int op = 0; op < 4; ++op) {
            const int x = 18 + op * 223;
            drawPanel(g, {(float)x, 108.f, 214.f, 135.f});
            drawLegend(g, "OP " + juce::String(op + 1), {x + 12, 114, 120, 23});
            drawStripe(g, x + 10, 137, 194);
        }
        drawPanel(g, {911.f, 108.f, 151.f, 135.f});
        drawLegend(g, "MASTER", {923, 114, 126, 23});
        drawStripe(g, 921, 137, 131);
    }

    if (bankEditorVisible || librarianVisible) {
        const int top = bankEditorVisible ? 255 : 108;
        const float height = bankEditorVisible ? 399.f : 546.f;
        drawPanel(g, {18.f, (float)top, 488.f, height}, true);
        drawPanel(g, {574.f, (float)top, 488.f, height}, true);
        drawLegend(g, bankEditorVisible ? "SOURCE BANK" : "AVAILABLE BANKS",
                   {30, top + 6, 466, 22});
        drawLegend(g, bankEditorVisible ? "DESTINATION BANK" : "VOICES",
                   {586, top + 6, 466, 22});
        drawStripe(g, 30, top + 29, 464);
        drawStripe(g, 586, top + 29, 464);
    } else {
        drawPanel(g, {18.f, 255.f, 1044.f, 118.f}, true);
        drawLegend(g, "BANK AND PERFORMANCE", {30, 261, 400, 24});
        drawStripe(g, 30, 288, 1020);
    }
    drawStripe(g, 19, getHeight() - 13, getWidth() - 38);
}
CX5MEditor::~CX5MEditor() { setLookAndFeel(nullptr); processor.stopVoicePreview(); }

void CX5MEditor::resized() {
    synthTab.setBounds(772, 41, 74, 33);
    librarianTab.setBounds(850, 41, 96, 33);
    editorTab.setBounds(950, 41, 112, 33);

    previousPatch.setBounds(388, 43, 30, 35);
    bank.setBounds(422, 43, 151, 35);
    nextPatch.setBounds(577, 43, 30, 35);
    algorithm.setBounds(617, 43, 125, 35);
    for (int i = 0; i < 16; ++i) {
        const int x = 24 + (i / 4) * 223 + (i % 4) * 50;
        controls[i].slider->setBounds(x, 145, 50, 68);
        controls[i].label->setBounds(x, 214, 50, 20);
    }
    controls[16].slider->setBounds(916, 145, 52, 68);
    controls[16].label->setBounds(916, 214, 52, 20);
    bendLabel.setBounds(971, 142, 84, 15);
    bendRange.setBounds(975, 158, 77, 22);
    modLabel.setBounds(971, 184, 84, 16);
    modDepth.setBounds(974, 202, 80, 28);

    bankStatus.setBounds(30, 297, 1017, 26);
    cassette.setBounds(30, 331, 234, 30);
    settings.setBounds(272, 331, 94, 30);
    folderStatus.setBounds(377, 331, 672, 30);

    sourceBankChoice.setBounds(30, 290, 464, 27);
    destinationBankLabel.setBounds(586, 290, 464, 27);
    sourceList.setBounds(30, 322, 464, 222);
    draftList.setBounds(586, 322, 464, 222);
    copyVoice.setBounds(511, 398, 58, 54);
    previousSource.setBounds(30, 549, 75, 27);
    nextSource.setBounds(111, 549, 75, 27);
    auditionSource.setBounds(30, 582, 185, 29);
    stopAudition.setBounds(222, 582, 180, 29);
    swapUp.setBounds(586, 549, 100, 27);
    swapDown.setBounds(692, 549, 112, 27);
    renameSlot.setBounds(810, 549, 130, 27);
    auditionDraft.setBounds(586, 582, 170, 29);
    loadDraft.setBounds(586, 616, 154, 31);
    newDraft.setBounds(746, 616, 104, 31);
    saveDraft.setBounds(856, 616, 194, 31);
    editorStatus.setBounds(30, 618, 464, 30);

    bankList.setBounds(30, 146, 464, 451);
    voiceList.setBounds(586, 146, 464, 451);
    rescanBanks.setBounds(30, 611, 190, 32);
    librarianInfo.setBounds(586, 609, 464, 34);
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
    const bool controlsVisible = !librarian;
    setSize(1080, synthVisible ? 395 : 680);
    synthTab.setToggleState(synthVisible, juce::dontSendNotification);
    librarianTab.setToggleState(librarian, juce::dontSendNotification);
    editorTab.setToggleState(bankEditorVisible, juce::dontSendNotification);
    bankList.setVisible(librarian);
    voiceList.setVisible(librarian);
    rescanBanks.setVisible(librarian);
    librarianInfo.setVisible(librarian);
    for (auto& c : controls) { c.slider->setVisible(controlsVisible); c.label->setVisible(controlsVisible); }
    algorithm.setVisible(controlsVisible);
    bank.setVisible(controlsVisible);
    bendRange.setVisible(controlsVisible);
    bendLabel.setVisible(controlsVisible);
    modDepth.setVisible(controlsVisible);
    modLabel.setVisible(controlsVisible);
    previousPatch.setVisible(controlsVisible);
    nextPatch.setVisible(controlsVisible);
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
    const bool active = selected && owner.auditionMode == AuditionMode::source;
    if (selected) g.fillAll(juce::Colour(active ? cyan : 0xff31454b));
    g.setColour(juce::Colour(active ? 0xff081014 : cream));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 15.f, juce::Font::plain));
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
    const bool active = selected && owner.auditionMode == AuditionMode::slot;
    if (selected) g.fillAll(juce::Colour(active ? cyan : 0xff31454b));
    g.setColour(juce::Colour(active ? 0xff081014 : cream));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 15.f, juce::Font::plain));
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
    const bool sourceActive = auditionMode == AuditionMode::source;
    const bool slotActive = auditionMode == AuditionMode::slot;
    auditionSource.setColour(juce::TextButton::buttonColourId, sourceActive ? cyan : inactive);
    auditionDraft.setColour(juce::TextButton::buttonColourId, slotActive ? cyan : inactive);
    auditionSource.setColour(juce::TextButton::textColourOffId,
                             juce::Colour(sourceActive ? 0xff081d20 : cream));
    auditionDraft.setColour(juce::TextButton::textColourOffId,
                            juce::Colour(slotActive ? 0xff081d20 : cream));
    sourceList.repaint();
    draftList.repaint();
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
