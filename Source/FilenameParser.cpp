#include "FilenameParser.h"
#include <map>
#include <regex>
#include <algorithm>

namespace fv
{
namespace
{
struct Rule { std::string id, field, value; double score; };

const std::map<std::string, Rule>& dictionary()
{
    static const std::map<std::string, Rule> d = []
    {
        std::map<std::string, Rule> m;
        auto add = [&] (std::initializer_list<const char*> keys, Rule r) { for (auto* k : keys) m[k] = r; };

        add ({ "kick", "kik" },                              { "CAT-KICK", "Category", "Drums/Kick", 0.95 });
        add ({ "snare" },                                    { "CAT-SNARE", "Category", "Drums/Snare", 0.95 });
        add ({ "sn" },                                       { "CAT-SNARE-ABBR", "Category", "Drums/Snare", 0.85 });
        add ({ "clap" },                                     { "CAT-CLAP", "Category", "Drums/Clap", 0.95 });
        add ({ "cp" },                                       { "CAT-CLAP-ABBR", "Category", "Drums/Clap", 0.70 });
        add ({ "closedhat", "chh" },                         { "CAT-HAT-CLOSED", "Category", "Drums/Closed Hi-Hat", 0.95 });
        add ({ "hh", "hat", "hihat" },                       { "CAT-HAT-GENERIC", "Category", "Drums/Closed Hi-Hat", 0.80 });
        add ({ "openhat" },                                  { "CAT-HAT-OPEN", "Category", "Drums/Open Hat", 0.95 });
        add ({ "oh" },                                       { "CAT-HAT-OPEN-ABBR", "Category", "Drums/Open Hat", 0.80 });
        add ({ "808" },                                      { "CAT-808", "Category", "Bass/808", 0.92 });
        add ({ "sub" },                                      { "CAT-SUB", "Category", "Bass/Sub Bass", 0.70 });
        add ({ "vox", "voc", "vocal", "vocals" },            { "CAT-VOCALS", "Category", "Vocals", 0.90 });
        add ({ "acapella", "acappella" },                    { "CAT-ACAPELLA", "Category", "Vocals/Acapella", 0.95 });
        add ({ "riser", "rise" },                            { "CAT-RISER", "Category", "FX/Riser", 0.95 });
        add ({ "impact", "hit", "boom" },                    { "CAT-IMPACT", "Category", "FX/Impact", 0.70 });
        add ({ "atm", "ambience", "atmosphere" },            { "CAT-ATMOS", "Category", "Cinematic/Atmosphere", 0.90 });
        add ({ "drone" },                                    { "CAT-DRONE", "Category", "Cinematic/Drone", 0.92 });
        add ({ "loop", "lp" },                               { "FORM-LOOP", "Form", "Loop", 0.95 });
        add ({ "shot", "oneshot" },                          { "FORM-ONESHOT", "Form", "One-Shot", 0.95 });

        struct T { const char *k, *v; };
        for (auto t : { T{"trap","Trap"}, T{"hiphop","Hip-Hop"}, T{"dnb","D&B"}, T{"techno","Techno"}, T{"house","House"},
                        T{"pop","Pop"}, T{"rnb","R&B"}, T{"dark","Dark"}, T{"sad","Sad"}, T{"happy","Happy"},
                        T{"aggressive","Aggressive"}, T{"emotional","Emotional"}, T{"cinematic","Cinematic"}, T{"ambient","Ambient"} })
            m[t.k] = { "TAG-" + juce::String (t.v).toUpperCase().toStdString(), "Tag", t.v, 1.0 };
        return m;
    }();
    return d;
}

// Split on any non letter/digit AND on letter<->digit boundaries ("Kick01" -> kick, 01).
std::vector<std::string> tokenize (const juce::String& s)
{
    std::vector<std::string> out;
    juce::String cur;
    int prev = 0;
    for (auto c : s.toLowerCase())
    {
        const int k = juce::CharacterFunctions::isLetter (c) ? 1 : juce::CharacterFunctions::isDigit (c) ? 2 : 0;
        if (k == 0 || (prev != 0 && k != prev))
        {
            if (cur.isNotEmpty()) out.push_back (cur.toStdString());
            cur.clear();
        }
        if (k != 0) cur += juce::String::charToString (c);
        prev = k;
    }
    if (cur.isNotEmpty()) out.push_back (cur.toStdString());
    return out;
}

Evidence ev (const std::string& tok, const Rule& r)
{
    return { tok, r.field, r.value, r.id, r.score };
}
}

static ParseResult parseImpl (const juce::String& fileName)
{
    ParseResult res;
    res.version = "1.0.0";
    auto name = juce::File::createFileWithoutCheckingPath (fileName).getFileNameWithoutExtension().toStdString();

    // 1) Labels. std::regex has no lookbehind, so the "(?<!...)" checks are done by hand.
    static const std::regex bpmRx (R"((\d{2,3})\s?bpm)", std::regex::icase);
    static const std::regex keyRx (R"(([A-G])([#b]?)\s?(min|minor|maj|major)(?![A-Za-z]))", std::regex::icase);

    for (std::sregex_iterator it (name.begin(), name.end(), bpmRx), end; it != end; ++it)
    {
        const auto pos = (size_t) it->position (0);
        if (pos > 0 && std::isdigit ((unsigned char) name[pos - 1])) continue;
        const int b = std::stoi ((*it)[1].str());
        if (b >= 40 && b <= 300)
        {
            res.bpm = b;
            res.evidence.push_back ({ it->str(), "BpmLabel", juce::String (b), "LBL-BPM", 1.0 });
            name.replace (pos, (size_t) it->length (0), " ");
        }
        break;
    }

    for (std::sregex_iterator it (name.begin(), name.end(), keyRx), end; it != end; ++it)
    {
        const auto pos = (size_t) it->position (0);
        if (pos > 0 && std::isalpha ((unsigned char) name[pos - 1])) continue;
        const juce::String q ((*it)[3].str());
        const juce::String quality = q.toLowerCase().startsWith ("min") ? "Minor" : "Major";
        res.key = juce::String ((*it)[1].str()).toUpperCase() + juce::String ((*it)[2].str()) + " " + quality;
        res.evidence.push_back ({ it->str(), "KeyLabel", res.key, "LBL-KEY", 1.0 });
        name.replace (pos, (size_t) it->length (0), " ");
        break;
    }

    // 2) Tokens + dictionary; try joining two adjacent tokens first ("open hat" -> "openhat").
    const auto tokens = tokenize (juce::String (name));
    const auto& dict = dictionary();
    for (size_t i = 0; i < tokens.size();)
    {
        if (i + 1 < tokens.size())
            if (auto p = dict.find (tokens[i] + tokens[i + 1]); p != dict.end())
            { res.evidence.push_back (ev (tokens[i] + tokens[i + 1], p->second)); i += 2; continue; }
        if (auto s = dict.find (tokens[i]); s != dict.end())
            res.evidence.push_back (ev (tokens[i], s->second));
        ++i;
    }

    // 3) Sample form, separate from category.
    juce::StringArray forms;
    for (auto& e : res.evidence) if (e.field == "Form") forms.addIfNotAlreadyThere (e.value);
    if (forms.size() == 1) res.form = forms[0];

    // 4) Category resolution with explicit conflict handling.
    std::vector<Evidence> cats;
    for (auto& e : res.evidence) if (e.field == "Category") cats.push_back (e);
    if (cats.empty() && res.form == "Loop")
    {
        Evidence fb { "loop", "Category", "Loops", "CAT-LOOP-FALLBACK", 0.70 };
        cats.push_back (fb);
        res.evidence.push_back (fb);
    }

    res.suggestedCategory = "Uncategorized";
    if (! cats.empty())
    {
        auto best = *std::max_element (cats.begin(), cats.end(), [] (auto& a, auto& b) { return a.score < b.score; });
        juce::StringArray distinct;
        for (auto& c : cats) distinct.addIfNotAlreadyThere (c.value);
        res.conflict = distinct.size() > 1;
        res.score = res.conflict ? std::max (0.0, best.score - 0.25) : best.score;
        res.suggestedCategory = best.value.upToFirstOccurrenceOf ("/", false, false);
        if (best.value.contains ("/")) res.suggestedSub = best.value.fromFirstOccurrenceOf ("/", false, false);
    }

    res.strength = res.score >= 0.90 ? Strength::High : res.score >= 0.65 ? Strength::Medium : Strength::Low;
    for (auto& e : res.evidence) if (e.field == "Tag") res.tags.addIfNotAlreadyThere (e.value);
    res.assignedCategory = res.strength == Strength::High ? res.suggestedCategory : "Uncategorized";
    return res;
}

ParseResult parseFilename (const juce::String& fileName)
{
    try { return parseImpl (fileName); }
    catch (...) { ParseResult r; r.version = "1.0.0"; r.suggestedCategory = r.assignedCategory = "Uncategorized"; return r; }
}
}
