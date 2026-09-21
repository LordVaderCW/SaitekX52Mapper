#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <fstream>
#include <atomic>

namespace x52 {
using Json = nlohmann::json;
class Journal final {
public:
    explicit Journal(const std::filesystem::path& directory);
    ~Journal();
    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;
    void Event(const std::string& type, Json fields = Json::object());
    [[nodiscard]] std::string Error() const;
    [[nodiscard]] std::uint64_t Dropped() const { return dropped_.load(); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
private:
    void Run(std::stop_token stop);
    std::filesystem::path path_;
    std::ofstream stream_;
    mutable std::mutex mutex_;
    std::condition_variable_any condition_;
    std::deque<std::string> pending_;
    std::string error_;
    std::atomic<std::uint64_t> dropped_{};
    std::jthread worker_;
};
Json StateJson(const struct X52State& state);
Json DiffJson(const std::vector<struct BitChange>& changes);
void WriteJson(const std::filesystem::path& path, const Json& value);
}
