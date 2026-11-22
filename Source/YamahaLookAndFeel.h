#pragma once
#include <JuceHeader.h>
#include <cmath>
// Cosmetic-only JUCE rendering: no state, DSP, bank or MIDI changes.
class YamahaLookAndFeel final : public juce::LookAndFeel_V4 {
public:
 YamahaLookAndFeel() {
   setColour(juce::Label::textColourId, juce::Colour(0xffeee1c6));
   setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff15191a));
   setColour(juce::TextEditor::textColourId, juce::Colours::white);
   setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff73777e));
   setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff22252b));
   setColour(juce::PopupMenu::textColourId, juce::Colours::white);
   setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff008e91));
 }
 void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                       float start, float end, juce::Slider&) override {
   const float cx = (float)x + (float)w * 0.5f;
   const float cy = (float)y + (float)h * 0.5f;
   const float r = juce::jmin((float)w, (float)h) * 0.34f;
   if (r < 2.f) return;

   const auto pointOnDial = [cx, cy](float radius, float angle) {
     return juce::Point<float>(cx + std::sin(angle) * radius,
                               cy - std::cos(angle) * radius);
   };
   g.setColour(juce::Colour(0xffc5b99e));
   for (int tick = 0; tick <= 10; ++tick) {
     const auto angle = start + (end - start) * (float)tick / 10.f;
     const float length = tick % 5 == 0 ? 4.5f : 2.5f;
     g.drawLine({pointOnDial(r + 3.f, angle), pointOnDial(r + 3.f + length, angle)}, 0.9f);
   }

   const auto dial = juce::Rectangle<float>(2.f * r, 2.f * r).withCentre({cx, cy});
   g.setColour(juce::Colours::black.withAlpha(0.65f));
   g.fillEllipse(dial.expanded(1.f).translated(0.f, 2.f));
   g.setGradientFill(juce::ColourGradient(juce::Colour(0xff87817a), dial.getTopLeft(),
                                          juce::Colour(0xff111212), dial.getBottomRight(), false));
   g.fillEllipse(dial);
   g.setColour(juce::Colour(0xff1c1b1a));
   for (int ridge = 0; ridge < 36; ++ridge) {
     const auto angle = juce::MathConstants<float>::twoPi * (float)ridge / 36.f;
     g.drawLine({pointOnDial(r * 0.77f, angle), pointOnDial(r - 0.5f, angle)}, 0.8f);
   }
   g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5c5853), dial.getTopLeft(),
                                          juce::Colour(0xff222221), dial.getBottomRight(), false));
   g.fillEllipse(dial.reduced(r * 0.19f));

   const float angle = start + pos * (end - start);
   g.setColour(juce::Colour(0xffeee1c6));
   g.drawLine({pointOnDial(r * 0.42f, angle), pointOnDial(r * 0.81f, angle)}, 2.2f);
 }
 void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& bg,
                           bool hover,bool down) override {
   auto r=b.getLocalBounds().toFloat().reduced(1.f);
   auto top=down?juce::Colour(0xff17191d):(hover?juce::Colour(0xff57534d):juce::Colour(0xff44403c));
   auto bottom=down?juce::Colour(0xff34373d):juce::Colour(0xff262422);
   if(bg.getSaturation()>0.55f && bg.getBrightness()>0.55f){top=bg.brighter(0.12f);bottom=bg.darker(0.28f);}
   g.setGradientFill(juce::ColourGradient(top,r.getX(),r.getY(),bottom,r.getX(),r.getBottom(),false));
   g.fillRoundedRectangle(r,4.f);
   g.setColour(juce::Colour(0xffaaa093).withAlpha(0.75f));g.drawRoundedRectangle(r,4.f,1.f);
   g.setColour(juce::Colour(0xff101215));g.drawLine(r.getX()+4,r.getBottom()-2,r.getRight()-4,r.getBottom()-2,1.f);
 }
 void drawComboBox(juce::Graphics& g,int w,int h,bool, int,int,int,int,juce::ComboBox& box) override {
   auto r=juce::Rectangle<float>(0,0,(float)w,(float)h).reduced(0.5f);
   const auto face = box.findColour(juce::ComboBox::backgroundColourId);
   g.setGradientFill(juce::ColourGradient(face.brighter(0.16f), 0.f, 0.f,
                                          face.darker(0.3f), 0.f, (float)h, false));
   g.fillRoundedRectangle(r,3.f);g.setColour(juce::Colour(0xff85898f));g.drawRoundedRectangle(r,3.f,1.f);
   juce::Path arrow;arrow.addTriangle(w-21.f,h*0.43f,w-11.f,h*0.43f,w-16.f,h*0.60f);
   g.setColour(box.findColour(juce::ComboBox::arrowColourId));g.fillPath(arrow);
 }
};
