#include "PluginEditor.h"
#include "UIData.h"

using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;
using Args = juce::Array<juce::var>;

// Events must carry a non-void payload, otherwise the page cannot parse them.
static juce::var pingVar() { auto* o = new juce::DynamicObject(); o->setProperty ("t", 1); return juce::var (o); }

FlameVaultEditor::FlameVaultEditor (FlameVaultProcessor& p)
    : AudioProcessorEditor (&p), proc (p), web (makeOptions())
{
    addAndMakeVisible (web);

    proc.getLibrary().onChanged = [this] { web.emitEventIfBrowserIsVisible ("library", pingVar()); };
    proc.getLibrary().onScan    = [this] (juce::var v) { web.emitEventIfBrowserIsVisible ("scan", v); };

    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (1100, 640, 3200, 1800);
    setSize (1500, 900);
    startTimerHz (30);
}

FlameVaultEditor::~FlameVaultEditor()
{
    stopTimer();
    proc.getLibrary().onChanged = nullptr;
    proc.getLibrary().onScan = nullptr;
}

void FlameVaultEditor::resized() { web.setBounds (getLocalBounds()); }

void FlameVaultEditor::timerCallback()
{
    const auto s = proc.getPlaybackState();
    const auto key = s.id + juce::String (s.position, 3) + (s.playing ? "1" : "0");
    if (key == lastSent) return;                           // only talk to the page when something changed
    lastSent = key;

    auto* o = new juce::DynamicObject();
    o->setProperty ("id", s.id);
    o->setProperty ("pos", s.position);
    o->setProperty ("dur", s.length);
    o->setProperty ("playing", s.playing);
    web.emitEventIfBrowserIsVisible ("playback", juce::var (o));
}

static juce::String mimeFor (const juce::String& name)
{
    if (name.endsWithIgnoreCase (".html")) return "text/html";
    if (name.endsWithIgnoreCase (".png"))  return "image/png";
    if (name.endsWithIgnoreCase (".js"))   return "text/javascript";
    if (name.endsWithIgnoreCase (".css"))  return "text/css";
    return "application/octet-stream";
}

std::optional<juce::WebBrowserComponent::Resource> FlameVaultEditor::getResource (const juce::String& url)
{
    auto name = url.upToFirstOccurrenceOf ("?", false, false).fromLastOccurrenceOf ("/", false, false);
    if (name.isEmpty()) name = "index.html";                // assets are matched by file name ("Assets/x.png" -> "x.png")

    for (int i = 0; i < UIData::namedResourceListSize; ++i)
    {
        const char* res = UIData::namedResourceList[i];
        if (juce::String (UIData::getNamedResourceOriginalFilename (res)) == name)
        {
            int size = 0;
            const auto* data = UIData::getNamedResource (res, size);
            std::vector<std::byte> bytes (reinterpret_cast<const std::byte*> (data),
                                          reinterpret_cast<const std::byte*> (data) + size);
            return juce::WebBrowserComponent::Resource { std::move (bytes), mimeFor (name) };
        }
    }
    return std::nullopt;
}

juce::WebBrowserComponent::Options FlameVaultEditor::makeOptions()
{
    using Opt = juce::WebBrowserComponent::Options;

    return Opt{}
        .withBackend (Opt::Backend::webview2)
        .withWinWebView2Options (Opt::WinWebView2{}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("FlameVaultWebView")))
        .withNativeIntegrationEnabled()
        .withResourceProvider ([this] (const juce::String& url) { return getResource (url); })

        .withNativeFunction ("getLibrary", [this] (const Args&, Completion done) { done (proc.getLibrary().libraryJson()); })

        .withNativeFunction ("addFolder", [this] (const Args&, Completion done)
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a sample folder", juce::File(), "");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto dir = fc.getResult();
                                      if (dir.isDirectory()) proc.getLibrary().addFolder (dir);
                                  });
            done (juce::var());
        })

        .withNativeFunction ("rescan", [this] (const Args&, Completion done) { proc.getLibrary().rescan(); done (juce::var()); })

        .withNativeFunction ("play", [this] (const Args& a, Completion done)
        {
            const auto id = a[0].toString();
            const auto f = proc.getLibrary().fileFor (id);
            done (f.existsAsFile() && proc.loadAndPlay (f, id));
        })
        .withNativeFunction ("pause",  [this] (const Args&, Completion done) { proc.pausePlayback(); done (juce::var()); })
        .withNativeFunction ("stop",   [this] (const Args&, Completion done) { proc.stopPlayback();  done (juce::var()); })
        .withNativeFunction ("seek",   [this] (const Args& a, Completion done) { proc.seekFraction ((double) a[0]); done (juce::var()); })
        .withNativeFunction ("setLoop",   [this] (const Args& a, Completion done) { proc.setLooping ((bool) a[0]); done (juce::var()); })
        .withNativeFunction ("setVolume", [this] (const Args& a, Completion done) { proc.setVolume ((float) (double) a[0]); done (juce::var()); })
        .withNativeFunction ("setRate",   [this] (const Args& a, Completion done) { proc.setRate ((double) a[0], (double) a[1]); done (juce::var()); })

        .withNativeFunction ("toggleFavorite", [this] (const Args& a, Completion done) { done (proc.getLibrary().toggleFavorite (a[0].toString())); })
        .withNativeFunction ("getPeaks", [this] (const Args& a, Completion done)
        {
            juce::Component::SafePointer<FlameVaultEditor> safe (this);
            proc.getLibrary().peaksAsync (a[0].toString(), [safe, done] (juce::var v) { if (safe != nullptr) done (v); });
        })

        // Drag a sample out of the plugin window into the DAW (FL Studio playlist / Sampler / Edison ...).
        .withNativeFunction ("startDrag", [this] (const Args& a, Completion done)
        {
            const auto f = proc.getLibrary().fileFor (a[0].toString());
            if (f.existsAsFile())
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
            done (juce::var());
        });
}
