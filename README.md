# FlameVault — AI Sample Manager (VST3)

JUCE 8 plugin (VST3 + Standalone, Windows first). The FLAMEVAULT dashboard runs in a WebView2 inside the plugin
window; C++ does the real work: folder scanning, filename parsing, waveform peaks, audio preview, drag-out to the DAW.

> **Status: written, NOT compiled or tested.** The authoring environment had no JUCE, no compiler and no network.
> Build it, then paste any compile errors back to get them fixed. Places most likely to need a tweak are listed at the bottom.

## Build (Windows 10/11)
Requirements: Visual Studio 2022 (Desktop C++ workload), CMake 3.22+, Git, internet (CMake downloads JUCE and the WebView2 SDK).

    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release

Outputs: `build/FlameVault_artefacts/Release/VST3/FlameVault.vst3` and `.../Standalone/FlameVault.exe`.
Test the **Standalone** first, then copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\`.

## FL Studio
1. Options > Manage plugins > *Find installed plugins* (fast scan is fine).
2. Add **FlameVault** from the Generators list (it is an instrument so its preview audio reaches the mixer).
3. Click **Add Folder** in the sidebar and pick your sample root. Scanning runs in the background.
4. Click a row to select, double-click (or the play button) to preview. Drag a row out of the plugin onto the Playlist / Channel Rack / Sampler.

## Layout
    CMakeLists.txt
    Source/
      FilenameParser.*    C++ port of your C# parser v1.0.0 (labels only, evidence kept, same thresholds)
      SampleLibrary.*     scanner thread, JSON library in %APPDATA%/FlameVault/library.json, peaks from real audio
      PluginProcessor.*   preview player (reader > transport > resampler > output), pitch/tempo, loop, volume
      PluginEditor.*      WebView2 host + native functions the page calls
    UI/index.html         the dashboard (embedded into the binary). Open it in any browser = demo mode with synthesized sounds
    UI/Assets/            only the two real image assets: 02 banner, 06 sidebar flame wave (matched by file name)
    docs/reference/       all 7 original reference crops (waveforms/logo are redrawn live, not used as images)

## What is real vs. not yet
Real: recursive scan (wav/aiff/flac/mp3/ogg), duration/format from the file, category/BPM/key *labels* from the filename,
waveforms drawn from the actual audio, play/pause/stop/seek/loop/volume, favorites persisted, Needs Review routing.
Not done: audio BPM/key analysis, AI descriptions (the description line is the filename evidence), duplicates,
missing-file relink, collections, folder watching, true time-stretch (pitch and tempo share one varispeed ratio),
list virtualization for very large libraries.

## Unverified spots (fix here first if the build or UI misbehaves)
- `GIT_TAG 8.0.4` in CMakeLists: use any JUCE 8.x tag that exists.
- WebView2 static linking: if CMake cannot fetch the SDK, install the NuGet package `Microsoft.Web.WebView2` and pass
  `-DJUCE_WEBVIEW2_PACKAGE_LOCATION=<path>`. End users need the WebView2 Runtime (ships with Windows 11 / current Windows 10).
- `UI/index.html` talks to JUCE through `window.__JUCE__.backend` using the events `__juce__invoke` / `__juce__complete`.
  If calls never resolve, replace the small shim at the top of the script with JUCE's `index.js` from
  `modules/juce_gui_extra/native/javascript/`.
- Drag-out uses `DragAndDropContainer::performExternalDragDropOfFiles` started from a web pointer event. Hosts differ; if FL ignores it,
  that call is the place to adapt.
- The parser port is not covered by tests yet. Port `FilenameParserTests.cs` to a JUCE UnitTest to confirm parity with the C# version.

## Licensing
JUCE is AGPLv3 or commercial; the VST3 SDK is GPLv3 or Steinberg proprietary. Check this before distributing the plugin.

## Build on GitHub (no local setup)
Push this folder to a GitHub repo. `.github/workflows/build.yml` builds the VST3 on a Windows runner on every push.
Download it from the repo's **Actions** tab > latest run > **Artifacts** > `FlameVault-win64`.
Tag a commit `v0.1.0` (`git tag v0.1.0 && git push --tags`) to also publish the zips as a GitHub Release.

## Troubleshooting
The plugin writes a log to `%APPDATA%\FlameVault\log.txt` (type that into the Explorer address bar). After a crash or freeze,
the last lines show where it happened. The Standalone build also writes a stack trace there if it crashes.
