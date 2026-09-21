#pragma once
#include <windows.h>
#include <array>
#include <string>
#include <system_error>

namespace x52 {
[[nodiscard]] inline std::wstring WindowsError(const wchar_t* operation, DWORD code)
{
    std::array<wchar_t, 2048> buffer{};
    const auto length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    return std::wstring(operation) + L" failed. Win32 error " + std::to_wstring(code) +
        L": " + (length ? std::wstring(buffer.data(), length) : L"No system message available.");
}

class WindowsException final : public std::system_error {
public:
    WindowsException(const char* operation, DWORD code)
        : std::system_error(static_cast<int>(code), std::system_category(), operation) {}
};

class UniqueHandle final {
public:
    explicit UniqueHandle(HANDLE value) noexcept : value_(value) {}
    ~UniqueHandle() noexcept { if (valid()) CloseHandle(value_); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    [[nodiscard]] bool valid() const noexcept
    { return value_ != nullptr && value_ != INVALID_HANDLE_VALUE; }
    [[nodiscard]] HANDLE get() const noexcept { return value_; }
private:
    HANDLE value_;
};
}
