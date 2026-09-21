#include "Journal.hpp"
#include "../util/Text.hpp"
#include "../input/State.hpp"

namespace x52 {
Journal::Journal(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    path_ = directory / ("events-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(Qpc()) + ".jsonl");
    stream_.open(path_, std::ios::binary);
    if (!stream_) throw std::runtime_error("Cannot create event log: " + path_.string());
    worker_ = std::jthread([this](std::stop_token stop) { Run(stop); });
}
Journal::~Journal() { worker_.request_stop(); condition_.notify_all(); if (worker_.joinable()) worker_.join(); }
void Journal::Event(const std::string& type, Json fields)
{
    fields["event"] = type;
    fields["utc"] = UtcNow();
    fields["qpc"] = Qpc();
    auto line = fields.dump() + "\n";
    { std::lock_guard lock(mutex_);
        if (pending_.size() >= 4096) { ++dropped_; return; }
        pending_.push_back(std::move(line));
    }
    condition_.notify_one();
}
std::string Journal::Error() const { std::lock_guard lock(mutex_); return error_; }
void Journal::Run(std::stop_token stop)
{
    for (;;) {
        std::deque<std::string> batch;
        { std::unique_lock lock(mutex_);
            condition_.wait(lock, stop, [this] { return !pending_.empty(); });
            batch.swap(pending_);
            if (batch.empty() && stop.stop_requested()) break;
        }
        for (const auto& line : batch) stream_ << line;
        stream_.flush();
        if (!stream_) { std::lock_guard lock(mutex_); error_ = "Event log write failed (disk full or inaccessible)"; }
    }
}
Json StateJson(const X52State& state)
{
    Json out = Json::object();
    for (const auto& [id, control] : state.controls) out[id] = {
        {"raw", control.raw}, {"min", control.minimum}, {"max", control.maximum},
        {"normalized", control.normalized}, {"valid", control.valid},
        {"kind", static_cast<int>(control.kind)}, {"report_id", control.reportId}};
    return out;
}
Json DiffJson(const std::vector<BitChange>& changes)
{
    Json out = Json::array();
    for (const auto& change : changes) out.push_back({{"byte", change.byte},
        {"before", change.before}, {"after", change.after}, {"changed_bit_mask", change.mask}});
    return out;
}
void WriteJson(const std::filesystem::path& path, const Json& value)
{
    auto temporary = path;
    temporary += ".tmp";
    { std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot create " + temporary.string());
        file << value.dump(2) << '\n';
        file.flush();
        if (!file) throw std::runtime_error("Cannot write " + temporary.string());
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw WindowsException("MoveFileExW (save JSON)", GetLastError());
}
}
