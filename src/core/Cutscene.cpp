#include "Cutscene.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include "Config.hpp"  // trim
#include "Gossip.hpp"  // normalizeSubject
#include "Journal.hpp"  // clockLabel
#include "Zones.hpp"   // zoneName

namespace llm_npc {
namespace {

// Matches the fact bus's own content limit (KnownFact::content, "one short
// statement").
constexpr std::size_t kMaxJournalLine = 140;

// Splits "key = value". Returns false for a blank line, a comment, or a line
// with no '='.
//
// A '#' only starts a comment when it is the FIRST non-space character — the
// same departure from Config.cpp's readKv that LineBank.cpp and Storyline.cpp
// make, and for the same reason. Captions are prose: "Table #3, still set for
// two." has to survive.
bool splitKeyValue(const std::string& raw, std::string& key, std::string& value) {
    const std::string line = trim(raw);
    if (line.empty() || line.front() == '#') return false;
    const auto eq = line.find('=');
    if (eq == std::string::npos) return false;
    key = trim(line.substr(0, eq));
    value = trim(line.substr(eq + 1));
    return !key.empty();
}

// True when every character is renderable by the built-in bitmap font.
//
// raylib's default font has glyphs for ASCII 32-126 and nothing else, so a
// typographer's dash or a curly quote reaches the screen as a literal "?".
// Found the only way it could be — by looking at a capture. Every author-facing
// string is checked at load, because prose pasted from a document does this
// constantly and the failure is invisible until someone runs the scene.
bool isRenderableAscii(const std::string& text) {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > 126u) return false;
    }
    return true;
}

// "x, y, z" -> Vec3. Returns false if any component is missing, so a typo in a
// camera line is reported rather than silently placing the shot at the origin.
bool parseVec3(const std::string& text, Vec3& out) {
    float parts[3] = {0.f, 0.f, 0.f};
    std::size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        if (start > text.size()) return false;
        const auto comma = text.find(',', start);
        const std::string piece =
            trim(text.substr(start, comma == std::string::npos
                                        ? std::string::npos
                                        : comma - start));
        if (piece.empty()) return false;
        parts[i] = static_cast<float>(std::atof(piece.c_str()));
        if (comma == std::string::npos) {
            // Ran out of commas before filling all three.
            if (i != 2) return false;
            start = text.size() + 1;
        } else {
            start = comma + 1;
        }
    }
    out = Vec3{parts[0], parts[1], parts[2]};
    return true;
}

bool parseEase(const std::string& text, Ease& out) {
    if (text == "linear") { out = Ease::Linear; return true; }
    if (text == "smooth") { out = Ease::Smooth; return true; }
    if (text == "hold") { out = Ease::Hold; return true; }
    return false;
}

bool parseSkippable(const std::string& text, Skippable& out) {
    if (text == "never") { out = Skippable::Never; return true; }
    if (text == "always") { out = Skippable::Always; return true; }
    if (text == "after_first") { out = Skippable::AfterFirst; return true; }
    return false;
}

// Eased progress through a beat. Hold snaps: it is a cut, and a cut that eases
// is just a fast move.
float easedT(Ease ease, float t) {
    const float c = clampf(t, 0.f, 1.f);
    switch (ease) {
        case Ease::Linear: return c;
        case Ease::Smooth: return c * c * (3.f - 2.f * c);
        case Ease::Hold:   return 1.f;
    }
    return c;
}

CameraPose lerpPose(const CameraPose& a, const CameraPose& b, float t) {
    CameraPose out;
    out.position = a.position + (b.position - a.position) * t;
    // Angles go the short way round. Interpolating 350 -> 10 numerically spins
    // the camera 340 degrees backwards, which looks like a bug and is one.
    out.yawDeg = a.yawDeg + shortestAngleDelta(a.yawDeg, b.yawDeg) * t;
    out.pitchDeg = a.pitchDeg + shortestAngleDelta(a.pitchDeg, b.pitchDeg) * t;
    return out;
}

const std::string& emptyString() {
    static const std::string kEmpty;
    return kEmpty;
}

}  // namespace

float CutsceneDef::duration() const {
    float total = 0.f;
    for (const CutsceneBeat& b : beats) total += b.hold;
    return total;
}

std::optional<CutsceneDef> parseCutsceneFile(const std::filesystem::path& path,
                                             std::vector<std::string>* errors) {
    const std::string name = path.filename().string();
    const auto fail = [&](const std::string& why) -> std::optional<CutsceneDef> {
        if (errors) errors->push_back(name + ": " + why);
        return std::nullopt;
    };

    std::ifstream in(path);
    if (!in) return fail("cannot open");

    CutsceneDef scene;
    scene.id = path.stem().string();

    // Recoverable problems are collected and reported alongside a usable
    // cutscene, so an author fixing a typo does not lose the whole scene.
    std::vector<std::string> localErrors;
    bool inBeat = false;

    std::string raw;
    while (std::getline(in, raw)) {
        std::string key;
        std::string value;
        if (!splitKeyValue(raw, key, value)) continue;

        if (key == "beat") {
            CutsceneBeat beat;
            beat.id = value;
            scene.beats.push_back(std::move(beat));
            inBeat = true;
            continue;
        }

        if (!inBeat) {
            // File-level keys, which must all precede the first beat.
            if (key == "name") {
                scene.name = value;
            } else if (key == "skippable") {
                if (!parseSkippable(value, scene.skippable)) {
                    localErrors.push_back(name + ": unknown skippable `" + value +
                                          "` (never / always / after_first)");
                }
            } else if (key == "letterbox") {
                scene.letterboxPx = std::atoi(value.c_str());
            } else if (key == "journal_subject") {
                // Normalized here rather than trusted, because the journal
                // compares subject keys EXACTLY: an authored "Body In Plaza"
                // that reaches the bus unnormalized opens a second subject
                // nobody's testimony can ever join.
                scene.journalSubject = normalizeSubject(value);
                if (scene.journalSubject.empty()) {
                    localErrors.push_back(name + ": journal_subject `" + value +
                                          "` normalizes to nothing");
                }
            } else if (key == "journal_line") {
                if (!isRenderableAscii(value)) {
                    localErrors.push_back(name +
                                          ": journal_line has a non-ASCII "
                                          "character; the built-in font renders "
                                          "it as `?`");
                }
                // KnownFact::content is one short statement; the bus's own
                // limit is 140 and an overlong line would be cut mid-word by
                // whoever commits it.
                if (value.size() > kMaxJournalLine) {
                    localErrors.push_back(
                        name + ": journal_line is " + std::to_string(value.size()) +
                        " characters; trimmed to " + std::to_string(kMaxJournalLine));
                    scene.journalLine = value.substr(0, kMaxJournalLine);
                } else {
                    scene.journalLine = value;
                }
            } else if (key == "id") {
                // Allowed but ignored: the filename stem is authoritative, the
                // same rule storylines/README.md states. Saying so beats
                // silently honouring one of two disagreeing ids.
                if (value != scene.id) {
                    localErrors.push_back(name + ": `id` says `" + value +
                                          "` but the filename stem `" + scene.id +
                                          "` wins");
                }
            } else {
                localErrors.push_back(name + ": unknown file key `" + key + "`");
            }
            continue;
        }

        CutsceneBeat& beat = scene.beats.back();
        if (key == "camera") {
            if (parseVec3(value, beat.pose.position)) {
                beat.hasPosition = true;
            } else {
                localErrors.push_back(name + ": beat `" + beat.id +
                                      "` camera needs `x, y, z`, got `" + value + "`");
            }
        } else if (key == "yaw") {
            beat.pose.yawDeg = static_cast<float>(std::atof(value.c_str()));
            beat.hasYaw = true;
        } else if (key == "pitch") {
            beat.pose.pitchDeg = static_cast<float>(std::atof(value.c_str()));
            beat.hasPitch = true;
        } else if (key == "hold") {
            beat.hold = static_cast<float>(std::atof(value.c_str()));
        } else if (key == "ease") {
            if (!parseEase(value, beat.ease)) {
                localErrors.push_back(name + ": beat `" + beat.id +
                                      "` unknown ease `" + value +
                                      "` (linear / smooth / hold)");
            }
        } else if (key == "caption" || key == "slug" || key == "speaker" ||
                   key == "headline") {
            if (!isRenderableAscii(value)) {
                localErrors.push_back(name + ": beat `" + beat.id + "` " + key +
                                      " has a non-ASCII character; the built-in "
                                      "font renders it as `?`");
            }
            if (key == "caption") {
                beat.caption = value;
            } else if (key == "slug") {
                beat.slug = value;
            } else if (key == "speaker") {
                beat.speakerLine = value;
            } else {
                beat.headline = value;
            }
        } else if (key == "fade_in") {
            beat.fadeIn = static_cast<float>(std::atof(value.c_str()));
        } else if (key == "fade_out") {
            beat.fadeOut = static_cast<float>(std::atof(value.c_str()));
        } else if (key == "fade_colour" || key == "fade_color") {
            beat.fadeColour = value;
        } else {
            localErrors.push_back(name + ": beat `" + beat.id +
                                  "` unknown key `" + key + "`");
        }
    }

    if (scene.beats.empty()) return fail("no beats");
    if (scene.name.empty()) scene.name = scene.id;

    // Both journal keys or neither. Half a pair silently does nothing: the app
    // writes only when it has a subject AND a line, so a scene carrying just
    // one would never open its case — and a caller using the written fact as
    // its "already played" test would replay the scene on every launch forever.
    if (scene.journalSubject.empty() != scene.journalLine.empty()) {
        localErrors.push_back(name +
                              ": journal_subject and journal_line must both be "
                              "set or both omitted; the write is skipped");
        scene.journalSubject.clear();
        scene.journalLine.clear();
    }

    // A beat that renders on no frame at all is never what an author meant, so
    // hold is clamped up rather than reported. Negative holds come from a typed
    // minus sign and would otherwise run the clock backwards.
    for (CutsceneBeat& beat : scene.beats) {
        if (beat.hold < CutscenePlayer::kFixedTimestep) {
            beat.hold = CutscenePlayer::kFixedTimestep;
        }
    }

    if (errors) {
        errors->insert(errors->end(), localErrors.begin(), localErrors.end());
    }
    return scene;
}

std::vector<CutsceneDef> loadCutscenes(const std::filesystem::path& dir,
                                       std::vector<std::string>* errors) {
    std::vector<CutsceneDef> out;

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        std::fprintf(stderr,
                     "[llm_npc] cutscenes: cannot read \"%s\" — scripted "
                     "sequences disabled this session\n",
                     dir.string().c_str());
        return out;
    }

    // directory_iterator order is unspecified, so sort before parsing: load
    // order has to be reproducible across machines or a seeded pick is not.
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".cutscene") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const std::filesystem::path& file : files) {
        std::optional<CutsceneDef> scene = parseCutsceneFile(file, errors);
        if (scene) out.push_back(std::move(*scene));
    }
    return out;
}

const CutsceneDef* findCutscene(const std::vector<CutsceneDef>& all,
                                const std::string& id) {
    for (const CutsceneDef& scene : all) {
        if (scene.id == id) return &scene;
    }
    return nullptr;
}

void truncateToBudget(CutsceneDef& scene, float budgetSeconds) {
    if (budgetSeconds <= 0.f || scene.duration() <= budgetSeconds) return;

    const float over = scene.duration() - budgetSeconds;
    std::fprintf(stderr,
                 "[llm_npc] cutscene \"%s\": %.2fs over its %.2fs phase budget "
                 "— truncated at the boundary\n",
                 scene.id.c_str(), over, budgetSeconds);

    float used = 0.f;
    std::vector<CutsceneBeat> kept;
    for (CutsceneBeat& beat : scene.beats) {
        const float remaining = budgetSeconds - used;
        if (remaining < CutscenePlayer::kFixedTimestep) break;
        if (beat.hold > remaining) beat.hold = remaining;
        used += beat.hold;
        kept.push_back(std::move(beat));
    }
    scene.beats = std::move(kept);
}

namespace {

// Replaces every occurrence of `token` in `text`. A loop rather than one
// find/replace because a caption may legitimately name the zone twice.
void substitute(std::string& text, const std::string& token,
                const std::string& value) {
    std::size_t at = text.find(token);
    while (at != std::string::npos) {
        text.replace(at, token.size(), value);
        at = text.find(token, at + value.size());
    }
}

}  // namespace

CutsceneDef buildOpeningCutscene(const CutsceneDef& templateDef,
                                 const std::string& sceneZoneId,
                                 double murderHour, Vec3 bodyPosition) {
    CutsceneDef out = templateDef;

    // clockLabel takes world SECONDS; murderHour is an hour in [0, 24).
    const std::string hour = clockLabel(murderHour * 3600.0);
    const std::string& place = zoneName(sceneZoneId);

    for (CutsceneBeat& beat : out.beats) {
        // Authored positions are offsets from the body, so the same framing
        // works wherever the scene lands. Only translated when the beat
        // actually set a position — a beat inheriting the previous pose must
        // keep inheriting it, and translating an unset (0,0,0) would silently
        // turn "hold here" into "cut to the body".
        if (beat.hasPosition) {
            beat.pose.position = beat.pose.position + bodyPosition;
        }
        // Yaw and pitch are absolute. The body sits at a known offset from
        // every authored position, so an authored angle frames it identically
        // in any zone — and auto-aiming at the body would quietly override
        // shots deliberately composed to look away from it.
        substitute(beat.caption, "{zone}", place);
        substitute(beat.caption, "{hour}", hour);
    }
    return out;
}

namespace {

// One montage beat: the prototype, aimed at a clue's zone.
CutsceneBeat clueBeat(const CutsceneBeat& prototype, const ClueStep& step,
                      const std::string& killer) {
    CutsceneBeat beat = prototype;
    beat.id = prototype.id + "_" + std::to_string(step.order);
    if (beat.hasPosition) {
        // Authored as an offset from the zone centre, so one framing serves
        // all nine zones — the same trick the opening uses with the body.
        beat.pose.position = beat.pose.position + zoneCentre(step.zoneId);
    }
    substitute(beat.caption, "{clue}", step.caption);
    substitute(beat.caption, "{zone}", zoneName(step.zoneId));
    substitute(beat.caption, "{killer}", killer);
    return beat;
}

}  // namespace

CutsceneDef buildWinCutscene(const CutsceneDef& templateDef,
                             const MontagePlan& plan, const std::string& killer) {
    CutsceneDef out = templateDef;
    out.beats.clear();

    for (const CutsceneBeat& prototype : templateDef.beats) {
        if (prototype.id == "found") {
            for (const ClueStep& step : plan.found) {
                out.beats.push_back(clueBeat(prototype, step, killer));
            }
            continue;
        }
        if (prototype.id == "missed") {
            for (const ClueStep& step : plan.missed) {
                out.beats.push_back(clueBeat(prototype, step, killer));
            }
            continue;
        }
        // Anything announcing the missed beat goes with it. A "there was more"
        // title over nothing is worse than no title, and a group that swept
        // the board has earned not being told about a beat that is empty.
        if (plan.missed.empty() && prototype.id.rfind("missed", 0) == 0) continue;

        CutsceneBeat beat = prototype;
        substitute(beat.caption, "{killer}", killer);
        out.beats.push_back(std::move(beat));
    }
    return out;
}

void CutscenePlayer::play(const CutsceneDef& scene, const CameraPose& from) {
    if (active_) {
        std::fprintf(stderr,
                     "[llm_npc] cutscene \"%s\" requested while \"%s\" is "
                     "playing — ignored\n",
                     scene.id.c_str(), scene_.id.c_str());
        return;
    }
    if (scene.beats.empty()) {
        std::fprintf(stderr, "[llm_npc] cutscene \"%s\" has no beats — not played\n",
                     scene.id.c_str());
        return;
    }

    scene_ = scene;
    ++seen_[scene_.id];

    // Resolve pose inheritance once, here, because the first beat inherits from
    // the runtime pose and later beats inherit from the resolved value of the
    // one before — neither of which the parser can know.
    resolved_.clear();
    resolved_.reserve(scene_.beats.size() + 1);
    resolved_.push_back(from);
    for (const CutsceneBeat& beat : scene_.beats) {
        CameraPose pose = resolved_.back();
        if (beat.hasPosition) pose.position = beat.pose.position;
        if (beat.hasYaw) pose.yawDeg = beat.pose.yawDeg;
        if (beat.hasPitch) pose.pitchDeg = beat.pose.pitchDeg;
        resolved_.push_back(pose);
    }

    active_ = true;
    beatIndex_ = 0;
    beatElapsed_ = 0.f;
}

void CutscenePlayer::skip() {
    if (!active_ || !canSkip()) return;
    // Land on a clean state rather than wherever the fade happened to be. A
    // half-dimmed screen left behind by a skip is indistinguishable from a bug.
    active_ = false;
    beatElapsed_ = 0.f;
}

bool CutscenePlayer::canSkip() const {
    switch (scene_.skippable) {
        case Skippable::Never:  return false;
        case Skippable::Always: return true;
        case Skippable::AfterFirst: return timesSeen(scene_.id) > 1;
    }
    return true;
}

bool CutscenePlayer::advance(float dtSeconds) {
    if (!active_) return false;

    const float step = fixedStep_ ? kFixedTimestep : dtSeconds;
    beatElapsed_ += step;

    // A loop, not an if: one long step can cross several short beats, and
    // dropping the ones it skipped would make playback frame-rate dependent —
    // exactly what fixed-step mode exists to prevent.
    while (active_ && beatElapsed_ >= scene_.beats[beatIndex_].hold) {
        beatElapsed_ -= scene_.beats[beatIndex_].hold;
        ++beatIndex_;
        if (beatIndex_ >= static_cast<int>(scene_.beats.size())) {
            active_ = false;
            beatIndex_ = static_cast<int>(scene_.beats.size()) - 1;
            beatElapsed_ = 0.f;
        }
    }
    return active_;
}

const CutsceneBeat* CutscenePlayer::beat() const {
    if (scene_.beats.empty()) return nullptr;
    const int i = std::min(beatIndex_, static_cast<int>(scene_.beats.size()) - 1);
    return &scene_.beats[i];
}

CameraPose CutscenePlayer::pose() const {
    const CutsceneBeat* b = beat();
    if (b == nullptr) return CameraPose{};
    const std::size_t i = static_cast<std::size_t>(beatIndex_);
    if (i + 1 >= resolved_.size()) return resolved_.back();
    return lerpPose(resolved_[i], resolved_[i + 1],
                    easedT(b->ease, beatElapsed_ / b->hold));
}

float CutscenePlayer::fadeAlpha() const {
    const CutsceneBeat* b = beat();
    if (b == nullptr || !active_) return 0.f;

    // Fade in runs from opaque to clear at the start; fade out from clear to
    // opaque at the end. Both are clamped, so a fade longer than its beat
    // simply never finishes rather than overshooting into negative alpha.
    float alpha = 0.f;
    if (b->fadeIn > 0.f && beatElapsed_ < b->fadeIn) {
        alpha = std::max(alpha, 1.f - beatElapsed_ / b->fadeIn);
    }
    if (b->fadeOut > 0.f) {
        const float remaining = b->hold - beatElapsed_;
        if (remaining < b->fadeOut) {
            alpha = std::max(alpha, 1.f - remaining / b->fadeOut);
        }
    }
    return clampf(alpha, 0.f, 1.f);
}

const std::string& CutscenePlayer::fadeColour() const {
    const CutsceneBeat* b = beat();
    return b == nullptr ? emptyString() : b->fadeColour;
}

int CutscenePlayer::letterboxPx() const { return scene_.letterboxPx; }

const std::string& CutscenePlayer::caption() const {
    const CutsceneBeat* b = beat();
    return b == nullptr ? emptyString() : b->caption;
}

const std::string& CutscenePlayer::slug() const {
    const CutsceneBeat* b = beat();
    return b == nullptr ? emptyString() : b->slug;
}

const std::string& CutscenePlayer::speakerLine() const {
    const CutsceneBeat* b = beat();
    return b == nullptr ? emptyString() : b->speakerLine;
}

const std::string& CutscenePlayer::headline() const {
    const CutsceneBeat* b = beat();
    return b == nullptr ? emptyString() : b->headline;
}

int CutscenePlayer::timesSeen(const std::string& id) const {
    const auto it = seen_.find(id);
    return it == seen_.end() ? 0 : it->second;
}

}  // namespace llm_npc
