#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

/** Hosts the FLAMEVAULT dashboard (UI/index.html) in a WebView2 and bridges it to the C++ engine. */
class FlameVaultEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FlameVaultEditor (FlameVaultProcessor&);
    ~FlameVaultEditor() override;
    void resized() override;

private:
    juce::WebBrowserComponent::Options makeOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    void timerCallback() override;

    FlameVaultProcessor& proc;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::WebBrowserComponent web;
    juce::String lastSent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlameVaultEditor)
};
