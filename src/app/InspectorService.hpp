#pragma once
#include "../diagnostics/Recovery.hpp"
#include "../profiles/LearnedControls.hpp"
#include "../device/HidEnumerator.hpp"
#include "../input/AxisFilter.hpp"
#include <deque>
#include <functional>

namespace x52 {
struct Snapshot {
    bool connected{}, captureActive{}, learning{};
    std::wstring device, inventory;
    std::string status{"Starting inspector"}, error, utc, lastExport;
    X52State physical, filtered, safe;
    AxisFilterSettings filters;
    bool filtersReady{};
    Assignments assignments;
    std::vector<std::uint8_t> raw, previous;
    std::vector<BitChange> changes;
    std::vector<std::string> learnCandidates;
    Json learnEvidence;
    std::map<std::string, Json> controlEvidence;
    std::uint64_t assignmentRequest{};
    std::string assignmentError;
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> learnRanges;
    X52RecoveryManager recovery;
    double lastReportMs{}, processMs{};
    std::uint64_t sequence{}, captureReports{}, throttleChangesDuringDropout{}, stickChangesDuringDropout{};
};
enum class CommandType { Rescan, Reinitialize, StartLearn, SaveLearn, StartCapture,
    StopCapture, MarkDropout, MarkReturn, Export, Configure, DeviceEvent, AssignControl, ClearAssignment, ConfigureFilters };
struct Command {
    CommandType type;
    std::string text, id;
    LearnedControl learned;
    RecoverySettings settings;
    bool removed{};
    std::uint64_t request{};
    AxisFilterSettings filters;
};
class InspectorService final {
public:
    explicit InspectorService(std::filesystem::path directory);
    ~InspectorService();
    InspectorService(const InspectorService&) = delete;
    InspectorService& operator=(const InspectorService&) = delete;
    void Start();
    void Stop();
    void Send(Command command);
    Snapshot Read() const;
    [[nodiscard]] const std::filesystem::path& directory() const { return directory_; }
private:
    void Run(std::stop_token stop);
    void Publish(const Snapshot& snapshot);
    std::filesystem::path directory_;
    Journal journal_;
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    std::deque<Command> commands_;
    std::jthread worker_;
};
std::filesystem::path DefaultDataDirectory();
}
