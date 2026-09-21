#pragma once
#include "Journal.hpp"
#include "../input/State.hpp"

namespace x52 {
Json AnalyzeCapture(const std::vector<Json>& records, const Assignments& assignments, std::int64_t frequency);
Json AssignmentsJson(const Assignments& assignments);
// Ten seconds in production, also bounded by encoded bytes and record count.
class ReportHistory final {
public:
    explicit ReportHistory(std::int64_t windowTicks, std::size_t byteLimit = 16 * 1024 * 1024,
        std::size_t recordLimit = 4096) : windowTicks_(windowTicks), byteLimit_(byteLimit), recordLimit_(recordLimit) {}
    void Push(Json record)
    {
        const auto tick = record.at("qpc").get<std::int64_t>();
        sizes_.push_back(record.dump().size()); bytes_ += sizes_.back();
        records_.push_back(std::move(record)); Trim(tick);
    }
    void Trim(std::int64_t now)
    {
        while (!records_.empty() && (records_.size() > recordLimit_ || bytes_ > byteLimit_ ||
            now - records_.front().at("qpc").get<std::int64_t>() > windowTicks_)) {
            bytes_ -= sizes_.front(); sizes_.pop_front(); records_.pop_front();
        }
    }
    const std::deque<Json>& Records() const { return records_; }
private:
    std::int64_t windowTicks_;
    std::size_t byteLimit_, recordLimit_, bytes_{};
    std::deque<Json> records_;
    std::deque<std::size_t> sizes_;
};
}
