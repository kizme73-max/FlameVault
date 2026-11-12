#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "SampleLibrary.h"

/** Sample browser with a built-in preview player. The preview audio goes to the plugin's output,
    so it is heard through the DAW mixer channel the plugin is loaded on. */
class FlameVaultProcessor : public juce::AudioProcessor
{
public:
    FlameVaultProcessor();
    ~FlameVaultProcessor() override;

    // preview API (message thread)
    bool loadAndPlay (const juce::File& file, const juce::String& id);
    void pausePlayback();
    void stopPlayback();
    void seekFraction (double f);
    void setLooping (bool shouldLoop);
    void setVolume (float v)                       { volume.store (v); }
    void setRate (double pitchSemitones, double tempoPercent);

    struct PlaybackState { juce::String id; double position = 0, length = 0; bool playing = false; };
    PlaybackState getPlaybackState();

    SampleLibrary& getLibrary() { return library; }

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "FlameVault"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    SampleLibrary library;
    juce::AudioFormatManager formats;
    juce::TimeSliceThread diskThread { "FlameVault disk reader" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::ResamplingAudioSource> resampler;
    juce::CriticalSection lock;
    juce::String currentId;
    std::atomic<float> volume { 0.8f };
    bool looping = false, monoFile = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlameVaultProcessor)
};
