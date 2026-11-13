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
    bool favorite = false, needsReview = false, missing = false;
    std::vector<float> peaks;      // cached, computed from the real audio file on demand
};

/** Folder scanner + persistent library (JSON in %APPDATA%/FlameVault/library.json).
    Everything heavy (loading the JSON, scanning, building the page JSON, reading peaks, saving)
    runs on background threads so the DAW's UI thread never blocks. */
class SampleLibrary : private juce::Thread
{
public:
    SampleLibrary();
    ~SampleLibrary() override;

    juce::String libraryJson();                      // cached JSON text: { samples:[...], folders:[...] }
    void addFolder (const juce::File& folder);
    void rescan();
    bool toggleFavorite (const juce::String& id);
    juce::File fileFor (const juce::String& id);
    void peaksAsync (const juce::String& id, std::function<void (juce::var)> done); // done() runs on the message thread

    std::function<void (juce::var)> onScan;          // { scanning, done, total } (message thread)
    std::function<void()> onChanged;                 // library content changed   (message thread)

private:
    void run() override;
    void scanOnce();
    void load();
    void save();
    void persistAsync();
    void rebuildCache();
    juce::String buildJson();
    juce::var computePeaks (const juce::String& id);
    static juce::var recordToVar (const SampleRecord&);
    static juce::File storeFile();

    juce::CriticalSection lock, saveLock;
    std::vector<SampleRecord> records;
    juce::StringArray folders;
    juce::String cacheJson;
    std::atomic<bool> loaded { false }, doScan { false };

    JUCE_DECLARE_WEAK_REFERENCEABLE (SampleLibrary)
private:
    juce::ThreadPool pool { 1 };                     // declared last: destroyed first
};
