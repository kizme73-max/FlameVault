#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <vector>
#include "FilenameParser.h"

struct SampleRecord
{
    juce::String id, path, name, ext, category, subcategory, form, genres, key, description;
    int bpm = 0, channels = 0;
    double durationSec = 0, sampleRate = 0;
    float score = 0;
    bool favorite = false, needsReview = false;
    std::vector<float> peaks;      // cached, computed from the real audio file on demand
};

/** Folder scanner + persistent library (JSON in %APPDATA%/FlameVault/library.json).
    Public methods are called from the message thread; scanning runs on its own thread. */
class SampleLibrary : private juce::Thread
{
public:
    SampleLibrary();
    ~SampleLibrary() override;

    juce::var toVar();                               // { samples:[...], folders:[...] }
    void addFolder (const juce::File& folder);
    void rescan();
    bool toggleFavorite (const juce::String& id);
    juce::File fileFor (const juce::String& id);
    juce::var peaksFor (const juce::String& id);     // 240 normalised peaks read from the audio file

    std::function<void (juce::var)> onScan;          // { scanning, done, total } (message thread)
    std::function<void()> onChanged;                 // library content changed   (message thread)

private:
    void run() override;
    void load();
    void save();
    static juce::var recordToVar (const SampleRecord&);
    static juce::File storeFile();

    juce::CriticalSection lock;
    std::vector<SampleRecord> records;
    juce::StringArray folders;
    juce::AudioFormatManager peakFormats;

    JUCE_DECLARE_WEAK_REFERENCEABLE (SampleLibrary)
};
