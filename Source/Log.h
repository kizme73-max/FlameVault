#pragma once
#include <juce_core/juce_core.h>

// Tiny file log: %APPDATA%\FlameVault\log.txt  (the last line shows where a crash/hang happened)
inline void FvLog (const juce::String& msg)
{
    static juce::CriticalSection cs;
    const juce::ScopedLock sl (cs);
    auto f = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("FlameVault").getChildFile ("log.txt");
    f.getParentDirectory().createDirectory();
    if (f.getSize() > 512 * 1024) f.deleteFile();
    f.appendText (juce::Time::getCurrentTime().formatted ("%H:%M:%S  ") + msg + "\n");
}
