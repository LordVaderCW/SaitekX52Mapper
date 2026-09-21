#pragma once
#include "Windows.hpp"
#include <span>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace x52 {
inline std::string Utf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!count) throw WindowsException("WideCharToMultiByte", GetLastError());
    std::string result(static_cast<std::size_t>(count), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), count, nullptr, nullptr))
        throw WindowsException("WideCharToMultiByte", GetLastError());
    return result;
}
inline std::wstring Wide(const std::string& value)
{
    if (value.empty()) return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (!count) throw WindowsException("MultiByteToWideChar", GetLastError());
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), count))
        throw WindowsException("MultiByteToWideChar", GetLastError());
    return result;
}
inline std::string Hex(std::span<const std::uint8_t> bytes)
{
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (auto value : bytes) out << std::setw(2) << static_cast<unsigned>(value) << ' ';
    return out.str();
}
inline std::wstring WindowsLines(const std::string& value)
{
    std::string lines;
    lines.reserve(value.size());
    for (const auto character : value) {
        if (character == '\n') lines += '\r';
        lines += character;
    }
    return Wide(lines);
}
inline std::string UtcNow()
{
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << time.wYear << '-' << std::setw(2) << time.wMonth
        << '-' << std::setw(2) << time.wDay << 'T' << std::setw(2) << time.wHour << ':'
        << std::setw(2) << time.wMinute << ':' << std::setw(2) << time.wSecond << '.'
        << std::setw(3) << time.wMilliseconds << 'Z';
    return out.str();
}
inline std::int64_t Qpc()
{
    LARGE_INTEGER value{};
    if (!QueryPerformanceCounter(&value)) throw WindowsException("QueryPerformanceCounter", GetLastError());
    return value.QuadPart;
}
inline std::int64_t QpcFrequency()
{
    LARGE_INTEGER value{};
    if (!QueryPerformanceFrequency(&value)) throw WindowsException("QueryPerformanceFrequency", GetLastError());
    return value.QuadPart;
}
}
