#include "device/DeadzoneSettings.hpp"
#include "ui/PropertiesView.hpp"
#include <sstream>

namespace {
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct TestWindow {
    HWND handle{};
    ~TestWindow(){if(handle)DestroyWindow(handle);}
};
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
