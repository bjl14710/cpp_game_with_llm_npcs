#include "RatingLog.hpp"

#include <ctime>
#include <fstream>
#include <iostream>

#include "json.hpp"

namespace llm_npc {

RatingLog::RatingLog(std::filesystem::path dir) : dir_(std::move(dir)) {}

bool RatingLog::appendCandidate(const std::string& personaName,
                                const std::vector<std::string>& traitIds,
                                const std::string& playerLine,
                                const std::string& npcLine) {
    return append("candidates.jsonl", personaName, traitIds, playerLine, npcLine);
}

bool RatingLog::appendRejected(const std::string& personaName,
                               const std::vector<std::string>& traitIds,
                               const std::string& playerLine,
                               const std::string& npcLine) {
    return append("rejected.jsonl", personaName, traitIds, playerLine, npcLine);
}

bool RatingLog::append(const char* filename, const std::string& personaName,
                       const std::vector<std::string>& traitIds,
                       const std::string& playerLine, const std::string& npcLine) {
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    std::ofstream out(dir_ / filename, std::ios::app);
    if (!out) {
        if (!warned_) {
            std::cerr << "[llm_npc] ratings: cannot write " << (dir_ / filename)
                      << " — ratings disabled this session\n";
            warned_ = true;
        }
        return false;
    }
    nlohmann::json row;
    row["ts"] = static_cast<long long>(std::time(nullptr));
    row["persona"] = personaName;
    row["traits"] = traitIds;
    row["player"] = playerLine;
    row["npc"] = npcLine;
    out << row.dump() << '\n';
    return true;
}

}  // namespace llm_npc
