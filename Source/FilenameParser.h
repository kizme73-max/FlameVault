#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// C++ port of SampleManager.Services.FilenameParser v1.0.0 (deterministic, explainable, no audio, no AI).
namespace fv
{
enum class Strength { Low, Medium, High };

struct Evidence { juce::String token, field, value, ruleId; double score = 0; };

struct ParseResult
{
    juce::String version, suggestedCategory, suggestedSub, assignedCategory, form, key; // key e.g. "C# Minor" (filename LABEL only)
    juce::StringArray tags;
    int bpm = 0;                 // 0 = none. Filename LABEL only, never verified audio analysis.
    double score = 0;            // heuristic strength, not a calibrated probability
    Strength strength = Strength::Low;
    bool conflict = false;
    std::vector<Evidence> evidence;

    bool needsReview() const { return strength == Strength::Medium || conflict; }
};

ParseResult parseFilename (const juce::String& fileName);
}
