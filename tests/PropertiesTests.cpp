#include "device/DeadzoneSettings.hpp"
#include "ui/PropertiesView.hpp"
#include <sstream>

namespace {
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct TestWindow {
    HWND handle{};
    ~TestWindow(){if(handle)DestroyWindow(handle);}
};
void SavedCalibrationTests(const std::string& valid)
{
    using namespace x52;
    struct Fixture {
        const std::filesystem::path directory=std::filesystem::temp_directory_path()/
            (L"x52-calibration-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        const std::filesystem::path first=directory/L"SaiC075C-first.pr0", second=directory/L"SaiC075C-second.pr0", other=directory/L"SaiC0762-other.pr0";
        Fixture(){Check(std::filesystem::create_directory(directory),"Create isolated calibration fixture");}
        ~Fixture(){std::error_code error;for(const auto& file:{first,second,other})std::filesystem::remove(file,error);std::filesystem::remove(directory,error);}
    } fixture;
    const auto write=[](const std::filesystem::path& file,const std::string& bytes){std::ofstream stream(file,std::ios::binary);stream<<bytes;stream.close();Check(!stream.fail(),"Write calibration fixture");};
    const auto refused=[&]{try{(void)FindSavedX52Calibration(fixture.directory);return false;}catch(const std::exception&){return true;}};
    Check(refused(),"Absent calibration must not fabricate defaults");
    write(fixture.other,valid);Check(refused(),"Other model calibration is not used");
    write(fixture.first,valid);
    Check(FindSavedX52Calibration(fixture.directory)==fixture.first,"Saved X52 settings remain readable without an active driver path");
    write(fixture.second,valid);Check(refused(),"Multiple saved calibrations require an explicit choice, not newest-file guessing");
    std::filesystem::remove(fixture.second);
    write(fixture.first,"[profile version=0x00000005 [commands]]");Check(refused(),"Command profiles cannot be fallback calibration");
    write(fixture.first,std::string(65537,'x'));Check(refused(),"Saved calibration size remains bounded");
    std::filesystem::remove(fixture.first);std::filesystem::create_directory(fixture.first);
    Check(refused(),"Directories masquerading as saved calibration are rejected");
}
void MouseDragTests()
{
    using namespace x52;
    TestWindow parent{CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,1000,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    Check(parent.handle!=nullptr,"Create mouse test host");
    const auto child=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|SS_NOTIFY,0,0,1000,500,parent.handle,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(child!=nullptr,"Create deadzone mouse test control");
    BattlefieldTheme theme;PropertiesView view;view.Attach(child,theme,true);
    std::array<AxisDeadzone,9> baseline{};view.Settings(baseline);
    Check(SendMessageW(child,WM_NCHITTEST,0,0)==HTCLIENT,"Deadzone control must receive mouse hit tests");
    const auto point=[](int x){return MAKELPARAM(x,36);};
    // At this size, coincident centre bounds draw at x=235 and x=253.
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(235));
    Check(GetCapture()==child,"Mouse drag captures pointer outside control");
    SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(195));
    SendMessageW(child,WM_LBUTTONUP,0,point(195));
    Check(view.Settings()[0].limits[1]<baseline[0].limits[1] && view.Settings()[0].limits[2]==baseline[0].limits[2],"Left centre handle independently draggable");
    Check(GetCapture()!=child,"Mouse release ends capture");
    view.Settings(baseline);
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(253));
    SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(293));
    SendMessageW(child,WM_LBUTTONUP,0,point(293));
    Check(view.Settings()[0].limits[2]>baseline[0].limits[2] && view.Settings()[0].limits[1]==baseline[0].limits[1],"Right centre handle independently draggable");
    for(std::size_t i=1;i<baseline.size();++i)Check(view.Settings()[i]==baseline[i],"Dragging leaves other axes unchanged");
    const auto edited=view.Settings();EnableWindow(child,FALSE);
    SendMessageW(child,WM_LBUTTONDOWN,MK_LBUTTON,point(10));
    SendMessageW(child,WM_MOUSEMOVE,MK_LBUTTON,point(100));
    SendMessageW(child,WM_LBUTTONUP,0,point(100));
    Check(view.Settings()==edited,"Disabled editor rejects mouse edits");
}
}
void PropertiesTests()
{
    MouseDragTests();
    using namespace x52;
    std::ostringstream text;text<<"[profile version=0x01000001 [controllers [controller=e81d998b-c604-4d71-be97-35ca01439c7e [member=c7719f41-f667-4514-bbb4-3f38c9e4d05a] [controls ";
    for(const auto id:DeadzoneAxes)text<<"[axis="<<id<<" envelope=envelope [envelope cran=32768 hran=65535 ldead=30000 hdead=35000 hsat=65535 lcurve=32768 hcurve=32768]]";
    text<<"]]]]";
    SavedCalibrationTests(text.str());
    const auto root=ParsePr0(text.str());auto axes=DecodeDeadzones(root);
    Check(axes[7].limits[1]==30000 && axes[8].limits[2]==35000,"Both mouse axes read real envelope limits");
    axes[7].limits={20,24000,41000,65000};
    const auto edited=UpdateDeadzones(root,axes);
    Check(DecodeDeadzones(ParsePr0(SerializePr0(edited)))==axes,"All four bounds roundtrip independently");
    Check(DecodeDeadzones(root)[7].limits[1]==30000,"Original calibration preserved");
    auto invalid=root; invalid.children[0].children[0].children.push_back(Pr0Node{"shifts",{}, {},{}});
    bool rejected=false;try{(void)DecodeDeadzones(invalid);}catch(...){rejected=true;}
    Check(rejected,"Command profile sections cannot be loaded by calibration editor");
    invalid=root;auto& controls=invalid.children[0].children[0].children[1];controls.children[1]=controls.children[0];
    rejected=false;try{(void)DecodeDeadzones(invalid);}catch(...){rejected=true;}Check(rejected,"Duplicate axes rejected");
    invalid=root;invalid.children[0].children[0].children[1].children[0].children[0].attributes["lpower"]="1";
    rejected=false;try{(void)DecodeDeadzones(invalid);}catch(...){rejected=true;}Check(rejected,"Nonlinear custom curves rejected without modification");
    AxisDeadzone axis;PropertiesView::MoveLimit(axis,1,70000);Check(axis.limits[1]==32768,"Centre handles cannot cross");
    PropertiesView::MoveLimit(axis,0,70000);Check(axis.limits[0]==32767,"Minimum retains active travel");
    PropertiesView::MoveLimit(axis,3,-100);Check(axis.limits[3]==32769,"Maximum retains active travel");ValidateDeadzone(axis);
    PropertiesView::MoveLimit(axis,0,-10);Check(axis.limits[0]==0,"Limits stay in the vendor range");
    axis.limits={0,35000,30000,65535};rejected=false;try{ValidateDeadzone(axis);}catch(...){rejected=true;}Check(rejected,"Inverted centre rejected");
}
