#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Log.h"

static void fvCrashHandler (void*) { FvLog ("CRASH\n" + juce::SystemStats::getStackBacktrace()); }

FlameVaultProcessor::FlameVaultProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    if (wrapperType == wrapperType_Standalone) juce::SystemStats::setApplicationCrashHandler (fvCrashHandler);
    FvLog ("processor: created");
    formats.registerBasicFormats();
    diskThread.startThread();
    resampler = std::make_unique<juce::ResamplingAudioSource> (&transport, false, 2);
}

FlameVaultProcessor::~FlameVaultProcessor()
{
    FvLog ("processor: destroying");
    transport.stop();
    transport.setSource (nullptr);
    diskThread.stopThread (2000);
}

bool FlameVaultProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void FlameVaultProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    FvLog ("processor: prepareToPlay " + juce::String (sampleRate) + " Hz, block " + juce::String (samplesPerBlock));
    transport.prepareToPlay (samplesPerBlock, sampleRate);
    resampler->prepareToPlay (samplesPerBlock, sampleRate);
}

bool FlameVaultProcessor::loadAndPlay (const juce::File& file, const juce::String& id)
{
    if (id == currentId && readerSource != nullptr)           // same file: resume / restart
    {
        if (transport.hasStreamFinished()) transport.setPosition (0.0);
        transport.start();
        return true;
    }

    std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (file) };
    if (reader == nullptr) return false;

    const double sr = reader->sampleRate;
    monoFile = reader->numChannels == 1;
    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    newSource->setLooping (looping);

    {
        const juce::ScopedLock sl (lock);                      // audio thread outputs silence while swapping
        transport.stop();
        transport.setSource (nullptr);
        readerSource = std::move (newSource);
        transport.setSource (readerSource.get(), 32768, &diskThread, sr, 2);
        currentId = id;
        transport.setPosition (0.0);
        transport.start();
    }
    return true;
}

void FlameVaultProcessor::pausePlayback() { transport.stop(); }
void FlameVaultProcessor::stopPlayback()  { transport.stop(); transport.setPosition (0.0); }

void FlameVaultProcessor::seekFraction (double f)
{
    const auto len = transport.getLengthInSeconds();
    if (len > 0.0) transport.setPosition (juce::jlimit (0.0, 1.0, f) * len);
}

void FlameVaultProcessor::setLooping (bool b)
{
    looping = b;
    if (readerSource != nullptr) readerSource->setLooping (b);
}

void FlameVaultProcessor::setRate (double pitchSemitones, double tempoPercent)
{
    // Pitch and tempo share one resampling ratio (varispeed). Independent time-stretch is a later step.
    resampler->setResamplingRatio (std::pow (2.0, pitchSemitones / 12.0) * tempoPercent / 100.0);
}

FlameVaultProcessor::PlaybackState FlameVaultProcessor::getPlaybackState()
{
    return { currentId, transport.getCurrentPosition(), transport.getLengthInSeconds(), transport.isPlaying() };
}

void FlameVaultProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    buffer.clear();

    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || readerSource == nullptr) return;

    juce::AudioSourceChannelInfo info (&buffer, 0, buffer.getNumSamples());
    resampler->getNextAudioBlock (info);

    if (monoFile && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    buffer.applyGain (volume.load());
}

juce::AudioProcessorEditor* FlameVaultProcessor::createEditor() { return new FlameVaultEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FlameVaultProcessor(); }
