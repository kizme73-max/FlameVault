#include "SampleLibrary.h"
#include <map>

SampleLibrary::SampleLibrary() : juce::Thread ("FlameVault scanner")
{
    peakFormats.registerBasicFormats();
    load();
}

SampleLibrary::~SampleLibrary() { stopThread (5000); }

juce::File SampleLibrary::storeFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("FlameVault").getChildFile ("library.json");
}

juce::var SampleLibrary::recordToVar (const SampleRecord& r)
{
    auto* o = new juce::DynamicObject();
    const auto genreList = r.genres.isEmpty() ? juce::String ("-") : r.genres;
    o->setProperty ("id", r.id);
    o->setProperty ("path", r.path);
    o->setProperty ("name", r.name);
    o->setProperty ("fmt", r.ext);
    o->setProperty ("cat", r.category);
    o->setProperty ("sub", r.subcategory.isEmpty() ? juce::String ("-") : r.subcategory);
    o->setProperty ("form", r.form);
    o->setProperty ("genre", genreList.upToFirstOccurrenceOf (",", false, false).trim());
    o->setProperty ("genres", genreList);
    o->setProperty ("key", r.key.isEmpty() ? juce::String ("-") : r.key);
    o->setProperty ("bpm", r.bpm);
    o->setProperty ("dur", r.durationSec);
    o->setProperty ("conf", juce::roundToInt (r.score * 100.0f));
    o->setProperty ("fav", r.favorite);
    o->setProperty ("review", r.needsReview);
    o->setProperty ("desc", r.description);
    return juce::var (o);
}

juce::var SampleLibrary::toVar()
{
    juce::Array<juce::var> list, f;
    {
        const juce::ScopedLock sl (lock);
        for (auto& r : records)
        {
            auto v = recordToVar (r);
            v.getDynamicObject()->setProperty ("missing", ! juce::File (r.path).existsAsFile());
            list.add (v);
        }
        for (auto& s : folders) f.add (s);
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("samples", list);
    o->setProperty ("folders", f);
    return juce::var (o);
}

void SampleLibrary::addFolder (const juce::File& folder)
{
    { const juce::ScopedLock sl (lock); folders.addIfNotAlreadyThere (folder.getFullPathName()); }
    save();
    rescan();
}

void SampleLibrary::rescan()
{
    signalThreadShouldExit();
    stopThread (3000);
    startThread();
}

bool SampleLibrary::toggleFavorite (const juce::String& id)
{
    bool now = false;
    {
        const juce::ScopedLock sl (lock);
        for (auto& r : records) if (r.id == id) { r.favorite = ! r.favorite; now = r.favorite; }
    }
    save();
    return now;
}

juce::File SampleLibrary::fileFor (const juce::String& id)
{
    const juce::ScopedLock sl (lock);
    for (auto& r : records) if (r.id == id) return juce::File (r.path);
    return {};
}

juce::var SampleLibrary::peaksFor (const juce::String& id)
{
    constexpr int N = 240;
    juce::File file;
    {
        const juce::ScopedLock sl (lock);
        for (auto& r : records)
            if (r.id == id)
            {
                if (r.peaks.size() == (size_t) N)
                {
                    juce::Array<juce::var> a;
                    for (auto p : r.peaks) a.add (p);
                    return a;
                }
                file = juce::File (r.path);
            }
    }

    std::vector<float> peaks ((size_t) N, 0.0f);
    if (std::unique_ptr<juce::AudioFormatReader> reader { peakFormats.createReaderFor (file) })
    {
        const auto len = reader->lengthInSamples;
        const int ch = (int) juce::jmin (2u, reader->numChannels);
        float maxAll = 0.0f;
        for (int i = 0; i < N && len > 0; ++i)
        {
            const auto a = (juce::int64) (len * i / N);
            const auto n = juce::jmax<juce::int64> (1, (juce::int64) (len * (i + 1) / N) - a);
            juce::Range<float> r[2];
            reader->readMaxLevels (a, n, r, ch);
            float m = 0.0f;
            for (int c = 0; c < ch; ++c) m = juce::jmax (m, std::abs (r[c].getStart()), std::abs (r[c].getEnd()));
            peaks[(size_t) i] = m;
            maxAll = juce::jmax (maxAll, m);
        }
        if (maxAll > 0.0f) for (auto& p : peaks) p /= maxAll;
    }

    juce::Array<juce::var> a;
    for (auto p : peaks) a.add (p);
    { const juce::ScopedLock sl (lock); for (auto& r : records) if (r.id == id) r.peaks = peaks; }
    return a;
}

void SampleLibrary::run()
{
    juce::AudioFormatManager fm;                  // scanner thread gets its own manager
    fm.registerBasicFormats();

    juce::StringArray dirs;
    std::map<juce::String, SampleRecord> old;
    {
        const juce::ScopedLock sl (lock);
        dirs = folders;
        for (auto& r : records) old[r.id] = r;
    }

    juce::Array<juce::File> files;
    for (auto& d : dirs)
        juce::File (d).findChildFiles (files, juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    juce::WeakReference<SampleLibrary> self (this);
    auto post = [self] (bool scanning, int done, int total)
    {
        juce::MessageManager::callAsync ([self, scanning, done, total]
        {
            if (self != nullptr && self->onScan)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("scanning", scanning);
                o->setProperty ("done", done);
                o->setProperty ("total", total);
                self->onScan (juce::var (o));
            }
        });
    };

    std::vector<SampleRecord> out;
    const int total = files.size();
    int done = 0;
    post (true, 0, total);

    for (auto& f : files)
    {
        if (threadShouldExit()) return;

        SampleRecord r;
        const auto id = juce::String::toHexString (f.getFullPathName().hashCode64());
        auto it = old.find (id);
        if (it != old.end() && it->second.durationSec > 0) r = it->second;
        else
        {
            r.id = id;
            r.path = f.getFullPathName();
            r.name = f.getFileNameWithoutExtension();
            r.ext = f.getFileExtension().removeCharacters (".").toUpperCase();

            const auto p = fv::parseFilename (f.getFileName());
            r.category = p.assignedCategory;
            r.subcategory = p.strength == fv::Strength::High ? p.suggestedSub : juce::String();
            r.form = p.form;
            r.genres = p.tags.joinIntoString (", ");
            r.bpm = p.bpm;
            r.score = (float) p.score;
            r.needsReview = p.needsReview();
            r.key = p.key.replace (" Minor", "m").replace (" Major", "");

            juce::StringArray why;
            for (auto& e : p.evidence) why.add (e.token + " (" + e.ruleId + ")");
            r.description = why.isEmpty() ? "No category found in the filename. Review this sample manually."
                                          : "Detected from the filename: " + why.joinIntoString (", ") + ".";

            if (std::unique_ptr<juce::AudioFormatReader> rd { fm.createReaderFor (f) })
            {
                r.sampleRate = rd->sampleRate;
                r.channels = (int) rd->numChannels;
                r.durationSec = rd->sampleRate > 0 ? (double) rd->lengthInSamples / rd->sampleRate : 0.0;
            }
        }
        if (it != old.end()) r.favorite = it->second.favorite;
        out.push_back (std::move (r));

        if (++done % 25 == 0) post (true, done, total);
    }

    {
        const juce::ScopedLock sl (lock);
        records = std::move (out);
    }
    save();
    post (false, total, total);
    juce::MessageManager::callAsync ([self] { if (self != nullptr && self->onChanged) self->onChanged(); });
}

void SampleLibrary::save()
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> list, f;
    {
        const juce::ScopedLock sl (lock);
        for (auto& r : records)
        {
            auto v = recordToVar (r);
            auto* o = v.getDynamicObject();
            o->setProperty ("score", r.score);
            o->setProperty ("sub", r.subcategory);
            o->setProperty ("key", r.key);
            o->setProperty ("genres", r.genres);
            o->setProperty ("ch", r.channels);
            o->setProperty ("sr", r.sampleRate);
            list.add (v);
        }
        for (auto& s : folders) f.add (s);
    }
    root->setProperty ("version", 1);
    root->setProperty ("samples", list);
    root->setProperty ("folders", f);

    const auto file = storeFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (root)));
}

void SampleLibrary::load()
{
    const auto v = juce::JSON::parse (storeFile());
    if (! v.isObject()) return;

    if (auto* fa = v["folders"].getArray())
        for (auto& s : *fa) folders.add (s.toString());
    if (auto* arr = v["samples"].getArray())
        for (auto& s : *arr)
        {
            SampleRecord r;
            r.id = s["id"].toString(); r.path = s["path"].toString(); r.name = s["name"].toString(); r.ext = s["fmt"].toString();
            r.category = s["cat"].toString(); r.subcategory = s["sub"].toString(); r.form = s["form"].toString();
            r.genres = s["genres"].toString(); r.key = s["key"].toString(); r.description = s["desc"].toString();
            r.bpm = (int) s["bpm"]; r.durationSec = (double) s["dur"]; r.score = (float) (double) s["score"];
            r.favorite = (bool) s["fav"]; r.needsReview = (bool) s["review"];
            r.channels = (int) s["ch"]; r.sampleRate = (double) s["sr"];
            records.push_back (std::move (r));
        }
}
