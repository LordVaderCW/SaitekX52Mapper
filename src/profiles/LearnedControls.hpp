#pragma once
#include "../diagnostics/Journal.hpp"
#include "../input/State.hpp"

namespace x52 {
Assignments LoadAssignments(const std::filesystem::path& path);
void SaveAssignment(const std::filesystem::path& path, const std::string& id,
    const LearnedControl& control, const Json& evidence);
void RemoveAssignment(const std::filesystem::path& path, const std::string& id);
}
