#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdint.h>
#include <cmath>
#include <ppltasks.h>

using Microsoft::WRL::ComPtr;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Gaming::Input;
using namespace Windows::Media;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Devices;
using namespace Windows::Media::Render;
using namespace Windows::Storage;
using namespace Windows::UI::Core;

extern "C" bool bluewake_cp2_runtime_self_test(void);
extern "C" bool bluewake_cp3_storage_self_test(
    const wchar_t* localFolder, uint8_t* beforeOut, uint8_t* afterOut);
extern "C" bool bluewake_cp3_module_self_test(void);
extern "C" bool bluewake_cp4_real_module_self_test(void);
extern "C" bool bluewake_cp5_chassis_self_test(void);

MIDL_INTERFACE("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")
IMemoryBufferByteAccess : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(
        BYTE** value,
        UINT32* capacity) = 0;
};

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

        void Render(
            bool aHeld,
            bool runtimeOk,
            bool storageOk,
            bool moduleOk,
            bool realModuleOk,
            bool chassisOk,
            int audioStage,
            int priorCrashStage,
            bool audioFailed)
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

            const float pass[4] = { 0.025f, 0.055f, 0.095f, 1.0f };
            const float runtimeFail[4] = { 0.30f, 0.015f, 0.015f, 1.0f };
            const float storageFail[4] = { 0.35f, 0.10f, 0.01f, 1.0f };
            const float moduleFail[4] = { 0.24f, 0.02f, 0.28f, 1.0f };
            const float realModuleFail[4] = { 0.34f, 0.28f, 0.01f, 1.0f };
            const float chassisFail[4] = { 0.01f, 0.24f, 0.30f, 1.0f };
            const float audioFail[4] = { 0.42f, 0.00f, 0.00f, 1.0f };
            const float stage1[4] = { 0.00f, 0.18f, 0.35f, 1.0f };
            const float stage2[4] = { 0.18f, 0.02f, 0.30f, 1.0f };
            const float stage3[4] = { 0.35f, 0.12f, 0.00f, 1.0f };
            const float stage4[4] = { 0.38f, 0.38f, 0.38f, 1.0f };
            const float stage5[4] = { 0.00f, 0.28f, 0.18f, 1.0f };
            const float crash1[4] = { 0.00f, 0.70f, 1.00f, 1.0f };
            const float crash2[4] = { 0.65f, 0.00f, 0.85f, 1.0f };
            const float crash3[4] = { 1.00f, 0.35f, 0.00f, 1.0f };
            const float crash4[4] = { 1.00f, 1.00f, 1.00f, 1.0f };
            const float crash5[4] = { 0.00f, 1.00f, 0.55f, 1.0f };
            const float active[4] = { 0.035f, 0.30f, 0.10f, 1.0f };

            const float* clear = pass;
            if (!runtimeOk)
                clear = runtimeFail;
            else if (!storageOk)
                clear = storageFail;
            else if (!moduleOk)
                clear = moduleFail;
            else if (!realModuleOk)
                clear = realModuleFail;
            else if (!chassisOk)
                clear = chassisFail;
            else if (audioFailed)
                clear = audioFail;
            else if (priorCrashStage == 1)
                clear = crash1;
            else if (priorCrashStage == 2)
                clear = crash2;
            else if (priorCrashStage == 3)
                clear = crash3;
            else if (priorCrashStage == 4)
                clear = crash4;
            else if (priorCrashStage == 5)
                clear = crash5;
            else if (audioStage == 1)
                clear = stage1;
            else if (audioStage == 2)
                clear = stage2;
            else if (audioStage == 3)
                clear = stage3;
            else if (audioStage == 4)
                clear = stage4;
            else if (audioStage >= 5)
                clear = stage5;

            if (aHeld)
                clear = active;
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

            m_runtimeOk = bluewake_cp2_runtime_self_test();
            values->Insert("CP2RuntimeProbe", PropertyValue::CreateBoolean(m_runtimeOk));

            uint8_t before = 0u;
            uint8_t after = 0u;
            auto localFolder = ApplicationData::Current->LocalFolder;
            m_storageOk = bluewake_cp3_storage_self_test(
                localFolder->Path->Data(), &before, &after);

            auto expectedValue = values->Lookup("CP3ExpectedSramMarker");
            if (expectedValue != nullptr)
            {
                auto expected = dynamic_cast<IPropertyValue^>(expectedValue);
                if (expected == nullptr ||
                    expected->Type != PropertyType::Int32 ||
                    (uint8_t)expected->GetInt32() != before)
                {
                    m_storageOk = false;
                }
            }

            values->Insert(
                "CP3ExpectedSramMarker",
                PropertyValue::CreateInt32((int)after));
            values->Insert(
                "CP3StorageProbe",
                PropertyValue::CreateBoolean(m_storageOk));

            m_moduleOk = bluewake_cp3_module_self_test();
            values->Insert(
                "CP3ModuleProbe",
                PropertyValue::CreateBoolean(m_moduleOk));

            m_realModuleOk = bluewake_cp4_real_module_self_test();
            values->Insert(
                "CP4RealModuleProbe",
                PropertyValue::CreateBoolean(m_realModuleOk));

            m_chassisOk = bluewake_cp5_chassis_self_test();
            values->Insert(
                "CP5ChassisProbe",
                PropertyValue::CreateBoolean(m_chassisOk));

            int attempt = 0;
            int completed = 0;

            auto attemptValue = values->Lookup("CP65AudioAttemptStage");
            if (attemptValue != nullptr)
            {
                auto boxed = dynamic_cast<IPropertyValue^>(attemptValue);
                if (boxed != nullptr && boxed->Type == PropertyType::Int32)
                    attempt = boxed->GetInt32();
            }

            auto completedValue = values->Lookup("CP65AudioCompletedStage");
            if (completedValue != nullptr)
            {
                auto boxed = dynamic_cast<IPropertyValue^>(completedValue);
                if (boxed != nullptr && boxed->Type == PropertyType::Int32)
                    completed = boxed->GetInt32();
            }

            if (attempt > completed)
                m_priorCrashStage = attempt;

            // Every new launch starts from a clean known-good CP5 audio state.
            values->Insert(
                "CP65AudioAttemptStage",
                PropertyValue::CreateInt32(0));
            values->Insert(
                "CP65AudioCompletedStage",
                PropertyValue::CreateInt32(0));
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

                    const bool bHeld = IsGamepadBHeld();
                    if (bHeld && !m_bWasHeld && m_priorCrashStage == 0)
                        AdvanceAudioStage();
                    m_bWasHeld = bHeld;

                    m_renderer.Render(
                        IsGamepadAHeld(), m_runtimeOk, m_storageOk,
                        m_moduleOk, m_realModuleOk, m_chassisOk,
                        m_audioStage, m_priorCrashStage, m_audioFailed);
                }
                else
                {
                    window->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
                }
            }
        }

        virtual void Uninitialize() {}

    private:
        bool IsGamepadBHeld()
        {
            auto pads = Gamepad::Gamepads;
            if (pads == nullptr || pads->Size == 0)
                return false;

            const auto reading = pads->GetAt(0)->GetCurrentReading();
            const auto buttons = static_cast<unsigned int>(reading.Buttons);
            const auto b = static_cast<unsigned int>(GamepadButtons::B);
            return (buttons & b) != 0;
        }

        AudioFrame^ CreatePreparedToneFrame()
        {
            if (m_audioGraph == nullptr || m_audioFrameInput == nullptr)
                return nullptr;

            auto encoding = m_audioGraph->EncodingProperties;
            if (encoding == nullptr ||
                encoding->SampleRate == 0u ||
                encoding->ChannelCount == 0u)
                return nullptr;

            const uint32_t rate = encoding->SampleRate;
            const uint32_t channels = encoding->ChannelCount;
            const uint32_t frames = rate * 2u;
            const uint32_t floatCount = frames * channels;
            const uint32_t byteCount = floatCount * sizeof(float);

            auto frame = ref new AudioFrame(byteCount);
            auto buffer = frame->LockBuffer(AudioBufferAccessMode::Write);
            auto reference = buffer->CreateReference();

            ComPtr<IMemoryBufferByteAccess> byteAccess;
            ThrowIfFailed(reinterpret_cast<IInspectable*>(reference)
                ->QueryInterface(IID_PPV_ARGS(&byteAccess)));

            BYTE* bytes = nullptr;
            UINT32 capacity = 0u;
            ThrowIfFailed(byteAccess->GetBuffer(&bytes, &capacity));
            if (bytes == nullptr || capacity < byteCount)
                return nullptr;

            float* samples = reinterpret_cast<float*>(bytes);
            constexpr double pi = 3.14159265358979323846;
            const uint32_t fadeFrames = rate >= 100u ? rate / 100u : 1u;

            for (uint32_t i = 0u; i < frames; ++i)
            {
                const bool second = i >= rate;
                const uint32_t noteFrame = second ? i - rate : i;
                const double freq = second ? 880.0 : 660.0;
                double envelope = 1.0;
                if (noteFrame < fadeFrames)
                    envelope =
                        static_cast<double>(noteFrame) /
                        static_cast<double>(fadeFrames);
                else if (rate - noteFrame < fadeFrames)
                    envelope =
                        static_cast<double>(rate - noteFrame) /
                        static_cast<double>(fadeFrames);

                const float sample = static_cast<float>(
                    std::sin(
                        2.0 * pi * freq *
                        static_cast<double>(i) /
                        static_cast<double>(rate)) *
                    0.55 * envelope);

                for (uint32_t ch = 0u; ch < channels; ++ch)
                    samples[i * channels + ch] = sample;
            }

            return frame;
        }

        bool RunAudioStage(int stage)
        {
            try
            {
                if (stage == 1)
                {
                    auto devices = concurrency::create_task(
                        DeviceInformation::FindAllAsync(
                            MediaDevice::GetAudioRenderSelector())).get();
                    if (devices == nullptr || devices->Size == 0u)
                        return false;

                    m_audioDevice = devices->GetAt(0);
                    return m_audioDevice != nullptr;
                }

                if (stage == 2)
                {
                    if (m_audioDevice == nullptr)
                        return false;

                    auto settings =
                        ref new AudioGraphSettings(AudioRenderCategory::Media);
                    settings->QuantumSizeSelectionMode =
                        QuantumSizeSelectionMode::SystemDefault;
                    settings->PrimaryRenderDevice = m_audioDevice;

                    auto result = concurrency::create_task(
                        AudioGraph::CreateAsync(settings)).get();
                    if (result == nullptr ||
                        result->Status != AudioGraphCreationStatus::Success ||
                        result->Graph == nullptr)
                        return false;

                    m_audioGraph = result->Graph;
                    return true;
                }

                if (stage == 3)
                {
                    if (m_audioGraph == nullptr)
                        return false;

                    auto result = concurrency::create_task(
                        m_audioGraph->CreateDeviceOutputNodeAsync()).get();
                    if (result == nullptr ||
                        result->Status !=
                            AudioDeviceNodeCreationStatus::Success ||
                        result->DeviceOutputNode == nullptr)
                        return false;

                    m_audioOutput = result->DeviceOutputNode;
                    return true;
                }

                if (stage == 4)
                {
                    if (m_audioGraph == nullptr || m_audioOutput == nullptr)
                        return false;

                    auto encoding = m_audioGraph->EncodingProperties;
                    if (encoding == nullptr)
                        return false;

                    m_audioFrameInput =
                        m_audioGraph->CreateFrameInputNode(encoding);
                    if (m_audioFrameInput == nullptr)
                        return false;

                    m_audioFrameInput->AddOutgoingConnection(m_audioOutput);
                    m_audioFrameInput->Stop();

                    auto frame = CreatePreparedToneFrame();
                    if (frame == nullptr)
                        return false;

                    m_audioFrameInput->AddFrame(frame);
                    return true;
                }

                if (stage == 5)
                {
                    if (m_audioGraph == nullptr || m_audioFrameInput == nullptr)
                        return false;

                    m_audioGraph->Start();
                    m_audioFrameInput->Start();
                    return true;
                }
            }
            catch (Platform::Exception^)
            {
                return false;
            }

            return false;
        }

        void AdvanceAudioStage()
        {
            if (m_audioStage >= 5 || m_audioFailed)
                return;

            const int nextStage = m_audioStage + 1;
            auto values = ApplicationData::Current->LocalSettings->Values;
            values->Insert(
                "CP65AudioAttemptStage",
                PropertyValue::CreateInt32(nextStage));

            const bool ok = RunAudioStage(nextStage);
            if (!ok)
            {
                m_audioFailed = true;
                values->Insert(
                    "CP65AudioFailedStage",
                    PropertyValue::CreateInt32(nextStage));
                return;
            }

            m_audioStage = nextStage;
            values->Insert(
                "CP65AudioCompletedStage",
                PropertyValue::CreateInt32(nextStage));
        }

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

        bool m_closed = false;
        bool m_visible = true;
        bool m_runtimeOk = false;
        bool m_storageOk = false;
        bool m_moduleOk = false;
        bool m_realModuleOk = false;
        bool m_chassisOk = false;
        bool m_bWasHeld = false;
        bool m_audioFailed = false;
        int m_audioStage = 0;
        int m_priorCrashStage = 0;
        DeviceInformation^ m_audioDevice = nullptr;
        AudioGraph^ m_audioGraph = nullptr;
        AudioDeviceOutputNode^ m_audioOutput = nullptr;
        AudioFrameInputNode^ m_audioFrameInput = nullptr;
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
