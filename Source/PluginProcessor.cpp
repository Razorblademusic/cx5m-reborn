#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "CassetteBank.h"
#include <algorithm>
#include <cmath>
#include <memory>

juce::AudioProcessorValueTreeState::ParameterLayout CX5MProcessor::makeParams() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1}, "Output", juce::NormalisableRange<float>(0.f,1.f), .35f));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"algorithm",1}, "Algorithm", juce::StringArray{"Algorithm 1","Algorithm 2","Algorithm 3","Algorithm 4","Algorithm 5","Algorithm 6","Algorithm 7","Algorithm 8"}, 7));
    juce::StringArray bankNames;
    bankNames.add("Prototype (v0.3)");
    for (int i=0;i<48;++i) {
        juce::String name;
        for(int j=0;j<7;++j) { auto c = cassetteBank::raw[static_cast<size_t>(i*64+j)]; if(c>=32 && c<=126) name += juce::String::charToString(static_cast<juce_wchar>(c)); }
        bankNames.add(juce::String(i+1).paddedLeft('0',2) + "  " + name.trim());
    }
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"bank",1}, "Cassette Voice", bankNames, 0));
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
void CX5MProcessor::write(uint8_t reg,uint8_t value) { chip.write_address(reg); chip.write_data(value); }
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
            write(static_cast<uint8_t>(0xe0+offset),0x04);
        }
    }
    lastBank = -1; activeBank = 0;
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
    // For recovered voices, use the original TL with a conservative velocity offset.
    for(int op=0;op<4;++op) {
        if(activeBank>0) {
            const size_t pos = static_cast<size_t>((activeBank-1)*64+16+op*8);
            int base = bankBytes()[pos] & 127;
            int sensitivity = (bankBytes()[pos+1] >> 4) & 7;
            int tl = juce::jlimit(0,127,base + static_cast<int>((1.f-velocity)*sensitivity*5.f));
            write(static_cast<uint8_t>(0x60+ch+op*8),static_cast<uint8_t>(tl));
            continue;
        }
        auto s=juce::String(op+1);
        float level=state.getRawParameterValue("level"+s)->load();
        int tl=juce::jlimit(0,127,static_cast<int>(std::round((1.f-level*velocity)*110.f)));
        write(static_cast<uint8_t>(0x60+ch+op*8),static_cast<uint8_t>(tl));
    }
    uint8_t mask = activeBank>0 ? static_cast<uint8_t>(bankBytes()[static_cast<size_t>((activeBank-1)*64+11)] & 0x78) : 0x78;
    write(0x08,static_cast<uint8_t>(mask|ch));
}
void CX5MProcessor::noteOff(int note) {
    int ch=-1;
    for(int i=0;i<8;++i) if(voices[i].note==note&&!voices[i].released&&(ch<0||voices[i].age<voices[ch].age)) ch=i;
    if(ch>=0) {voices[ch].released=true;voices[ch].releaseSamples=static_cast<int>(hostRate*3.0);write(0x08,static_cast<uint8_t>(ch));}
}
void CX5MProcessor::loadCassetteVoice(int index) {
    const size_t base = static_cast<size_t>((index-1)*64);
    const auto& r = bankBytes();
    // Mute existing notes before changing chip registers.
    for(int ch=0;ch<8;++ch) { write(0x08,static_cast<uint8_t>(ch)); voices[ch] = {}; }
    write(0x18,r[base+8]); // LFO frequency
    write(0x19,static_cast<uint8_t>(r[base+9]&0x7f)); // AMD
    write(0x19,static_cast<uint8_t>(0x80|(r[base+10]&0x7f))); // PMD
    write(0x1b,static_cast<uint8_t>((r[base+14]>>5)&3)); // LFO waveform
    for(int ch=0;ch<8;++ch) {
        write(static_cast<uint8_t>(0x20+ch),r[base+12]); // L/R feedback algorithm
        write(static_cast<uint8_t>(0x38+ch),r[base+13]); // PMS / AMS
        for(int op=0;op<4;++op) {
            const size_t o=base+16+static_cast<size_t>(op)*8;
            const uint8_t slot=static_cast<uint8_t>(ch+op*8);
            write(static_cast<uint8_t>(0x40+slot),r[o+3]&0x7f); // DT1/MUL
            write(static_cast<uint8_t>(0x60+slot),r[o]&0x7f); // TL
            write(static_cast<uint8_t>(0x80+slot),r[o+4]&0xdf); // KS/AR (bit 5 reserved)
            write(static_cast<uint8_t>(0xa0+slot),r[o+5]&0x9f); // AM enable / D1R
            write(static_cast<uint8_t>(0xc0+slot),r[o+6]&0x9f); // DT2 / D2R
            write(static_cast<uint8_t>(0xe0+slot),r[o+7]); // SL / RR
        }
    }
}
void CX5MProcessor::updateParameters() {
    int selected = juce::jlimit(0,48,static_cast<int>(state.getRawParameterValue("bank")->load()));
    if(selected != lastBank || seenImportRevision != importRevision.load(std::memory_order_acquire)) {
        seenImportRevision = importRevision.load(std::memory_order_relaxed);
        activeBank = selected; lastBank = selected;
        if(selected>0) loadCassetteVoice(selected);
        else { lastAlgorithm=-1; lastRatio.fill(-1); lastLevel.fill(-1); lastAttack.fill(-1); lastRelease.fill(-1); }
    }
    if(activeBank>0) return;

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
        int rr=4;
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
    // Take an immutable bank snapshot once per audio block; no disk I/O or locks here.
    currentAudioBank = std::atomic_load(&importedBank);
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
const CX5MProcessor::BankData& CX5MProcessor::bankBytes() const noexcept {
    return currentAudioBank ? *currentAudioBank : cassetteBank::raw;
}

bool CX5MProcessor::importVoiceBank(const juce::File& file, juce::String& error) {
    if (!file.existsAsFile() || file.getSize() != 3072) {
        error = "Select a decoded 48-voice bank (.bin, exactly 3072 bytes). WAV decoding is not yet supported.";
        return false;
    }
    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes) || bytes.getSize() != 3072) {
        error = "Could not read the bank file."; return false;
    }
    auto candidate = std::make_shared<BankData>();
    std::memcpy(candidate->data(), bytes.getData(), candidate->size());
    int plausible = 0;
    for (int i=0;i<48;++i) {
        const auto* r = candidate->data() + i*64;
        bool name = true;
        for (int j=0;j<7;++j) if (r[j] < 32 || r[j] > 126) name = false;
        if (name && r[11] == 0x78) ++plausible;
    }
    if (plausible < 40) {
        error = "This does not resemble a decoded CX5M 48-voice bank (expected printable names and Yamaha voice records).";
        return false;
    }
    std::shared_ptr<const BankData> immutable = candidate;
    std::atomic_store(&importedBank, immutable);
    importedBankName = file.getFileName();
    // Force reload of currently selected patch on the audio thread.
    // No change to parameters or chip state from the GUI thread.
    importRevision.fetch_add(1, std::memory_order_release);
    return true;
}

juce::String CX5MProcessor::getImportedVoiceName(int index) const {
    auto bank = std::atomic_load(&importedBank);
    const auto& data = bank ? *bank : cassetteBank::raw;
    juce::String name;
    if (index < 0 || index >= 48) return name;
    for (int j=0;j<7;++j) {
        auto c = data[static_cast<size_t>(index*64+j)];
        if (c>=32 && c<=126) name += juce::String::charToString(static_cast<juce_wchar>(c));
    }
    return name.trim();
}

bool CX5MProcessor::hasImportedBank() const noexcept {
    return static_cast<bool>(std::atomic_load(&importedBank));
}
juce::String CX5MProcessor::getBankDisplayName() const {
    return hasImportedBank() ? importedBankName : "Original Bank";
}
void CX5MProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree = state.copyState();
    if (auto bank = std::atomic_load(&importedBank)) {
        juce::ValueTree payload("CX5MImportedBank");
        payload.setProperty("format", 1, nullptr);
        payload.setProperty("filename", importedBankName, nullptr);
        payload.setProperty("bytes", juce::MemoryBlock(bank->data(), bank->size()).toBase64Encoding(), nullptr);
        tree.addChild(payload, -1, nullptr);
    }
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, dest);
}
void CX5MProcessor::setStateInformation(const void* data, int bytes) {
    auto xml = getXmlFromBinary(data, bytes);
    if (!xml || !xml->hasTagName(state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    auto payload = tree.getChildWithName("CX5MImportedBank");
    // Also accept project states written by the first v0.5 release.
    auto encoded = payload.isValid() ? payload.getProperty("bytes").toString()
                                     : tree.getProperty("importedBankBase64").toString();
    std::shared_ptr<const BankData> restored;
    if (encoded.isNotEmpty()) {
        juce::MemoryBlock block;
        if (block.fromBase64Encoding(encoded) && block.getSize() == 3072) {
            auto mutableBank = std::make_shared<BankData>();
            std::memcpy(mutableBank->data(), block.getData(), mutableBank->size());
            restored = mutableBank;
        }
    }
    std::atomic_store(&importedBank, restored);
    importedBankName = restored ? payload.getProperty("filename", "Imported Bank").toString() : juce::String();
    importRevision.fetch_add(1, std::memory_order_release);
    if (payload.isValid()) tree.removeChild(payload, nullptr);
    tree.removeProperty("importedBankBase64", nullptr);
    state.replaceState(tree);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new CX5MProcessor();}
