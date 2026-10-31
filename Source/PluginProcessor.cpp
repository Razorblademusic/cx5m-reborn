#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout CX5MProcessor::makeParams() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1}, "Output", juce::NormalisableRange<float>(0.f,1.f), .35f));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"algorithm",1}, "Algorithm", juce::StringArray{"Algorithm 1","Algorithm 2","Algorithm 3","Algorithm 4","Algorithm 5","Algorithm 6","Algorithm 7","Algorithm 8"}, 7));
    for (int i=1;i<=4;++i) {
        auto n=juce::String(i);
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"ratio"+n,1},"OP"+n+" Ratio",juce::NormalisableRange<float>(.5f,16.f,.5f),1.f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"level"+n,1},"OP"+n+" Level",juce::NormalisableRange<float>(0.f,1.f),i==4?.8f:.45f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack"+n,1},"OP"+n+" Attack",juce::NormalisableRange<float>(.001f,2.f,.001f,.35f),.015f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release"+n,1},"OP"+n+" Release",juce::NormalisableRange<float>(.02f,5.f,.001f,.4f),.35f));
    }
    return p;
}
CX5MProcessor::CX5MProcessor() : AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)), state(*this,nullptr,"CX5MState",makeParams()), chip(interface) {}
void CX5MProcessor::write(uint8_t reg, uint8_t value)
{
    chip.write_address(reg);
    chip.write_data(value);
}
void CX5MProcessor::prepareToPlay(double sr,int) {
    hostRate=std::max(1.0,sr);
    chip.reset();
    nativeRate=static_cast<double>(chip.sample_rate(3579545));
    fraction=0.0; prev={}; next={}; voices={}; counter=0; bend=8192;
    lastRatio.fill(-1); lastLevel.fill(-1); lastAttack.fill(-1); lastRelease.fill(-1); lastAlgorithm=-1;
    write(0x18,0); write(0x19,0); write(0x1b,0);
    for(int ch=0;ch<8;++ch) {
        write(static_cast<uint8_t>(0x20+ch),0xc7); // both outputs, algorithm 7
        write(static_cast<uint8_t>(0x38+ch),0);
        for(int op=0;op<4;++op) {
            int offset=ch+op*8;
            write(static_cast<uint8_t>(0x40+offset),1);
            write(static_cast<uint8_t>(0x60+offset),127);
            write(static_cast<uint8_t>(0x80+offset),31);
            write(static_cast<uint8_t>(0xa0+offset),0);
            write(static_cast<uint8_t>(0xc0+offset),0);
            write(static_cast<uint8_t>(0xe0+offset),0x4f);
        }
    }
    updateParameters(); tick(); prev=next;
}
void CX5MProcessor::setPitch(int ch,int note) {
    // YM2151 KC: octave in bits 4-6, semitone code with gaps at E/A.
    constexpr int code[12]={0,1,2,4,5,6,8,9,10,12,13,14};
    int n=juce::jlimit(0,95,note);
    int octave=juce::jlimit(0,7,n/12-1);
    int semitone=n%12;
    write(static_cast<uint8_t>(0x28+ch),static_cast<uint8_t>((octave<<4)|code[semitone]));
    // Fractional semitone pitch bend, 2-semitone range.
    double semis=(bend-8192)/8192.0*2.0;
    int fractionValue=static_cast<int>(std::round(semis*64.0));
    int fine=juce::jlimit(0,63,fractionValue);
    // Negative bend requires shifting the key code downward.
    if(fractionValue<0) {
        int bent=juce::jlimit(0,95,n+static_cast<int>(std::floor(semis)));
        int oct=juce::jlimit(0,7,bent/12-1);
        write(static_cast<uint8_t>(0x28+ch),static_cast<uint8_t>((oct<<4)|code[bent%12]));
        fine=juce::jlimit(0,63,static_cast<int>(std::round((semis-std::floor(semis))*64)));
    }
    write(static_cast<uint8_t>(0x30+ch),static_cast<uint8_t>(fine<<2));
}
void CX5MProcessor::noteOn(int note,float velocity) {
    int ch=-1;
    for(int i=0;i<8;++i) if(voices[i].note<0) {ch=i;break;}
    if(ch<0) for(int i=0;i<8;++i) if(voices[i].released) {ch=i;break;}
    if(ch<0) {ch=0;for(int i=1;i<8;++i) if(voices[i].age<voices[ch].age) ch=i;}
    write(0x08,static_cast<uint8_t>(ch));
    voices[ch]={note,false,++counter,velocity,0};
    setPitch(ch,note);
    // Velocity controls chip TL (not audio amplitude), set before key on.
    for(int op=0;op<4;++op) {
        auto s=juce::String(op+1);
        float level=state.getRawParameterValue("level"+s)->load();
        int tl=juce::jlimit(0,127,static_cast<int>(std::round((1.f-level*velocity)*110.f)));
        write(static_cast<uint8_t>(0x60+ch+op*8),static_cast<uint8_t>(tl));
    }
    write(0x08,static_cast<uint8_t>(0x78|ch));
}
void CX5MProcessor::noteOff(int note) {
    int ch=-1;
    for(int i=0;i<8;++i) if(voices[i].note==note&&!voices[i].released&&(ch<0||voices[i].age<voices[ch].age)) ch=i;
    if(ch>=0) {voices[ch].released=true;voices[ch].releaseSamples=static_cast<int>(hostRate*3.0);write(0x08,static_cast<uint8_t>(ch));}
}
void CX5MProcessor::updateParameters() {
    int alg=juce::jlimit(0,7,static_cast<int>(state.getRawParameterValue("algorithm")->load()));
    if(alg!=lastAlgorithm) {for(int ch=0;ch<8;++ch) write(static_cast<uint8_t>(0x20+ch),static_cast<uint8_t>(0xc0|alg));lastAlgorithm=alg;}
    for(int op=0;op<4;++op) {
        auto s=juce::String(op+1);
        int ratio=juce::jlimit(1,15,static_cast<int>(std::round(state.getRawParameterValue("ratio"+s)->load())));
        int level=juce::jlimit(0,127,static_cast<int>(std::round((1.f-state.getRawParameterValue("level"+s)->load())*110.f)));
        // Prototype time-to-register approximation; NOT original Yamaha voicing mapping.
        double attack=state.getRawParameterValue("attack"+s)->load();
        double release=state.getRawParameterValue("release"+s)->load();
        int ar=juce::jlimit(1,31,static_cast<int>(std::round(31-11*std::log1p(attack*10))));
        int rr = 4;
        for(int ch=0;ch<8;++ch) {
            auto o=static_cast<uint8_t>(ch+op*8);
            if(ratio!=lastRatio[op]) write(static_cast<uint8_t>(0x40+o),static_cast<uint8_t>(ratio));
            if(level!=lastLevel[op] && voices[ch].note<0) write(static_cast<uint8_t>(0x60+o),static_cast<uint8_t>(level));
            if(ar!=lastAttack[op]) write(static_cast<uint8_t>(0x80+o),static_cast<uint8_t>(ar));
            if(rr!=lastRelease[op]) write(static_cast<uint8_t>(0xe0+o),static_cast<uint8_t>(rr));
        }
        lastRatio[op]=ratio;lastLevel[op]=level;lastAttack[op]=ar;lastRelease[op]=rr;
    }
}
void CX5MProcessor::tick() {
    ymfm::ym2151::output_data out{};
    chip.generate(&out);
    next[0]=static_cast<double>(out.data[0])/32768.0;
    next[1]=static_cast<double>(out.data[1])/32768.0;
}
void CX5MProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals guard;
    buffer.clear();
    updateParameters();
    auto it=midi.cbegin();
    double step=nativeRate/hostRate;
    float gain=state.getRawParameterValue("output")->load()*.5f;
    for(int i=0;i<buffer.getNumSamples();++i) {
        while(it!=midi.cend() && (*it).samplePosition<=i) {
            auto m=(*it).getMessage();
            if(m.isNoteOn()) noteOn(m.getNoteNumber(),m.getFloatVelocity());
            else if(m.isNoteOff()) noteOff(m.getNoteNumber());
            else if(m.isPitchWheel()) {bend=m.getPitchWheelValue();for(int ch=0;ch<8;++ch) if(voices[ch].note>=0) setPitch(ch,voices[ch].note);}
            else if(m.isAllSoundOff()||m.isAllNotesOff()) {for(int ch=0;ch<8;++ch) {write(0x08,static_cast<uint8_t>(ch));voices[ch]={};}}
            ++it;
        }
        fraction+=step;
        while(fraction>=1.0) {prev=next;tick();fraction-=1.0;}
        // Released voices are reusable after 3 seconds; YM2151 envelopes continue on chip.
        for(auto& v:voices) if(v.released && --v.releaseSamples<=0) v={};
        for(int ch=0;ch<buffer.getNumChannels();++ch)
            buffer.setSample(ch,i,static_cast<float>((prev[std::min(ch,1)]*(1.0-fraction)+next[std::min(ch,1)]*fraction)*gain));
    }
}
juce::AudioProcessorEditor* CX5MProcessor::createEditor(){return new CX5MEditor(*this);}
void CX5MProcessor::getStateInformation(juce::MemoryBlock& dest){auto xml=state.copyState().createXml();copyXmlToBinary(*xml,dest);}
void CX5MProcessor::setStateInformation(const void* data,int bytes){auto xml=getXmlFromBinary(data,bytes);if(xml&&xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new CX5MProcessor();}
