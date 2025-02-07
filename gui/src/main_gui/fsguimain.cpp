#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <d3d11.h>
#include <dxgi1_2.h>

using namespace winrt;
using namespace Windows::UI::Core;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Core;

class UwpDirectXApp : public winrt::implements<UwpDirectXApp, IFrameworkView>
{
private:
    CoreWindow m_window{ nullptr };
    com_ptr<ID3D11Device> m_d3dDevice;
    com_ptr<ID3D11DeviceContext> m_d3dContext;
    com_ptr<IDXGISwapChain1> m_swapChain;
    com_ptr<ID3D11RenderTargetView> m_renderTargetView;

public:
    void Initialize(CoreApplicationView const& applicationView)
    {
        applicationView.Activated({ this, &UwpDirectXApp::OnActivated });
    }

    void SetWindow(CoreWindow const& window)
    {
        m_window = window;
        m_window.PointerPressed([](CoreWindow const&, PointerEventArgs const& args) {
            printf("Pointer pressed!\n");
        });
        InitDirect3D();
    }

    void Load(winrt::hstring const&) {}
    void Run()
    {
        while (true)
        {
            m_window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
            Render();
        }
    }
    void Uninitialize() {}

    void OnActivated(CoreApplicationView const&, IActivatedEventArgs const& args)
    {
        CoreWindow::GetForCurrentThread().Activate();
    }

    void InitDirect3D()
    {
        D3D11_CREATE_DEVICE_FLAG deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        
        D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, 
            featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION, m_d3dDevice.put(), nullptr, m_d3dContext.put()
        );
    }

    void Render()
    {
        FLOAT clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_d3dContext->ClearRenderTargetView(m_renderTargetView.get(), clearColor);
        m_swapChain->Present(1, 0);
    }
};

class UwpDirectXAppFactory : public winrt::implements<UwpDirectXAppFactory, IFrameworkViewSource>
{
public:
    IFrameworkView CreateView() { return winrt::make<UwpDirectXApp>(); }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(winrt::make<UwpDirectXAppFactory>());
}
