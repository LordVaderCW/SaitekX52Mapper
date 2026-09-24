#include "PropertiesView.hpp"
#include "BufferedPaint.hpp"
#include <windowsx.h>
#include <sstream>

namespace x52 {
namespace {
void Fill(HDC dc, RECT r, COLORREF color) { SetDCBrushColor(dc,color); FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH))); }
void TextAt(HDC dc, RECT r, const std::wstring& text, COLORREF color=BattlefieldTheme::Ink) {
    SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT); DrawTextW(dc,text.c_str(),-1,&r,DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK);
}
void Line(HDC dc,int x1,int y1,int x2,int y2,COLORREF color) {
    SetDCPenColor(dc,color); const auto old=SelectObject(dc,GetStockObject(DC_PEN)); MoveToEx(dc,x1,y1,nullptr);LineTo(dc,x2,y2);SelectObject(dc,old);
}
std::string Id(DWORD usage) { std::ostringstream s;s<<"r0:p"<<std::hex<<(usage>>16)<<":u"<<(usage&65535)<<":l1";return s.str(); }
int ThumbX(RECT row,const AxisDeadzone& axis,int handle)
{
    const int width=row.right-row.left-20;
    int x=row.left+10+MulDiv(axis.limits[static_cast<std::size_t>(handle)],width,65535);
    if((handle==1||handle==2) && MulDiv(axis.limits[2]-axis.limits[1],width,65535)<18)
        x+=(handle==1?-9:9);
    return x;
}
}
PropertiesView::~PropertiesView(){if(window_) RemoveWindowSubclass(window_,Proc,1);}
void PropertiesView::Attach(HWND window,BattlefieldTheme& theme,bool deadzones)
{
    window_=window;theme_=&theme;deadzones_=deadzones;live_.fill(-1);buttons_.fill(-1);
    if(!SetWindowSubclass(window,Proc,1,reinterpret_cast<DWORD_PTR>(this))) throw WindowsException("Properties view",GetLastError());
}
void PropertiesView::Settings(const std::array<AxisDeadzone,9>& axes){axes_=axes;if(window_)InvalidateRect(window_,nullptr,FALSE);}
void PropertiesView::Inputs(const X52State& state,bool connected)
{
    std::array<int,9> next;next.fill(-1);std::array<int,35> buttons;buttons.fill(-1);int hat=0;
    if(connected) {
        for(std::size_t i=0;i<next.size();++i) {
            const auto it=state.controls.find(Id(DeadzoneAxes[i]));
            if(it!=state.controls.end() && it->second.valid) next[i]=static_cast<int>(std::lround((it->second.normalized+1)*32767.5));
        }
        for(DWORD i=1;i<35;++i) {const auto it=state.controls.find(Id(0x90000+i));if(it!=state.controls.end())buttons[i]=it->second.raw?1:0;}
        const auto it=state.controls.find(Id(0x10039));if(it!=state.controls.end())hat=static_cast<int>(it->second.raw);
    }
    if(next!=live_||buttons!=buttons_||hat!=hat_||connected!=connected_) {
        live_=next;buttons_=buttons;hat_=hat;connected_=connected;
        if(window_ && IsWindowVisible(window_))InvalidateRect(window_,nullptr,FALSE);
    }
}
RECT PropertiesView::Row(int axis)const
{
    RECT r{};GetClientRect(window_,&r);const int gap=24;
    const int column=axis/5,row=axis%5,width=(r.right-gap)/2;
    const int height=std::max(64,static_cast<int>(r.bottom)/5);
    return {column*(width+gap),row*height,column*(width+gap)+width,(row+1)*height-2};
}
int PropertiesView::MoveLimit(AxisDeadzone& axis,int handle,int position)
{
    if(handle<0||handle>3)throw std::runtime_error("Invalid deadzone handle");
    auto& a=axis.limits;
    const int low=handle==0?0:a[static_cast<std::size_t>(handle-1)]+(handle==2?0:1);
    const int high=handle==3?65535:a[static_cast<std::size_t>(handle+1)]-(handle==1?0:1);
    a[static_cast<std::size_t>(handle)]=std::clamp(position,low,high);return a[static_cast<std::size_t>(handle)];
}
void PropertiesView::Move(int x)
{
    const auto r=Row(selectedAxis_);const int left=r.left+10,width=r.right-r.left-20;
    MoveLimit(axes_[static_cast<std::size_t>(selectedAxis_)],selectedHandle_,MulDiv(x-grabOffset_-left,65535,std::max(1,width)));
    InvalidateRect(window_,&r,FALSE);
    SendMessageW(GetParent(window_),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(window_),1),reinterpret_cast<LPARAM>(window_));
}
void PropertiesView::Paint(HDC dc)
{
    const int saved=SaveDC(dc);
    theme_->Background(dc,window_);const auto font=reinterpret_cast<HFONT>(SendMessageW(window_,WM_GETFONT,0,0));
    const auto old=SelectObject(dc,font?font:GetStockObject(DEFAULT_GUI_FONT));
    if(deadzones_) {
        for(int i=0;i<9;++i) {
            const auto r=Row(i);auto title=r;title.bottom=title.top+24;TextAt(dc,title,DeadzoneNames[static_cast<std::size_t>(i)]);
            const int left=r.left+10,width=r.right-r.left-20,y=r.top+20;
            const auto& a=axes_[static_cast<std::size_t>(i)].limits;
            const auto x=[&](int n){return left+MulDiv(n,width,65535);};
            Fill(dc,{left,y,x(a[0]),y+10},BattlefieldTheme::Line);
            Fill(dc,{x(a[1]),y,x(a[2]),y+10},RGB(87,57,52));
            Fill(dc,{x(a[3]),y,left+width,y+10},BattlefieldTheme::Line);
            Line(dc,left,y+10,left+width,y+10,BattlefieldTheme::Muted);
            if(live_[static_cast<std::size_t>(i)]>=0)Line(dc,x(live_[static_cast<std::size_t>(i)]),y,x(live_[static_cast<std::size_t>(i)]),y+10,RGB(250,85,62));
            for(int h=0;h<4;++h) {
                const int px=ThumbX(r,axes_[static_cast<std::size_t>(i)],h);
                Line(dc,x(a[static_cast<std::size_t>(h)]),y+9,px,y+15,BattlefieldTheme::Muted);
                Fill(dc,{px-6,y+10,px+7,y+24},IsWindowEnabled(window_)?BattlefieldTheme::Ink:BattlefieldTheme::Line);
                if(GetFocus()==window_&&selectedAxis_==i&&selectedHandle_==h){RECT focus{px-9,y+7,px+10,y+27};DrawFocusRect(dc,&focus);}
            }
            std::wstring value=L"Min "+std::to_wstring(a[0])+L"    Centre "+std::to_wstring(a[1])+L" - "+std::to_wstring(a[2])+L"    Max "+std::to_wstring(a[3]);
            TextAt(dc,{r.left,y+28,r.right,r.bottom},value,BattlefieldTheme::Muted);
        }
    } else {
        RECT area{};GetClientRect(window_,&area);
        SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,area.right,550,nullptr);SetViewportExtEx(dc,area.right,area.bottom,nullptr);
        area.bottom=550;const int half=area.right/2;
        const auto cross=[&](RECT box,int x,int y,const wchar_t* label) {
            TextAt(dc,{box.left,box.top-26,box.right,box.top},label);Fill(dc,box,BattlefieldTheme::Panel);
            Line(dc,(box.left+box.right)/2,box.top,(box.left+box.right)/2,box.bottom,BattlefieldTheme::Line);
            Line(dc,box.left,(box.top+box.bottom)/2,box.right,(box.top+box.bottom)/2,BattlefieldTheme::Line);
            if(x>=0&&y>=0){const int px=box.left+MulDiv(x,box.right-box.left-1,65535),py=box.top+MulDiv(y,box.bottom-box.top-1,65535);Fill(dc,{px-4,py-4,px+5,py+5},RGB(250,85,62));}
        };
        const int boxSize=std::min(170,half/2-24);
        cross({0,30,boxSize,30+boxSize},live_[0],live_[1],L"STICK X / Y");
        cross({boxSize+24,30,boxSize*2+24,30+boxSize},live_[7],live_[8],L"MOUSE MINI-STICK");
        for(int i=2;i<7;++i){const int y=boxSize+64+(i-2)*53;TextAt(dc,{0,y,half-24,y+23},DeadzoneNames[static_cast<std::size_t>(i)]);
            Fill(dc,{0,y+25,half-24,y+33},BattlefieldTheme::Panel);if(live_[static_cast<std::size_t>(i)]>=0)Fill(dc,{0,y+25,MulDiv(live_[static_cast<std::size_t>(i)],half-24,65535),y+33},BattlefieldTheme::Muted);}
        TextAt(dc,{half,4,area.right,29},L"BUTTONS / TOGGLES / MODE");
        const int cell=(area.right-half)/7;
        for(int i=1;i<35;++i){const int x=half+((i-1)%7)*cell,y=35+((i-1)/7)*47;const bool active=buttons_[static_cast<std::size_t>(i)]==1;
            RECT box{x,y,x+cell-8,y+34};Fill(dc,box,active?BattlefieldTheme::Ink:BattlefieldTheme::Panel);TextAt(dc,{x+10,y+6,box.right,box.bottom},std::to_wstring(i),active?BattlefieldTheme::Panel:buttons_[static_cast<std::size_t>(i)]<0?BattlefieldTheme::Line:BattlefieldTheme::Muted);}
        const std::array labels{L"LOWER HAT",L"UPPER HAT",L"THROTTLE HAT"};
        for(int h=0;h<3;++h){const int x=half+h*(area.right-half)/3,y=330;TextAt(dc,{x,y,area.right,y+24},labels[static_cast<std::size_t>(h)]);
            const int cx=x+55,cy=y+72;for(int d=0;d<8;++d){const double angle=d*3.141592653589793/4;
                bool active=h==0?hat_==d+1:false;
                if(h>0){const int base=h==1?16:20;const auto on=[&](int dir){return buttons_[static_cast<std::size_t>(base+dir)]==1;};
                    const int dx=(on(1)?1:0)-(on(3)?1:0),dy=(on(2)?1:0)-(on(0)?1:0);active=(dx||dy)&&static_cast<int>(std::lround(std::sin(angle)))==dx&&-static_cast<int>(std::lround(std::cos(angle)))==dy;}
                const int px=cx+static_cast<int>(std::sin(angle)*33),py=cy-static_cast<int>(std::cos(angle)*33);Fill(dc,{px-5,py-5,px+6,py+6},active?RGB(250,85,62):BattlefieldTheme::Line);}}
        TextAt(dc,{half,470,area.right,area.bottom},connected_?L"Live driver-reported inputs. Missing controls are dimmed.\nUse Live inputs for exact raw counts and physical names.":L"Disconnected. Waiting for the X52.",BattlefieldTheme::Muted);
    }
    SelectObject(dc,old);
    RestoreDC(dc,saved);
}
LRESULT CALLBACK PropertiesView::Proc(HWND window,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR data) noexcept
{
    auto& view=*reinterpret_cast<PropertiesView*>(data);
    try {
        switch(message){
        case WM_NCHITTEST:if(view.deadzones_&&IsWindowEnabled(window))return HTCLIENT;break;
        case WM_SETCURSOR:if(view.deadzones_&&IsWindowEnabled(window)){SetCursor(LoadCursorW(nullptr,IDC_SIZEWE));return TRUE;}break;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:case WM_PRINTCLIENT:{PAINTSTRUCT ps{};const auto dc=message==WM_PAINT?BeginPaint(window,&ps):reinterpret_cast<HDC>(w);{PaintBuffer buffer(window,dc);view.Paint(buffer.Dc());}if(message==WM_PAINT)EndPaint(window,&ps);return 0;}
        case WM_GETDLGCODE:return view.deadzones_?DLGC_WANTARROWS:0;
        case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(window,nullptr,FALSE);break;
        case WM_LBUTTONDOWN:if(view.deadzones_&&IsWindowEnabled(window)){
            SetFocus(window);int distance=INT_MAX;
            for(int i=0;i<9;++i){const auto r=view.Row(i);if(GET_Y_LPARAM(l)<r.top+20||GET_Y_LPARAM(l)>r.top+47)continue;
                if(GET_X_LPARAM(l)<r.left||GET_X_LPARAM(l)>=r.right)continue;
                for(int h=0;h<4;++h){const int x=ThumbX(r,view.axes_[static_cast<std::size_t>(i)],h);
                    const int d=abs(GET_X_LPARAM(l)-x);if(d<distance){distance=d;view.selectedAxis_=i;view.selectedHandle_=h;}}}
            if(distance==INT_MAX)return 0;
            const auto row=view.Row(view.selectedAxis_);
            const int original=row.left+10+MulDiv(view.axes_[static_cast<std::size_t>(view.selectedAxis_)].limits[static_cast<std::size_t>(view.selectedHandle_)],row.right-row.left-20,65535);
            view.grabOffset_=distance<=18?GET_X_LPARAM(l)-original:0;
            view.dragging_=true;SetCapture(window);view.Move(GET_X_LPARAM(l));return 0;}
            break;
        case WM_MOUSEMOVE:if(view.dragging_){view.Move(GET_X_LPARAM(l));return 0;}break;
        case WM_LBUTTONUP:if(view.dragging_){view.Move(GET_X_LPARAM(l));view.dragging_=false;ReleaseCapture();return 0;}break;
        case WM_CAPTURECHANGED:case WM_CANCELMODE:view.dragging_=false;break;
        case WM_KEYDOWN:if(view.deadzones_&&IsWindowEnabled(window)){
            if(w==VK_UP||w==VK_DOWN)view.selectedAxis_=(view.selectedAxis_+(w==VK_UP?8:1))%9;
            else if(w==VK_SPACE)view.selectedHandle_=(view.selectedHandle_+1)%4;
            else if(w==VK_LEFT||w==VK_RIGHT){auto& axis=view.axes_[static_cast<std::size_t>(view.selectedAxis_)];MoveLimit(axis,view.selectedHandle_,axis.limits[static_cast<std::size_t>(view.selectedHandle_)]+(w==VK_LEFT?-1:1)*(GetKeyState(VK_SHIFT)<0?655:66));SendMessageW(GetParent(window),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(window),1),reinterpret_cast<LPARAM>(window));}
            else break;InvalidateRect(window,nullptr,FALSE);return 0;}break;
        case WM_NCDESTROY:RemoveWindowSubclass(window,Proc,1);view.window_=nullptr;break;
        }
    }catch(...){OutputDebugStringW(L"X52 properties view error\n");}
    return DefSubclassProc(window,message,w,l);
}
}
