#pragma once
// Scoped frame-budget instrumentation. Zero dependencies, header-only.
//
// Issue #257. Compiles to NOTHING unless ENABLE_PROFILING is defined, so this
// is safe to leave in the tree: with it off, every macro expands to `((void)0)`
// and no timer object, atomic or allocation survives the preprocessor.
//
// WHY NOT TRACY. Tracy is the better instrument and this would be a worse
// header than its ZoneScoped. It is also a new third-party dependency fetched
// from the network at configure time, and CLAUDE.md says to ask before adding
// one. This measures what #257 asks for with nothing new in the build.
//
// WHY NOT JUST TIME THE WHOLE RUN. Because this project already does, and it
// is why the recorded baseline is wrong. main.cpp sets FLAG_VSYNC_HINT and
// SetTargetFPS(60), so EndDrawing() blocks until the next vblank and raylib
// sleeps to pace the loop. Wall-clock frame time is therefore PINNED near
// 16.67 ms whenever the real work fits inside it — measuring the frame
// limiter, not the frame. Scoped timers around the work are the only way to
// see actual cost and real headroom, which is exactly the distinction
// tools/bench_npc_render.py cannot make.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <source_location>
#include <string>
#include <unordered_map>
#include <vector>

namespace prof {

// Per-call samples are retained, not just aggregates. A frame budget is a
// DEADLINE, not an average: a scope averaging 4 ms that spikes to 20 ms every
// sixtieth frame stutters visibly while its mean looks healthy. Keeping the
// samples is what makes a real p99 possible instead of a max and a guess.
//
// Cost is trivial at this granularity — ~10 scopes x 600 frames x 8 bytes is
// under 50 KB. The cap exists only so an accidental scope inside a hot inner
// loop degrades to aggregates rather than eating memory.
inline constexpr std::size_t kMaxSamplesPerScope = 250000;

struct ScopeStats {
    std::uint64_t calls = 0;
    std::int64_t total_ns = 0;
    std::int64_t min_ns = INT64_MAX;
    std::int64_t max_ns = 0;
    std::vector<std::int64_t> samples;
    // Insertion order, so the report lists scopes as the frame runs them
    // rather than in unordered_map order.
    std::size_t first_seen = 0;
};

class Registry {
   public:
    static Registry& instance() {
        static Registry r;
        return r;
    }

    void record(const std::string& name, std::int64_t ns) {
        std::lock_guard lock(m_mutex);
        auto [it, inserted] = m_stats.try_emplace(name);
        ScopeStats& s = it->second;
        if (inserted) s.first_seen = m_stats.size();
        ++s.calls;
        s.total_ns += ns;
        s.min_ns = std::min(s.min_ns, ns);
        s.max_ns = std::max(s.max_ns, ns);
        if (s.samples.size() < kMaxSamplesPerScope) s.samples.push_back(ns);
    }

    void mark_frame() {
        std::lock_guard lock(m_mutex);
        ++m_frames;
    }

    std::uint64_t frames() const {
        std::lock_guard lock(m_mutex);
        return m_frames;
    }

    // Writes the aggregate plus every retained sample as JSON. Called once at
    // shutdown; the report generator reads this file.
    //
    // Hand-rolled rather than via nlohmann: this header must stay includable
    // from the app layer with no extra link or include dependency, and the
    // shape is two loops.
    void dump_json(const std::string& path) const {
        std::lock_guard lock(m_mutex);
        std::ofstream out(path);
        if (!out) return;

        std::vector<const std::pair<const std::string, ScopeStats>*> ordered;
        ordered.reserve(m_stats.size());
        for (const auto& kv : m_stats) ordered.push_back(&kv);
        std::sort(ordered.begin(), ordered.end(), [](auto* a, auto* b) {
            return a->second.first_seen < b->second.first_seen;
        });

        out << "{\n  \"frames\": " << m_frames << ",\n  \"scopes\": [\n";
        for (std::size_t i = 0; i < ordered.size(); ++i) {
            const auto& [name, s] = *ordered[i];
            out << "    {\"name\": \"" << name << "\", \"calls\": " << s.calls
                << ", \"total_ns\": " << s.total_ns << ", \"min_ns\": "
                << (s.calls ? s.min_ns : 0) << ", \"max_ns\": " << s.max_ns
                << ", \"samples\": [";
            for (std::size_t j = 0; j < s.samples.size(); ++j) {
                if (j) out << ',';
                out << s.samples[j];
            }
            out << "]}" << (i + 1 < ordered.size() ? "," : "") << "\n";
        }
        out << "  ]\n}\n";
    }

   private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ScopeStats> m_stats;
    std::uint64_t m_frames = 0;
};

class ScopeTimer {
   public:
    // An empty name auto-fills from the enclosing function, which is what
    // std::source_location buys over the __FILE__/__LINE__ macro dance this
    // would need below C++20. The upgrade in #256 is what makes it available.
    explicit ScopeTimer(std::string name,
                        const std::source_location loc = std::source_location::current())
        : m_name(name.empty() ? std::string(loc.function_name()) : std::move(name)),
          m_start(std::chrono::steady_clock::now()) {}

    ~ScopeTimer() {
        const auto elapsed = std::chrono::steady_clock::now() - m_start;
        Registry::instance().record(
            m_name, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    }

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

   private:
    std::string m_name;
    std::chrono::steady_clock::time_point m_start;
};

// Begin/end pair for phases that are NOT naturally braced.
//
// main.cpp's loop body is roughly 1300 lines in one scope, and its major
// phases (simulate / draw 3D / draw UI) are delimited by nothing but blank
// lines. Wrapping those in `{ }` purely to place an RAII timer would move
// hundreds of declarations into inner scopes and change what compiles. This
// measures the same interval without touching the surrounding structure.
//
// Not nestable, deliberately — one span at a time keeps it impossible to
// mis-pair silently. Use ScopeTimer for anything that nests.
class Span {
   public:
    static void begin(std::string name) {
        current().m_name = std::move(name);
        current().m_start = std::chrono::steady_clock::now();
    }
    static void end() {
        Span& s = current();
        if (s.m_name.empty()) return;
        const auto elapsed = std::chrono::steady_clock::now() - s.m_start;
        Registry::instance().record(
            s.m_name, std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
        s.m_name.clear();
    }

   private:
    static Span& current() {
        static thread_local Span s;
        return s;
    }
    std::string m_name;
    std::chrono::steady_clock::time_point m_start;
};

}  // namespace prof

#ifdef ENABLE_PROFILING
#define PROF_SCOPE() ::prof::ScopeTimer _prof_t_{""}
#define PROF_SCOPE_N(name) ::prof::ScopeTimer _prof_t_{name}
#define PROF_SPAN_BEGIN(name) ::prof::Span::begin(name)
#define PROF_SPAN_END() ::prof::Span::end()
#define PROF_FRAME() ::prof::Registry::instance().mark_frame()
#define PROF_DUMP(path) ::prof::Registry::instance().dump_json(path)
#else
#define PROF_SCOPE() ((void)0)
#define PROF_SCOPE_N(name) ((void)0)
#define PROF_SPAN_BEGIN(name) ((void)0)
#define PROF_SPAN_END() ((void)0)
#define PROF_FRAME() ((void)0)
#define PROF_DUMP(path) ((void)0)
#endif
