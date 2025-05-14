#include "pch.h"
#include "CameraInterface.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <shlwapi.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include <propvarutil.h>
#include <mferror.h>
#include <mftransform.h>
#include <thread>
#include <windows.h>
#include <comdef.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

// --- Variables globales para preview ---
static IMFMediaSource* g_pSource = nullptr;
static IMFMediaSession* g_pSession = nullptr;
static IMFActivate** g_ppDevices = nullptr;
static UINT32 g_deviceCount = 0;
static IMFTopology* g_pTopology = nullptr;
static HWND g_hwndPreview = nullptr;
static std::vector<IMFActivate*> g_audioDevices;

// --- Variables globales para grabación ---
struct MediaCaptureContext {
    IMFMediaSource* videoSource = nullptr;
    IMFMediaSource* audioSource = nullptr;
    IMFSourceReader* videoReader = nullptr;
    IMFSourceReader* audioReader = nullptr;
    IMFSinkWriter* sinkWriter = nullptr;
    DWORD videoStreamIndex = 0;
    DWORD audioStreamIndex = 0;
    std::thread recordingThread;
    std::atomic<bool> isRecording = false;
    std::atomic<bool> isPaused = false;
};

static MediaCaptureContext g_ctx;

// --- Prototipos internos ---
bool InitializeRecorder(int videoIndex, int audioIndex, const wchar_t* outputPath);
bool PauseRecorder();
bool ResumeRecorder();
bool StopRecorder();

// --- Inicialización y limpieza ---
void InitializeCameraSystem() {
    MFStartup(MF_VERSION);
}

void ShutdownCameraSystem() {
    StopPreview();
    StopRecorder();

    for (auto dev : g_audioDevices) dev->Release();
    g_audioDevices.clear();

    if (g_ppDevices) {
        for (UINT32 i = 0; i < g_deviceCount; i++) {
            g_ppDevices[i]->Release();
        }
        CoTaskMemFree(g_ppDevices);
        g_ppDevices = nullptr;
    }

    MFShutdown();
}

// --- Enumeración y nombres de cámaras y micrófonos ---
int GetCameraCount() {
    IMFAttributes* pAttributes = nullptr;
    MFCreateAttributes(&pAttributes, 1);
    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (SUCCEEDED(MFEnumDeviceSources(pAttributes, &g_ppDevices, &g_deviceCount))) {
        pAttributes->Release();
        return g_deviceCount;
    }

    pAttributes->Release();
    return 0;
}

void GetCameraName(int index, wchar_t* nameBuffer, int bufferLength) {
    if (!g_ppDevices || index < 0 || index >= (int)g_deviceCount)
        return;

    WCHAR* szFriendlyName = nullptr;
    UINT32 cchName;

    g_ppDevices[index]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &szFriendlyName, &cchName);
    wcsncpy_s(nameBuffer, bufferLength, szFriendlyName, _TRUNCATE);
    CoTaskMemFree(szFriendlyName);
}

int GetMicrophoneCount() {
    for (auto dev : g_audioDevices) dev->Release();
    g_audioDevices.clear();

    IMFAttributes* pAttributes = nullptr;
    UINT32 count = 0;
    IMFActivate** ppDevices = nullptr;

    MFCreateAttributes(&pAttributes, 1);
    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
    if (SUCCEEDED(MFEnumDeviceSources(pAttributes, &ppDevices, &count))) {
        for (UINT32 i = 0; i < count; i++) {
            g_audioDevices.push_back(ppDevices[i]);
        }
    }

    if (pAttributes) pAttributes->Release();
    CoTaskMemFree(ppDevices);

    return static_cast<int>(g_audioDevices.size());
}

void GetMicrophoneName(int index, wchar_t* nameBuffer, int nameBufferSize) {
    if (index < 0 || index >= (int)g_audioDevices.size()) return;

    WCHAR* szFriendlyName = nullptr;
    UINT32 cchName;

    if (SUCCEEDED(g_audioDevices[index]->GetAllocatedString(
        MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
        &szFriendlyName, &cchName)))
    {
        wcsncpy_s(nameBuffer, nameBufferSize, szFriendlyName, _TRUNCATE);
        CoTaskMemFree(szFriendlyName);
    }
}

// --- Vista previa ---
HRESULT CreateMediaSourceFromDevice(int index, IMFMediaSource** ppSource) {
    if (!g_ppDevices || index >= (int)g_deviceCount) return E_FAIL;
    return g_ppDevices[index]->ActivateObject(IID_PPV_ARGS(ppSource));
}

bool StartPreview(int index, HWND hwnd) {
    StopPreview();
    g_hwndPreview = hwnd;

    if (FAILED(CreateMediaSourceFromDevice(index, &g_pSource))) return false;

    if (FAILED(MFCreateMediaSession(nullptr, &g_pSession))) return false;

    IMFPresentationDescriptor* pPD = nullptr;
    g_pSource->CreatePresentationDescriptor(&pPD);

    IMFStreamDescriptor* pSD = nullptr;
    BOOL fSelected;
    pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD);

    IMFActivate* pSinkActivate = nullptr;
    MFCreateVideoRendererActivate(hwnd, &pSinkActivate);

    MFCreateTopology(&g_pTopology);

    IMFTopologyNode* pSourceNode = nullptr;
    IMFTopologyNode* pOutputNode = nullptr;

    MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode);
    pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, g_pSource);
    pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
    pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
    g_pTopology->AddNode(pSourceNode);

    MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode);
    pOutputNode->SetObject(pSinkActivate);
    g_pTopology->AddNode(pOutputNode);

    pSourceNode->ConnectOutput(0, pOutputNode, 0);

    g_pSession->SetTopology(0, g_pTopology);

    PROPVARIANT varStart;
    PropVariantInit(&varStart);
    g_pSession->Start(&GUID_NULL, &varStart);
    PropVariantClear(&varStart);

    return true;
}

void StopPreview() {
    if (g_pSession) {
        g_pSession->Close();
        g_pSession->Shutdown();
        g_pSession->Release();
        g_pSession = nullptr;
    }

    if (g_pSource) {
        g_pSource->Shutdown();
        g_pSource->Release();
        g_pSource = nullptr;
    }

    if (g_pTopology) {
        g_pTopology->Release();
        g_pTopology = nullptr;
    }
}

bool PauseRecording() {
    return g_ctx.isRecording ? PauseRecorder() : false;
}

bool ResumeRecording() {
    return g_ctx.isRecording ? ResumeRecorder() : false;
}

bool StopRecording() {
    return g_ctx.isRecording ? StopRecorder() : false;
}

// --- Lógica interna de grabación ---
bool StartRecording(int videoIndex, int audioIndex, const wchar_t* outputPath) {
    HRESULT hr = S_OK;

    // 1. Activar fuentes de video/audio
    hr = g_ppDevices[videoIndex]->ActivateObject(IID_PPV_ARGS(&g_ctx.videoSource));
    if (FAILED(hr)) return false;

    hr = g_audioDevices[audioIndex]->ActivateObject(IID_PPV_ARGS(&g_ctx.audioSource));
    if (FAILED(hr)) return false;

    // 2. Crear SourceReaders
    hr = MFCreateSourceReaderFromMediaSource(g_ctx.videoSource, nullptr, &g_ctx.videoReader);
    if (FAILED(hr)) return false;

    hr = MFCreateSourceReaderFromMediaSource(g_ctx.audioSource, nullptr, &g_ctx.audioReader);
    if (FAILED(hr)) return false;

    // 2.1 Establecer tipo de salida deseado (video NV12)
    IMFMediaType* videoRequestType = nullptr;
    MFCreateMediaType(&videoRequestType);
    videoRequestType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    videoRequestType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    g_ctx.videoReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, videoRequestType);
    videoRequestType->Release();

    // 2.2 Establecer tipo deseado para audio PCM
    IMFMediaType* audioRequestType = nullptr;
    MFCreateMediaType(&audioRequestType);
    audioRequestType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audioRequestType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    g_ctx.audioReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, audioRequestType);
    audioRequestType->Release();

    // 3. Crear SinkWriter
    hr = MFCreateSinkWriterFromURL(outputPath, nullptr, nullptr, &g_ctx.sinkWriter);
    if (FAILED(hr)) return false;

    // 4. MediaType salida video (H264)
    IMFMediaType* videoOutType = nullptr;
    MFCreateMediaType(&videoOutType);
    videoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    videoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    videoOutType->SetUINT32(MF_MT_AVG_BITRATE, 8000000);
    videoOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(videoOutType, MF_MT_FRAME_SIZE, 1280, 720);
    MFSetAttributeRatio(videoOutType, MF_MT_FRAME_RATE, 30, 1);
    MFSetAttributeRatio(videoOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    hr = g_ctx.sinkWriter->AddStream(videoOutType, &g_ctx.videoStreamIndex);
    videoOutType->Release();
    if (FAILED(hr)) return false;

    // 5. MediaType entrada video (NV12)
    IMFMediaType* videoInType = nullptr;
    MFCreateMediaType(&videoInType);
    videoInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    videoInType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(videoInType, MF_MT_FRAME_SIZE, 1280, 720);
    MFSetAttributeRatio(videoInType, MF_MT_FRAME_RATE, 30, 1);
    MFSetAttributeRatio(videoInType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    hr = g_ctx.sinkWriter->SetInputMediaType(g_ctx.videoStreamIndex, videoInType, nullptr);
    videoInType->Release();
    if (FAILED(hr)) return false;

    // 6. MediaType salida audio (AAC)
    IMFMediaType* audioOutType = nullptr;
    MFCreateMediaType(&audioOutType);
    audioOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audioOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    audioOutType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    audioOutType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    audioOutType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    audioOutType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 192000);
    audioOutType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
    hr = g_ctx.sinkWriter->AddStream(audioOutType, &g_ctx.audioStreamIndex);
    audioOutType->Release();
    if (FAILED(hr)) return false;

    // 7. MediaType entrada audio (PCM)
    IMFMediaType* audioInType = nullptr;
    MFCreateMediaType(&audioInType);
    audioInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audioInType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    audioInType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    audioInType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    audioInType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    hr = g_ctx.sinkWriter->SetInputMediaType(g_ctx.audioStreamIndex, audioInType, nullptr);
    audioInType->Release();
    if (FAILED(hr)) return false;

    // 8. Comenzar grabación
    hr = g_ctx.sinkWriter->BeginWriting();
    if (FAILED(hr)) return false;

    g_ctx.isRecording = true;
    g_ctx.isPaused = false;

    g_ctx.recordingThread = std::thread([] {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG videoRtStart = 0;
        LONGLONG audioRtStart = 0;
        const LONGLONG videoFrameDuration = 333333; // 30 fps
        const LONGLONG audioSampleDuration = 100000; // ~10ms

        while (g_ctx.isRecording) {
            if (g_ctx.isPaused) {
                Sleep(10);
                continue;
            }

            IMFSample* pSample = nullptr;

            // Video
            HRESULT hr = g_ctx.videoReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, nullptr, &pSample);
            if (SUCCEEDED(hr) && pSample) {
                pSample->SetSampleTime(videoRtStart);
                pSample->SetSampleDuration(videoFrameDuration);
                g_ctx.sinkWriter->WriteSample(g_ctx.videoStreamIndex, pSample);
                videoRtStart += videoFrameDuration;
                pSample->Release();
            }

            // Audio
            pSample = nullptr;
            hr = g_ctx.audioReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, nullptr, &pSample);
            if (SUCCEEDED(hr) && pSample) {
                pSample->SetSampleTime(audioRtStart);
                pSample->SetSampleDuration(audioSampleDuration);
                g_ctx.sinkWriter->WriteSample(g_ctx.audioStreamIndex, pSample);
                audioRtStart += audioSampleDuration;
                pSample->Release();
            }

            Sleep(1);
        }

        g_ctx.sinkWriter->Finalize();
        g_ctx.sinkWriter->Release(); g_ctx.sinkWriter = nullptr;
        g_ctx.videoReader->Release(); g_ctx.videoReader = nullptr;
        g_ctx.audioReader->Release(); g_ctx.audioReader = nullptr;
        g_ctx.videoSource->Release(); g_ctx.videoSource = nullptr;
        g_ctx.audioSource->Release(); g_ctx.audioSource = nullptr;
        });
        g_ctx.recordingThread = std::thread([] {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG videoRtStart = 0;
        LONGLONG audioRtStart = 0;

        const LONGLONG VIDEO_FRAME_DURATION = 333333; // 30 fps
        const LONGLONG AUDIO_FRAME_DURATION = 10000000 / 100; // 100 fps aprox.

        while (g_ctx.isRecording) {
            if (g_ctx.isPaused) {
                Sleep(10);
                continue;
            }

            IMFSample* pSample = nullptr;

            // Leer y escribir video
            HRESULT hr = g_ctx.videoReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, nullptr, &pSample);
            if (SUCCEEDED(hr) && pSample) {
                pSample->SetSampleTime(videoRtStart);
                pSample->SetSampleDuration(VIDEO_FRAME_DURATION);
                g_ctx.sinkWriter->WriteSample(g_ctx.videoStreamIndex, pSample);
                pSample->Release();
                videoRtStart += VIDEO_FRAME_DURATION;
            }

            // Leer y escribir audio
            pSample = nullptr;
            hr = g_ctx.audioReader->ReadSample(
                MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, nullptr, &pSample);
            if (SUCCEEDED(hr) && pSample) {
                pSample->SetSampleTime(audioRtStart);
                pSample->SetSampleDuration(AUDIO_FRAME_DURATION);
                g_ctx.sinkWriter->WriteSample(g_ctx.audioStreamIndex, pSample);
                pSample->Release();
                audioRtStart += AUDIO_FRAME_DURATION;
            }

            Sleep(1);
        }

        g_ctx.sinkWriter->Finalize();
        g_ctx.sinkWriter->Release(); g_ctx.sinkWriter = nullptr;
        g_ctx.videoReader->Release(); g_ctx.videoReader = nullptr;
        g_ctx.audioReader->Release(); g_ctx.audioReader = nullptr;
        g_ctx.videoSource->Release(); g_ctx.videoSource = nullptr;
        g_ctx.audioSource->Release(); g_ctx.audioSource = nullptr;
        });
    return true;
}



bool PauseRecorder() {
    g_ctx.isPaused = true;
    return true;
}

bool ResumeRecorder() {
    g_ctx.isPaused = false;
    return true;
}

bool StopRecorder() {
    if (!g_ctx.isRecording) return false;
    g_ctx.isRecording = false;
    g_ctx.isPaused = false;

    // Liberar recursos si estuvieran asignados
    if (g_ctx.videoReader) { g_ctx.videoReader->Release(); g_ctx.videoReader = nullptr; }
    if (g_ctx.audioReader) { g_ctx.audioReader->Release(); g_ctx.audioReader = nullptr; }
    if (g_ctx.sinkWriter) { g_ctx.sinkWriter->Release(); g_ctx.sinkWriter = nullptr; }
    if (g_ctx.videoSource) { g_ctx.videoSource->Release(); g_ctx.videoSource = nullptr; }
    if (g_ctx.audioSource) { g_ctx.audioSource->Release(); g_ctx.audioSource = nullptr; }

    if (g_ctx.recordingThread.joinable())
        g_ctx.recordingThread.join();

    return true;
}
