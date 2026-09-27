#include "app/CalibrationWatchdog.hpp"
#include <stdexcept>

void CalibrationWatchdogTests()
{
    using namespace x52;
    const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    DeadzoneSettings settings;settings.devicePath=L"original-X52-test";settings.file=L"saved.pr0";settings.original="saved bounds";
    CalibrationObservation missing{true,false,settings};
    CalibrationObservation active=missing;active.settings->activeFile=active.settings->file;
    CalibrationWatchdog policy;
    for(unsigned i=0;i<5;++i)check(!policy.Observe({false,false,settings},i*2000),"No automatic reload outside Battlefield");
    for(unsigned i=0;i<5;++i)check(!policy.Observe(active,i*2000),"Active calibration cannot trigger a drift guess");
    check(!policy.Observe(missing,10000),"One missing-path sample cannot reload");
    check(policy.Observe(missing,12000),"Two matching missing-path samples request one reload");
    check(!policy.Observe(missing,14000),"One attempt per loss; no repeated reload while still missing");
    (void)policy.Observe(active,16000);(void)policy.Observe(active,18000);
    check(!policy.Observe(missing,20000) && !policy.Observe(missing,22000),"Repeated faults respect cooldown");
    check(policy.Observe(missing,72000),"Next loss may reload after 60-second cooldown");
    (void)policy.Observe(active,74000);(void)policy.Observe(active,76000);
    check(!policy.Observe(missing,132000) && policy.Observe(missing,134000),"Third bounded recovery attempt");
    (void)policy.Observe(active,136000);(void)policy.Observe(active,138000);
    check(!policy.Observe(missing,200000) && !policy.Observe(missing,202000) && policy.Paused(),"Three-attempt cap prevents a recovery loop");
    CalibrationWatchdog changed;
    check(!changed.Observe(missing,0),"First sample");
    auto other=missing;other.settings->original="externally edited bounds";
    check(!changed.Observe(other,2000),"Changed calibration must be observed twice again");
    check(changed.Observe(other,4000),"Fresh matching saved state can be restored");
    CalibrationWatchdog paused;paused.Pause();
    check(!paused.Observe(missing,0) && !paused.Observe(missing,2000),"Adapter failures pause automatic recovery");
    CalibrationWatchdog absent;
    (void)absent.Observe(missing,0);(void)absent.Observe({true,false,std::nullopt},2000);
    check(!absent.Observe(missing,4000),"Missing device breaks confirmation sequence");
    CalibrationWatchdog oneLoss;
    (void)oneLoss.Observe(missing,0);check(oneLoss.Observe(missing,2000),"Initial missing-path recovery");
    check(!oneLoss.Observe(missing,90000),"Cooldown expiry alone does not re-arm an unresolved loss");
}
