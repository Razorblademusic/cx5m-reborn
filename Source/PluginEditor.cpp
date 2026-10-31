#include "PluginEditor.h"
CX5MEditor::CX5MEditor(CX5MProcessor& p) : AudioProcessorEditor(&p), processor(p) {
    for (int i = 1; i <= 4; ++i) {
        auto s = juce::String(i);
        addControl("ratio" + s, "RATIO"); addControl("level" + s, "LEVEL");
        addControl("attack" + s, "ATTACK"); addControl("release" + s, "RELEASE");
    }
    addControl("output", "OUTPUT");
    algorithm.addItemList({"Chain", "Two pairs", "Three plus carrier", "Parallel"}, 1);
    addAndMakeVisible(algorithm);
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state, "algorithm", algorithm);
    cassette.setEnabled(false); addAndMakeVisible(cassette);
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
    g.drawText("4-OP FM  •  EARLY SYNTH PROTOTYPE  •  NOT YET YM2151-ACCURATE", 24, 54, 800, 24, juce::Justification::centredLeft);
    for (int i = 0; i < 4; ++i) {
        auto r = juce::Rectangle<float>(20.f + i * 210.f, 105.f, 198.f, 250.f);
        g.setColour(juce::Colour(0xff263342)); g.fillRoundedRectangle(r, 8.f);
        g.setColour(juce::Colour(0xff80c8dc)); g.setFont(17.f);
        g.drawText("OPERATOR " + juce::String(i + 1), r.toNearestInt().removeFromTop(32), juce::Justification::centred);
    }
}
void CX5MEditor::resized() {
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
    cassette.setBounds(480, 410, 345, 36);
}
