#include <fslazywindow.h>
#include <ysgl.h>
#ifdef FSGUI_USE_SYSTEM_FONT
#include <ysfontrenderer.h>
#endif
#include "fsguiapp.h"

#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <d3d11.h>

using namespace winrt;
using namespace Windows::UI::Core;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Core;

class FsLazyWindowApplication : public FsLazyWindowApplicationBase
{
public:
    static FsLazyWindowApplication *currentApplication;
    YsSystemFontRenderer *sysFont;

    FsLazyWindowApplication();
    ~FsLazyWindowApplication();

    virtual void BeforeEverything(int ac, char *av[]);
    virtual void GetOpenWindowOption(FsOpenWindowOption &opt) const;
    virtual void Initialize(int ac, char *av[]);
    virtual void Interval(void);
    virtual void BeforeTerminate(void);
    virtual void Draw(void);
    virtual bool NeedRedraw(void) const;
    virtual bool MustTerminate(void) const;
    virtual long long int GetMinimumSleepPerInterval(void) const;
};

FsLazyWindowApplication *FsLazyWindowApplication::currentApplication = nullptr;

FsLazyWindowApplication::FsLazyWindowApplication() {
    sysFont = nullptr;
}

FsLazyWindowApplication::~FsLazyWindowApplication() {
    if (nullptr != sysFont) {
        delete sysFont;
        sysFont = nullptr;
    }
}

/* virtual */ void FsLazyWindowApplication::BeforeEverything(int, char *[]) {
}

/* virtual */ void FsLazyWindowApplication::GetOpenWindowOption(FsOpenWindowOption &opt) const {
    opt.x0 = 0;
    opt.y0 = 0;
    opt.wid = 1920;
    opt.hei = 1080;
}

/* virtual */ void FsLazyWindowApplication::Initialize(int ac, char *av[]) {
#ifdef FSGUI_USE_SYSTEM_FONT
    sysFont = new YsSystemFontRenderer;
    FsGuiObject::defUnicodeRenderer = sysFont;
#endif

#ifdef FSGUI_USE_MODERN_UI
    FsGuiObject::scheme = FsGuiObject::MODERN;
    FsGuiObject::defRoundRadius = 8.0;
    FsGuiObject::defHScrollBar = 20;
    FsGuiObject::defHAnnotation = 14;
    FsGuiObject::defVSpaceUnit = 12;

    FsGuiObject::defDialogBgCol.SetDoubleRGB(0.75, 0.75, 0.75);

    FsGuiObject::defTabBgCol.SetDoubleRGB(0.82, 0.82, 0.82);
    FsGuiObject::defTabClosedFgCol.SetDoubleRGB(0.8, 0.8, 0.8);
    FsGuiObject::defTabClosedBgCol.SetDoubleRGB(0.2, 0.2, 0.2);

    FsGuiObject::defBgCol.SetDoubleRGB(0.85, 0.85, 0.85);
    FsGuiObject::defFgCol.SetDoubleRGB(0.0, 0.0, 0.0);
    FsGuiObject::defActiveBgCol.SetDoubleRGB(0.3, 0.3, 0.7);
    FsGuiObject::defActiveFgCol.SetDoubleRGB(1.0, 1.0, 1.0);
    FsGuiObject::defFrameCol.SetDoubleRGB(0.0, 0.0, 0.0);

    FsGuiObject::defListFgCol.SetDoubleRGB(0.0, 0.0, 0.0);
    FsGuiObject::defListBgCol.SetDoubleRGB(0.8, 0.8, 0.8);
    FsGuiObject::defListActiveFgCol.SetDoubleRGB(1.0, 1.0, 1.0);
    FsGuiObject::defListActiveBgCol.SetDoubleRGB(0.3, 0.3, 0.7);
#endif

    FsGuiObject::defUnicodeRenderer->RequestDefaultFontWithPixelHeight(16);
    FsGuiObject::defAsciiRenderer->RequestDefaultFontWithPixelHeight(16);

    YsCoordSysModel = YSOPENGL;

    FsGuiMainCanvas::GetMainCanvas()->Initialize(ac, av);
}

/* virtual */ void FsLazyWindowApplication::Interval(void) {
    FsGuiMainCanvas::GetMainCanvas()->OnInterval();
}

/* virtual */ void FsLazyWindowApplication::BeforeTerminate(void) {
    YsGLSLDeleteSharedRenderer();
    FsGuiMainCanvas::DeleteMainCanvas();
}

/* virtual */ void FsLazyWindowApplication::Draw(void) {
    // Implement UWP-compatible DirectX rendering here.
    FsGuiMainCanvas::GetMainCanvas()->Draw();
}

/* virtual */ bool FsLazyWindowApplication::NeedRedraw(void) const {
    return (bool)FsGuiMainCanvas::GetMainCanvas()->NeedRedraw();
}

/* virtual */ bool FsLazyWindowApplication::MustTerminate(void) const {
    return (bool)FsGuiMainCanvas::GetMainCanvas()->appMustTerminate;
}

/* static */ FsLazyWindowApplicationBase *FsLazyWindowApplicationBase::GetApplication(void) {
    if (nullptr == FsLazyWindowApplication::currentApplication) {
        FsLazyWindowApplication::currentApplication = new FsLazyWindowApplication;
    }
    return FsLazyWindowApplication::currentApplication;
}

/* virtual */ long long int FsLazyWindowApplication::GetMinimumSleepPerInterval(void) const {
    return 60;
}
