#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdint.h>

using Microsoft::WRL::ComPtr;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Gaming::Input;
using namespace Windows::Storage;
using namespace Windows::UI::Core;

namespace BlueWakeUWP
{
    static void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw Platform::Exception::CreateException(hr);
        }
    }

    class D3D12Probe final
    {
    public:
        void Initialize(CoreWindow^ window)
        {
            UINT factoryFlags = 0;
#if defined(_DEBUG)
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            {
                debug->EnableDebugLayer();
                factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
#endif

            ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)));
            ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queue)));

            auto bounds = window->Bounds;
            const UINT width = static_cast<UINT>(bounds.Width > 1.0f ? bounds.Width : 1920.0f);
            const UINT height = static_cast<UINT>(bounds.Height > 1.0f ? bounds.Height : 1080.0f);

            DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
            swapDesc.Width = width;
            swapDesc.Height = height;
            swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            swapDesc.SampleDesc.Count = 1;
            swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapDesc.BufferCount = FrameCount;
            swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapDesc.Scaling = DXGI_SCALING_STRETCH;
            swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

            ComPtr<IDXGISwapChain1> swapChain1;
            ThrowIfFailed(m_factory->CreateSwapChainForCoreWindow(
                m_queue.Get(),
                reinterpret_cast<IUnknown*>(window),
                &swapDesc,
                nullptr,
                &swapChain1));

            ThrowIfFailed(swapChain1.As(&m_swapChain));
            m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = FrameCount;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)));

            m_rtvIncrement = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

            for (UINT i = 0; i < FrameCount; ++i)
            {
                ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
                m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, handle);
                handle.ptr += m_rtvIncrement;
                ThrowIfFailed(m_device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&m_allocators[i])));
            }

            ThrowIfFailed(m_device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                m_allocators[m_frameIndex].Get(),
                nullptr,
                IID_PPV_ARGS(&m_commandList)));
            ThrowIfFailed(m_commandList->Close());

            ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
            m_fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
            if (!m_fenceEvent)
            {
                ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        ~D3D12Probe()
        {
            WaitForGpu();
            if (m_fenceEvent)
            {
                CloseHandle(m_fenceEvent);
            }
        }

        void Render(bool aHeld)
        {
            ThrowIfFailed(m_allocators[m_frameIndex]->Reset());
            ThrowIfFailed(m_commandList->Reset(m_allocators[m_frameIndex].Get(), nullptr));

            D3D12_RESOURCE_BARRIER toTarget = {};
            toTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            toTarget.Transition.pResource = m_renderTargets[m_frameIndex].Get();
            toTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            toTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            toTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            m_commandList->ResourceBarrier(1, &toTarget);

            D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            rtv.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvIncrement;

            const float idle[4] = { 0.025f, 0.055f, 0.095f, 1.0f };
            const float active[4] = { 0.035f, 0.30f, 0.10f, 1.0f };
            const float* clear = aHeld ? active : idle;
            m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            m_commandList->ClearRenderTargetView(rtv, clear, 0, nullptr);

            D3D12_RESOURCE_BARRIER toPresent = toTarget;
            toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            m_commandList->ResourceBarrier(1, &toPresent);

            ThrowIfFailed(m_commandList->Close());
            ID3D12CommandList* lists[] = { m_commandList.Get() };
            m_queue->ExecuteCommandLists(1, lists);
            ThrowIfFailed(m_swapChain->Present(1, 0));

            WaitForGpu();
            m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        }

    private:
        void WaitForGpu()
        {
            if (!m_queue || !m_fence || !m_fenceEvent)
            {
                return;
            }

            const UINT64 value = ++m_fenceValue;
            ThrowIfFailed(m_queue->Signal(m_fence.Get(), value));
            if (m_fence->GetCompletedValue() < value)
            {
                ThrowIfFailed(m_fence->SetEventOnCompletion(value, m_fenceEvent));
                WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
            }
        }

        static constexpr UINT FrameCount = 2;
        ComPtr<IDXGIFactory4> m_factory;
        ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12CommandQueue> m_queue;
        ComPtr<IDXGISwapChain3> m_swapChain;
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
        ComPtr<ID3D12CommandAllocator> m_allocators[FrameCount];
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        ComPtr<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent = nullptr;
        UINT64 m_fenceValue = 0;
        UINT m_frameIndex = 0;
        UINT m_rtvIncrement = 0;
    };

    ref class App sealed : public IFrameworkView
    {
    public:
        App() {}

        virtual void Initialize(CoreApplicationView^ applicationView)
        {
            applicationView->Activated += ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(
                this, &App::OnActivated);

            auto values = ApplicationData::Current->LocalSettings->Values;
            int launchCount = 0;
            auto oldValue = values->Lookup("CP1LaunchCount");
            if (oldValue != nullptr)
            {
                auto boxed = dynamic_cast<IPropertyValue^>(oldValue);
                if (boxed != nullptr && boxed->Type == PropertyType::Int32)
                {
                    launchCount = boxed->GetInt32();
                }
            }
            values->Insert("CP1LaunchCount", PropertyValue::CreateInt32(launchCount + 1));
        }

        virtual void SetWindow(CoreWindow^ window)
        {
            window->Closed += ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(
                this, &App::OnClosed);
            window->VisibilityChanged += ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(
                this, &App::OnVisibilityChanged);

            m_renderer.Initialize(window);
        }

        virtual void Load(Platform::String^) {}

        virtual void Run()
        {
            auto window = CoreWindow::GetForCurrentThread();

            while (!m_closed)
            {
                if (m_visible)
                {
                    window->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
                    m_renderer.Render(IsGamepadAHeld());
                }
                else
                {
                    window->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
                }
            }
        }

        virtual void Uninitialize() {}

    private:
        bool IsGamepadAHeld()
        {
            auto pads = Gamepad::Gamepads;
            if (pads == nullptr || pads->Size == 0)
            {
                return false;
            }

            const auto reading = pads->GetAt(0)->GetCurrentReading();
            const auto buttons = static_cast<unsigned int>(reading.Buttons);
            const auto a = static_cast<unsigned int>(GamepadButtons::A);
            return (buttons & a) != 0;
        }

        void OnActivated(CoreApplicationView^, IActivatedEventArgs^)
        {
            CoreWindow::GetForCurrentThread()->Activate();
        }

        void OnClosed(CoreWindow^, CoreWindowEventArgs^)
        {
            m_closed = true;
        }

        void OnVisibilityChanged(CoreWindow^, VisibilityChangedEventArgs^ args)
        {
            m_visible = args->Visible;
        }

        CoreWindow^ m_window = nullptr;
        bool m_closed = false;
        bool m_visible = true;
        D3D12Probe m_renderer;
    };

    ref class AppSource sealed : public IFrameworkViewSource
    {
    public:
        virtual IFrameworkView^ CreateView()
        {
            return ref new App();
        }
    };
}

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
    CoreApplication::Run(ref new BlueWakeUWP::AppSource());
    return 0;
}
