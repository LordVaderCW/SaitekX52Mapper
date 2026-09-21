#pragma once
#include "Journal.hpp"
#include "../input/State.hpp"

namespace x52 {
Json AnalyzeCapture(const std::vector<Json>& records, const Assignments& assignments, std::int64_t frequency);
Json AssignmentsJson(const Assignments& assignments);
}
