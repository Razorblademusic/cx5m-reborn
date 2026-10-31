#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

juce::AudioProcessorValueTreeState::ParameterLayout CX5MProcessor::makeParams() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output", 1}, "Output", juce::NormalisableRange<float>(0.f, 1.f), .35f));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"algorithm", 1}, "Algorithm (prototype)", juce::StringArray{"Chain", "Two pairs", "Three plus carrier", "Parallel"}, 0));
    for (int i = 0; i < 4; ++i) {
        auto n = juce::String(i + 1);
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"ratio" + n, 1}, "OP" + n + " Ratio", juce::NormalisableRange<float>(.5f, 16.f, .5f), i == 3 ? 1.f : 2.f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"level" + n, 1}, "OP" + n + " Level", juce::NormalisableRange<float>(0.f, 1.f), i == 3 ? .8f : .45f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack" + n, 1}, "OP" + n + " Attack", juce::NormalisableRange<float>(.001f, 2.f, .001f, .35f), .015f));
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release" + n, 1}, "OP" + n + " Release", juce::NormalisableRange<float>(.02f, 5.f, .001f, .4f), .35f));
    }
    return p;
}
CX5MProcessor::CX5MProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)), state(*this, nullptr, "CX5MState", makeParams()) {}
void CX5MProcessor::prepareToPlay(double sr, int) { sampleRateHz = sr; voices = {}; counter = 0; bend = 8192; }
void CX5MProcessor::noteOn(int note, float velocity) {
    auto it = std::find_if(voices.begin(), voices.end(), [](const Voice& v){ return v.note < 0; });
    if (it == voices.end()) it = std::min_element(voices.begin(), voices.end(), [](const Voice& a, const Voice& b){ return a.age < b.age; });
    *it = Voice{}; it->note = note; it->velocity = velocity; it->age = ++counter;
}
void CX5MProcessor::noteOff(int note) {
    // One note-off releases one matching held note, not all overlapping instances.
    Voice* oldestHeld = nullptr;
    for (auto& v : voices)
        if (v.note == note && !v.released && (!oldestHeld || v.age < oldestHeld->age))
            oldestHeld = &v;
    if (oldestHeld) oldestHeld->released = true;
}
float CX5MProcessor::render(Voice& v, const Params& p) {
    if (v.note < 0) return 0.f;
    const double semitones = (bend - 8192) / 8192.0 * 2.0;
    const double hz = 440.0 * std::pow(2.0, (v.note - 69 + semitones) / 12.0);
    for (int i = 0; i < 4; ++i) {
        const double coeff = v.released ? p.releaseCoeff[i] : p.attackCoeff[i];
        v.env[i] = v.released ? v.env[i] * coeff : 1.0 - (1.0 - v.env[i]) * coeff;
        v.phase[i] += juce::MathConstants<double>::twoPi * hz * p.ratio[i] / sampleRateHz;
        if (v.phase[i] >= juce::MathConstants<double>::twoPi)
            v.phase[i] = std::fmod(v.phase[i], juce::MathConstants<double>::twoPi);
    }
    auto sine = [&](int i, double mod) { return std::sin(v.phase[i] + mod) * p.level[i] * v.env[i]; };
    double result = 0.0;
    switch (p.algorithm) {
        case 0: result = sine(3, sine(2, sine(1, sine(0, 0) * 5) * 5) * 5); break;
        case 1: result = .5 * (sine(1, sine(0, 0) * 5) + sine(3, sine(2, 0) * 5)); break;
        case 2: result = sine(3, (sine(0, 0) + sine(1, 0) + sine(2, 0)) * 3); break;
        default: result = .25 * (sine(0, 0) + sine(1, 0) + sine(2, 0) + sine(3, 0)); break;
    }
    if (v.released && v.env[0] < .0001 && v.env[1] < .0001 && v.env[2] < .0001 && v.env[3] < .0001)
        v.note = -1;
    return static_cast<float>(result * v.velocity);
}
void CX5MProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    Params p;
    // Snapshot parameter values once per block instead of allocating strings and
    // looking up parameters thousands of times per second on the audio thread.
    for (int i = 0; i < 4; ++i) {
        const auto suffix = juce::String(i + 1);
        p.ratio[i] = state.getRawParameterValue("ratio" + suffix)->load();
        p.level[i] = state.getRawParameterValue("level" + suffix)->load();
        const double attack = state.getRawParameterValue("attack" + suffix)->load();
        const double release = state.getRawParameterValue("release" + suffix)->load();
        p.attackCoeff[i] = std::exp(-1.0 / (sampleRateHz * attack));
        p.releaseCoeff[i] = std::exp(-1.0 / (sampleRateHz * release));
    }
    p.algorithm = static_cast<int>(state.getRawParameterValue("algorithm")->load());
    p.output = state.getRawParameterValue("output")->load();
    auto iter = midi.cbegin();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        while (iter != midi.cend() && (*iter).samplePosition <= sample) {
            auto m = (*iter).getMessage();
            if (m.isNoteOn()) noteOn(m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) noteOff(m.getNoteNumber());
            else if (m.isPitchWheel()) bend = m.getPitchWheelValue();
            else if (m.isAllNotesOff() || m.isAllSoundOff()) voices = {};
            ++iter;
        }
        float value = 0.f;
        for (auto& voice : voices) value += render(voice, p);
        value *= p.output * .25f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, sample, value);
    }
}
juce::AudioProcessorEditor* CX5MProcessor::createEditor() { return new CX5MEditor(*this); }
void CX5MProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto xml = state.copyState().createXml(); copyXmlToBinary(*xml, dest);
}
void CX5MProcessor::setStateInformation(const void* data, int bytes) {
    auto xml = getXmlFromBinary(data, bytes);
    if (xml && xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CX5MProcessor();
}
