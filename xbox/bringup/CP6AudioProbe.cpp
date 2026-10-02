#include <windows.h>
#include <xaudio2.h>
#include <stdint.h>
#include <cmath>
#include <vector>

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
    constexpr uint32_t frames = sampleRate * 2u;
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
    constexpr uint32_t fadeFrames = 640u;
    for (uint32_t frame = 0; frame < frames; ++frame) {
        const double t = static_cast<double>(frame) / sampleRate;
        double envelope = 1.0;
        if (frame < fadeFrames)
            envelope = static_cast<double>(frame) / fadeFrames;
        else if (frame > frames - fadeFrames)
            envelope = static_cast<double>(frames - frame) / fadeFrames;

        // Two clearly separated notes make the test hard to miss on a TV.
        const double frequency = frame < sampleRate ? 660.0 : 880.0;
        const int16_t sample = static_cast<int16_t>(
            std::sin(2.0 * pi * frequency * t) * 15000.0 * envelope);
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

    hr = s_engine->StartEngine();
    if (FAILED(hr))
        return false;

    hr = s_source->SetVolume(0.85f);
    if (FAILED(hr))
        return false;

    hr = s_source->Start(0);
    if (FAILED(hr))
        return false;

    // API success alone was a false positive on CP6.1. Give the Xbox audio
    // graph time to run, then prove that this source voice consumed samples.
    Sleep(350);

    XAUDIO2_VOICE_STATE state{};
    s_source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    // NOSAMPLESPLAYED intentionally suppresses the counter, so fetch again
    // without it for the actual runtime proof.
    s_source->GetState(&state, 0);
    return state.SamplesPlayed > 0u && state.BuffersQueued > 0u;
}
