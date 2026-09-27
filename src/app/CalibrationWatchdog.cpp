#include "CalibrationWatchdog.hpp"
#include <tlhelp32.h>

namespace x52 {
CalibrationObservation ObserveBattlefieldCalibration()
{
    CalibrationObservation result;
    UniqueHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));
    if(!snapshot.valid())throw WindowsException("Read Battlefield process list",GetLastError());
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot.get(),&entry)) {
        do {
            if(_wcsicmp(entry.szExeFile,L"bf3.exe")==0 || _wcsicmp(entry.szExeFile,L"bf4.exe")==0) {
                result.battlefieldRunning=true;break;
            }
        }while(Process32NextW(snapshot.get(),&entry));
        if(!result.battlefieldRunning && GetLastError()!=ERROR_NO_MORE_FILES)
            throw WindowsException("Read Battlefield process list",GetLastError());
    }else if(GetLastError()!=ERROR_NO_MORE_FILES)throw WindowsException("Read Battlefield process list",GetLastError());
    if(!result.battlefieldRunning)return result;
    std::vector<std::wstring> paths;
    for(const auto& device:EnumerateHid())if(device.isPs28())paths.push_back(device.path);
    if(paths.size()!=1)return result;
    result.settings=ReadDeadzones(paths.front());
    return result;
}
bool CalibrationWatchdog::Observe(const CalibrationObservation& observation,std::uint64_t now)
{
    if(!observation.battlefieldRunning || !observation.settings) {
        missing_=active_=0;previous_.reset();return false;
    }
    const auto& current=*observation.settings;
    if(!current.activeFile.empty()) {
        missing_=0;
        if(++active_>=2){active_=2;attemptedLoss_=false;}
        previous_=current;return false;
    }
    active_=0;
    if(!previous_ || previous_->devicePath!=current.devicePath || previous_->file!=current.file || previous_->original!=current.original)
        missing_=0;
    previous_=current;
    if(missing_<2)++missing_;
    if(missing_<2 || attemptedLoss_ || Paused() || now<retryAfter_)return false;
    attemptedLoss_=true;++attempts_;retryAfter_=now+60000;
    return true;
}
}
