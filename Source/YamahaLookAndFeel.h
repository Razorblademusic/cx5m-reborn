#pragma once
#include <JuceHeader.h>
#include <cmath>
// Cosmetic-only JUCE rendering: no state, DSP, bank or MIDI changes.
class YamahaLookAndFeel final : public juce::LookAndFeel_V4 {
public:
 YamahaLookAndFeel() {
   setColour(juce::Label::textColourId, juce::Colour(0xffe2e3e4));
   setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff20242a));
   setColour(juce::TextEditor::textColourId, juce::Colours::white);
   setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff73777e));
   setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff22252b));
   setColour(juce::PopupMenu::textColourId, juce::Colours::white);
   setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff008e91));
 }
 void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                       float start, float end, juce::Slider& slider) override {
   const auto teal=juce::Colour(0xff0fd1c4);
   const float size=juce::jmin((float)w,(float)h)*0.67f;
   const float cx=x+w*0.5f, cy=y+h*0.5f;
   const float r=size*0.5f, angle=start+pos*(end-start);
   juce::Path track, active;
   track.addCentredArc(cx,cy,r+4,r+4,0,start,end,true);
   active.addCentredArc(cx,cy,r+4,r+4,0,start,angle,true);
   g.setColour(juce::Colour(0xff090e13));g.strokePath(track,juce::PathStrokeType(5.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
   g.setColour(teal.withAlpha(0.90f));g.strokePath(active,juce::PathStrokeType(4.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
   auto knob=juce::Rectangle<float>(cx-r,cy-r,size,size);
   g.setGradientFill(juce::ColourGradient(juce::Colour(0xff777b81),cx-r,cy-r,juce::Colour(0xff111216),cx+r,cy+r,false));
   g.fillEllipse(knob);
   g.setColour(juce::Colour(0xff0c0e12));g.drawEllipse(knob,2.f);
   auto inner=knob.reduced(4.f);
   g.setGradientFill(juce::ColourGradient(juce::Colour(0xff46494f),cx-r,cy-r,juce::Colour(0xff17191d),cx+r,cy+r,false));
   g.fillEllipse(inner);
   g.setColour(juce::Colours::white.withAlpha(0.13f));
   g.drawEllipse(inner.reduced(1.f),0.9f);
   g.setColour(juce::Colour(0xff090b0e).withAlpha(0.65f));
   g.drawEllipse(inner.reduced(2.f),0.7f);
   const auto a=angle-juce::MathConstants<float>::halfPi;
   g.setColour(teal.brighter(0.3f));
   g.drawLine(cx+std::cos(a)*r*0.25f,cy+std::sin(a)*r*0.25f,
              cx+std::cos(a)*r*0.75f,cy+std::sin(a)*r*0.75f,2.4f);
 }
 void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& bg,
                           bool hover,bool down) override {
   auto r=b.getLocalBounds().toFloat().reduced(1.f);
   auto top=down?juce::Colour(0xff17191d):(hover?juce::Colour(0xff60636a):juce::Colour(0xff4b4e55));
   auto bottom=down?juce::Colour(0xff34373d):juce::Colour(0xff24262b);
   if(bg.getSaturation()>0.3f && bg.getBrightness()>0.32f){top=bg.brighter(0.12f);bottom=bg.darker(0.28f);}
   g.setGradientFill(juce::ColourGradient(top,r.getX(),r.getY(),bottom,r.getX(),r.getBottom(),false));
   g.fillRoundedRectangle(r.translated(0.f,1.2f),4.f);
   g.setColour(juce::Colours::white.withAlpha(down?0.06f:0.16f));
   g.drawLine(r.getX()+5.f,r.getY()+1.5f,r.getRight()-5.f,r.getY()+1.5f,1.f);
   g.setColour(juce::Colour(0xff92959a).withAlpha(0.75f));g.drawRoundedRectangle(r,4.f,1.f);
   g.setColour(juce::Colour(0xff101215));g.drawLine(r.getX()+4,r.getBottom()-2,r.getRight()-4,r.getBottom()-2,1.f);
 }
 void drawComboBox(juce::Graphics& g,int w,int h,bool, int,int,int,int,juce::ComboBox&) override {
   auto r=juce::Rectangle<float>(0,0,(float)w,(float)h).reduced(0.5f);
   g.setGradientFill(juce::ColourGradient(juce::Colour(0xff272b31),0,0,juce::Colour(0xff14181d),0,(float)h,false));
   g.fillRoundedRectangle(r,3.f);g.setColour(juce::Colour(0xff85898f));g.drawRoundedRectangle(r,3.f,1.f);
   juce::Path arrow;arrow.addTriangle(w-21.f,h*0.43f,w-11.f,h*0.43f,w-16.f,h*0.60f);
   g.setColour(juce::Colour(0xff11c9c1));g.fillPath(arrow);
 }
};
