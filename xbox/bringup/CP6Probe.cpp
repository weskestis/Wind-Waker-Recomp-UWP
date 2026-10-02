#include <windows.h>
#include <xaudio2.h>
#include <stdint.h>
#include <cmath>
#include <vector>

#include <webgpu/webgpu_cpp.h>

namespace {

bool request_adapter(
    const wgpu::Instance& instance,
    const wgpu::Surface& surface,
    wgpu::Adapter& adapter)
{
    bool callbackDone = false;
    wgpu::RequestAdapterStatus callbackStatus =
        wgpu::RequestAdapterStatus::CallbackCancelled;

    const wgpu::RequestAdapterOptions options{
        .featureLevel = wgpu::FeatureLevel::Compatibility,
        .powerPreference = wgpu::PowerPreference::HighPerformance,
        .backendType = wgpu::BackendType::D3D12,
        .compatibleSurface = surface,
    };

    const auto future = instance.RequestAdapter(
        &options,
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status,
            wgpu::Adapter result,
            wgpu::StringView) {
            callbackDone = true;
            callbackStatus = status;
            if (status == wgpu::RequestAdapterStatus::Success)
                adapter = std::move(result);
        });

    return instance.WaitAny(future, 5000000000ull) == wgpu::WaitStatus::Success &&
           callbackDone &&
           callbackStatus == wgpu::RequestAdapterStatus::Success &&
           static_cast<bool>(adapter);
}

bool request_device(
    const wgpu::Instance& instance,
    const wgpu::Adapter& adapter,
    wgpu::Device& device)
{
    bool callbackDone = false;
    wgpu::RequestDeviceStatus callbackStatus =
        wgpu::RequestDeviceStatus::CallbackCancelled;

    wgpu::DeviceDescriptor descriptor{};
    const auto future = adapter.RequestDevice(
        &descriptor,
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status,
            wgpu::Device result,
            wgpu::StringView) {
            callbackDone = true;
            callbackStatus = status;
            if (status == wgpu::RequestDeviceStatus::Success)
                device = std::move(result);
        });

    return instance.WaitAny(future, 5000000000ull) == wgpu::WaitStatus::Success &&
           callbackDone &&
           callbackStatus == wgpu::RequestDeviceStatus::Success &&
           static_cast<bool>(device);
}

wgpu::TextureFormat choose_surface_format(const wgpu::SurfaceCapabilities& caps)
{
    for (size_t i = 0; i < caps.formatCount; ++i) {
        if (caps.formats[i] == wgpu::TextureFormat::BGRA8Unorm ||
            caps.formats[i] == wgpu::TextureFormat::RGBA8Unorm)
            return caps.formats[i];
    }
    return caps.formatCount != 0
        ? caps.formats[0]
        : wgpu::TextureFormat::Undefined;
}

} // namespace

extern "C" bool bluewake_cp6_dawn_self_test(
    void* coreWindow,
    uint32_t width,
    uint32_t height)
{
    if (coreWindow == nullptr || width == 0 || height == 0)
        return false;

    const wgpu::InstanceFeatureName requiredFeatures[] = {
        wgpu::InstanceFeatureName::TimedWaitAny,
    };
    wgpu::InstanceDescriptor instanceDescriptor{
        .requiredFeatureCount = 1,
        .requiredFeatures = requiredFeatures,
    };

    wgpu::Instance instance = wgpu::CreateInstance(&instanceDescriptor);
    if (!instance)
        return false;

    wgpu::SurfaceDescriptorFromWindowsCoreWindow coreWindowDescriptor{};
    coreWindowDescriptor.coreWindow = coreWindow;

    const wgpu::SurfaceDescriptor surfaceDescriptor{
        .nextInChain = &coreWindowDescriptor,
        .label = "BlueWake Xbox CP6 CoreWindow",
    };

    wgpu::Surface surface = instance.CreateSurface(&surfaceDescriptor);
    if (!surface)
        return false;

    wgpu::Adapter adapter;
    if (!request_adapter(instance, surface, adapter))
        return false;

    wgpu::AdapterInfo adapterInfo{};
    if (adapter.GetInfo(&adapterInfo) != wgpu::Status::Success ||
        adapterInfo.backendType != wgpu::BackendType::D3D12)
        return false;

    wgpu::Device device;
    if (!request_device(instance, adapter, device))
        return false;

    wgpu::SurfaceCapabilities caps{};
    if (surface.GetCapabilities(adapter, &caps) != wgpu::Status::Success ||
        caps.formatCount == 0)
        return false;

    const wgpu::TextureFormat format = choose_surface_format(caps);
    if (format == wgpu::TextureFormat::Undefined)
        return false;

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = format,
        .usage = wgpu::TextureUsage::RenderAttachment,
        .width = width,
        .height = height,
        .presentMode = wgpu::PresentMode::Fifo,
    };
    surface.Configure(&config);

    wgpu::SurfaceTexture current{};
    surface.GetCurrentTexture(&current);
    if (!current.texture)
        return false;

    const auto view = current.texture.CreateView();
    if (!view)
        return false;

    const wgpu::RenderPassColorAttachment colorAttachment{
        .view = view,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = { 0.02, 0.08, 0.18, 1.0 },
    };
    const wgpu::RenderPassDescriptor renderPassDescriptor{
        .label = "BlueWake Xbox CP6 Dawn clear",
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    };

    const auto encoder = device.CreateCommandEncoder();
    if (!encoder)
        return false;
    auto pass = encoder.BeginRenderPass(&renderPassDescriptor);
    pass.End();

    const auto commands = encoder.Finish();
    if (!commands)
        return false;

    const auto queue = device.GetQueue();
    queue.Submit(1, &commands);

    if (surface.Present() != wgpu::Status::Success)
        return false;

    surface.Unconfigure();

    // All WebGPU objects are local on purpose. CP6 proves that Dawn can create
    // and present through the real Xbox/UWP CoreWindow, then releases the
    // surface so the already-certified CP1 D3D12 probe can own it afterward.
    return true;
}

extern "C" bool bluewake_cp6_audio_self_test(void)
{
    static IXAudio2* s_engine = nullptr;
    static IXAudio2MasteringVoice* s_master = nullptr;
    static IXAudio2SourceVoice* s_source = nullptr;
    static std::vector<int16_t> s_tone;

    if (s_engine != nullptr)
        return true;

    HRESULT hr = XAudio2Create(&s_engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr) || s_engine == nullptr)
        return false;

    hr = s_engine->CreateMasteringVoice(
        &s_master,
        XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE,
        0,
        nullptr,
        nullptr,
        AudioCategory_GameMedia);
    if (FAILED(hr) || s_master == nullptr)
        return false;

    constexpr uint32_t sampleRate = 32000;
    constexpr uint32_t frames = 3840; // 120 ms
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign =
        static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    hr = s_engine->CreateSourceVoice(
        &s_source,
        &format,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        nullptr,
        nullptr,
        nullptr);
    if (FAILED(hr) || s_source == nullptr)
        return false;

    s_tone.resize(frames * 2u);
    constexpr double pi = 3.14159265358979323846;
    for (uint32_t frame = 0; frame < frames; ++frame) {
        const double t = static_cast<double>(frame) / sampleRate;
        const double envelope =
            frame < 160u ? static_cast<double>(frame) / 160.0 :
            frame > frames - 320u
                ? static_cast<double>(frames - frame) / 320.0
                : 1.0;
        const int16_t sample = static_cast<int16_t>(
            std::sin(2.0 * pi * 440.0 * t) * 5000.0 * envelope);
        s_tone[frame * 2u] = sample;
        s_tone[frame * 2u + 1u] = sample;
    }

    XAUDIO2_BUFFER buffer{};
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes =
        static_cast<UINT32>(s_tone.size() * sizeof(s_tone[0]));
    buffer.pAudioData =
        reinterpret_cast<const BYTE*>(s_tone.data());

    hr = s_source->SubmitSourceBuffer(&buffer);
    if (FAILED(hr))
        return false;

    hr = s_source->Start(0);
    return SUCCEEDED(hr);
}
