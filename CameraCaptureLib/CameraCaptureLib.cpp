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
#include <windows.h>
#include <comdef.h>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <mfplay.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

std::thread videoThread;
std::thread audioThread;

// --- Variables globales ---
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
    std::thread audioThread;
    std::thread recordingThread;
    std::atomic<bool> isRecording = false;
    std::atomic<bool> isPaused = false;
    std::atomic<LONGLONG> baseTime{ -1 };
};
static MediaCaptureContext g_ctx;
static MediaCaptureContext g_ctx2;

// --- Prototipos internos ---
bool InitializeRecorder(int videoIndex, int audioIndex, const wchar_t* outputPath);
bool PauseRecorder();
bool ResumeRecorder();

void ResetContext() {
    g_ctx.videoStreamIndex = 0;
    g_ctx.audioStreamIndex = 0;
    g_ctx.baseTime = -1;
}

bool StopRecorder() {
    if (!g_ctx.isRecording) return false;

    g_ctx.isRecording = false;
    g_ctx.isPaused = false;

    // Esperar que los hilos terminen
    if (g_ctx.recordingThread.joinable()) g_ctx.recordingThread.join();
    if (g_ctx.audioThread.joinable()) g_ctx.audioThread.join();

    HRESULT hr = S_OK;

    if (g_ctx.sinkWriter) {
        hr = g_ctx.sinkWriter->Finalize();
        if (FAILED(hr)) std::cout << "[ERROR] SinkWriter->Finalize falló: 0x" << std::hex << hr << "\n";
        g_ctx.sinkWriter->Release();
        g_ctx.sinkWriter = nullptr;
    }

    if (g_ctx.videoReader) {
        g_ctx.videoReader->Release();
        g_ctx.videoReader = nullptr;
    }

    if (g_ctx.audioReader) {
        g_ctx.audioReader->Release();
        g_ctx.audioReader = nullptr;
    }

    if (g_ctx.videoSource) {
        g_ctx.videoSource->Shutdown();
        g_ctx.videoSource->Release();
        g_ctx.videoSource = nullptr;
    }

    if (g_ctx.audioSource) {
        g_ctx.audioSource->Shutdown();  // Recomendado: hacer shutdown explícito
        g_ctx.audioSource->Release();
        g_ctx.audioSource = nullptr;
    }

    ResetContext();

    std::cout << "[INFO] Grabación detenida correctamente.\n";
    return true;
}

// --- Inicialización y limpieza ---
void InitializeCameraSystem()
{
    if (g_ctx.videoReader != nullptr)
        return; // ya está inicializado

    // Inicializa Media Foundation
    MFStartup(MF_VERSION);

    // Crear y configurar IMFSourceReader, asignarlo a g_ctx.videoReader
    // Aquí deberías seleccionar una cámara y crear el lector con ella
}

void ShutdownCameraSystem() {
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

bool StartPreview(int index, HWND hwnd, PreviewSession* session)
{
    StopPreview(session); // ← también necesitarás una versión específica por sesión

    IMFPresentationDescriptor* pPD = nullptr;
    IMFStreamDescriptor* pSD = nullptr;
    IMFActivate* pSinkActivate = nullptr;
    IMFTopologyNode* pSourceNode = nullptr;
    IMFTopologyNode* pOutputNode = nullptr;

    HRESULT hr;

    if (FAILED(hr = CreateMediaSourceFromDevice(index, &session->pSource))) return false;
    if (FAILED(hr = MFCreateMediaSession(nullptr, &session->pSession))) return false;
    if (FAILED(hr = session->pSource->CreatePresentationDescriptor(&pPD))) return false;

    BOOL fSelected;
    if (FAILED(hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD))) return false;
    if (FAILED(hr = MFCreateVideoRendererActivate(hwnd, &pSinkActivate))) return false;
    if (FAILED(hr = MFCreateTopology(&session->pTopology))) return false;

    if (FAILED(hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode))) return false;
    pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, session->pSource);
    pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
    pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
    session->pTopology->AddNode(pSourceNode);

    if (FAILED(hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode))) return false;
    pOutputNode->SetObject(pSinkActivate);
    session->pTopology->AddNode(pOutputNode);
    pSourceNode->ConnectOutput(0, pOutputNode, 0);

    hr = session->pSession->SetTopology(0, session->pTopology);

    PROPVARIANT varStart;
    PropVariantInit(&varStart);
    hr = session->pSession->Start(&GUID_NULL, &varStart);
    PropVariantClear(&varStart);

    // Limpieza local (no limpia la sesión que sigue viva)
    pPD->Release();
    pSD->Release();
    pSinkActivate->Release();
    pSourceNode->Release();
    pOutputNode->Release();

    return SUCCEEDED(hr);
}


void StopPreview(PreviewSession* session)
{
    if (!session) return;

    if (session->pSession) {
        HRESULT hr;

        // Detener sesión
        hr = session->pSession->Stop();
        if (FAILED(hr)) {
            OutputDebugString(L"[ERROR] Stop falló\n");
        }

        // Cerrar sesión
        hr = session->pSession->Close();
        if (FAILED(hr)) {
            OutputDebugString(L"[ERROR] Close falló\n");
        }

        // Esperar a que se cierre
        IMFMediaEvent* pEvent = nullptr;
        MediaEventType meType = MEUnknown;

        while (SUCCEEDED(session->pSession->GetEvent(0, &pEvent))) {
            if (SUCCEEDED(pEvent->GetType(&meType))) {
                if (meType == MESessionClosed) {
                    pEvent->Release();
                    break;
                }
            }
            pEvent->Release();
        }

        session->pSession->Shutdown();
        session->pSession->Release();
        session->pSession = nullptr;
    }

    if (session->hwndPreview) {
        InvalidateRect(session->hwndPreview, NULL, TRUE);
        UpdateWindow(session->hwndPreview);
        session->hwndPreview = nullptr;
    }

    if (session->pSource) {
        session->pSource->Release();
        session->pSource = nullptr;
    }

    if (session->pTopology) {
        session->pTopology->Release();
        session->pTopology = nullptr;
    }

    std::cout << g_pSession << std::endl;
    std::cout << g_hwndPreview << std::endl;
    std::cout << g_pSource << std::endl;
    std::cout << g_pTopology << std::endl;
}

bool PauseRecording() { return g_ctx.isRecording ? PauseRecorder() : false; }
bool ResumeRecording() { return g_ctx.isRecording ? ResumeRecorder() : false; }
bool StopRecording() { return g_ctx.isRecording ? StopRecorder() : false; }

HRESULT GetCameraDeviceActivate(UINT32 camIndex, IMFActivate** ppActivate) {
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) goto done;

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    if (FAILED(hr)) goto done;

    if (camIndex >= count) {
        hr = E_INVALIDARG;
        goto done;
    }

    *ppActivate = ppDevices[camIndex];
    (*ppActivate)->AddRef();

done:
    for (UINT32 i = 0; i < count; ++i)
        if (ppDevices[i]) ppDevices[i]->Release();
    CoTaskMemFree(ppDevices);
    if (pAttributes) pAttributes->Release();
    return hr;
}

HRESULT GetAudioDeviceActivate(UINT32 micIndex, IMFActivate** ppActivate) {
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
    if (FAILED(hr)) goto done;

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    if (FAILED(hr)) goto done;

    if (micIndex >= count) {
        hr = E_INVALIDARG;
        goto done;
    }

    *ppActivate = ppDevices[micIndex];
    (*ppActivate)->AddRef();

done:
    for (UINT32 i = 0; i < count; ++i)
        if (ppDevices[i]) ppDevices[i]->Release();
    CoTaskMemFree(ppDevices);
    if (pAttributes) pAttributes->Release();
    return hr;
}

bool StartRecording(int camIndex, int micIndex, const wchar_t* outputPath) {
    StopRecorder();

    HRESULT hr = S_OK;

    UINT32 num = 0, den = 0;
    UINT32 width = 320, height = 180;
    IMFAttributes* pAttributes = nullptr;

    IMFMediaType* nativeVideoType = nullptr;
    IMFMediaType* outVideoType = nullptr;
    IMFActivate* pVideoActivate = nullptr;

    IMFMediaType* nativeAudioType = nullptr;
    IMFMediaType* outAudioType = nullptr;
    IMFActivate* pAudioActivate = nullptr;

    if (g_ctx.isRecording.load()) return false;

    hr = GetCameraDeviceActivate(camIndex, &pVideoActivate);
    if (FAILED(hr)) goto error;
    hr = pVideoActivate->ActivateObject(IID_PPV_ARGS(&g_ctx.videoSource));
    pVideoActivate->Release();
    if (FAILED(hr)) goto error;

    hr = GetAudioDeviceActivate(micIndex, &pAudioActivate);
    if (FAILED(hr)) goto error;
    hr = pAudioActivate->ActivateObject(IID_PPV_ARGS(&g_ctx.audioSource));
    pAudioActivate->Release();
    if (FAILED(hr)) goto error;

    hr = MFCreateSourceReaderFromMediaSource(g_ctx.videoSource, nullptr, &g_ctx.videoReader);
    if (FAILED(hr)) goto error;
    hr = MFCreateSourceReaderFromMediaSource(g_ctx.audioSource, nullptr, &g_ctx.audioReader);
    if (FAILED(hr)) goto error;

    // 3. Obtener tipo nativo video
    hr = g_ctx.videoReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &nativeVideoType);
    if (FAILED(hr)) goto error;
    hr = g_ctx.videoReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, nativeVideoType);
    if (FAILED(hr)) goto error;

    // 4. Obtener tipo nativo audio
    hr = g_ctx.audioReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &nativeAudioType);
    if (FAILED(hr)) goto error;
    hr = g_ctx.audioReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, nativeAudioType);
    if (FAILED(hr)) goto error;

    hr = MFCreateAttributes(&pAttributes, 1); if (FAILED(hr)) return hr;

    // Habilitar CBR explícitamente
    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE); // Opcional, para usar HW
    hr = pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE); // Opcional, si es necesario

    // 5. Crear SinkWriter
    hr = MFCreateSinkWriterFromURL(outputPath, nullptr, pAttributes, &g_ctx.sinkWriter); if (FAILED(hr)) goto error;

    // 6. Configurar tipo salida video (H264)
    hr = MFCreateMediaType(&outVideoType); if (FAILED(hr)) goto error;
    hr = outVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); if (FAILED(hr)) goto error;
    hr = outVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264); if (FAILED(hr)) goto error;
    hr = outVideoType->SetUINT32(MF_MT_AVG_BITRATE, 128000); if (FAILED(hr)) goto error;
    hr = outVideoType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive); if (FAILED(hr)) goto error;
    hr = MFSetAttributeRatio(outVideoType, MF_MT_FRAME_RATE, 24, 1); if (FAILED(hr)) goto error;
    hr = MFSetAttributeRatio(outVideoType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1); if (FAILED(hr)) goto error;
    hr = MFSetAttributeSize(outVideoType, MF_MT_FRAME_SIZE, 640, 360); if (FAILED(hr)) goto error;
    MFSetAttributeSize(outVideoType, MF_MT_FRAME_SIZE, width, height);
    MFGetAttributeRatio(nativeVideoType, MF_MT_FRAME_RATE, &num, &den);
    if (num == 0 || den == 0) { num = 30; den = 1; }
    MFSetAttributeRatio(outVideoType, MF_MT_FRAME_RATE, num, den);
    MFSetAttributeRatio(outVideoType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    hr = g_ctx.sinkWriter->AddStream(outVideoType, &g_ctx.videoStreamIndex); if (FAILED(hr)) goto error;
    hr = g_ctx.sinkWriter->SetInputMediaType(g_ctx.videoStreamIndex, nativeVideoType, nullptr); if (FAILED(hr)) goto error;

    // 7. Configurar salida audio (AAC)
    hr = MFCreateMediaType(&outAudioType); if (FAILED(hr)) goto error;
    hr = outAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio); if (FAILED(hr)) goto error;
    hr = outAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC); if (FAILED(hr)) goto error;
    hr = outAudioType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100); if (FAILED(hr)) goto error;
    hr = outAudioType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2); if (FAILED(hr)) goto error;
    hr = outAudioType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16); if (FAILED(hr)) goto error;
    hr = outAudioType->SetUINT32(MF_MT_AVG_BITRATE, 128000); if (FAILED(hr)) goto error;
    hr = outAudioType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29); if (FAILED(hr)) goto error;

    hr = g_ctx.sinkWriter->AddStream(outAudioType, &g_ctx.audioStreamIndex); if (FAILED(hr)) goto error;
    hr = g_ctx.sinkWriter->SetInputMediaType(g_ctx.audioStreamIndex, nativeAudioType, nullptr); if (FAILED(hr)) goto error;

    // 8. Iniciar escritura
    hr = g_ctx.sinkWriter->BeginWriting(); if (FAILED(hr)) goto error;
    g_ctx.isRecording = true; // se marca antes de lanzar los hilos

    // 9. Hilo de video
    g_ctx.recordingThread = std::thread([num, den]() {
        DWORD streamIndex, flags;
        LONGLONG sampleTime = 0;
        LONGLONG frameDuration = (10 * 1000 * 1000 * den) / num;

        while (g_ctx.isRecording) {
            if (g_ctx.isPaused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            IMFSample* pSample = nullptr;
            HRESULT hr = g_ctx.videoReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &sampleTime, &pSample);

            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;

            if (pSample) {
                if (g_ctx.baseTime == -1 && sampleTime > 0) g_ctx.baseTime = sampleTime;
                LONGLONG adjustedTime = sampleTime - g_ctx.baseTime;
                pSample->SetSampleTime(adjustedTime);
                pSample->SetSampleDuration(frameDuration);
                g_ctx.sinkWriter->WriteSample(g_ctx.videoStreamIndex, pSample);
                pSample->Release();
            }
        }
        });

    // 10. Hilo de audio
    g_ctx.audioThread = std::thread([]() {
        DWORD streamIndex, flags;
        LONGLONG sampleTime = 0;
        LONGLONG sampleDuration = 0;

        while (g_ctx.isRecording) {
            if (g_ctx.isPaused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            IMFSample* pSample = nullptr;
            HRESULT hr = g_ctx.audioReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &sampleTime, &pSample);

            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;

            if (pSample) {
                if (g_ctx.baseTime == -1 && sampleTime > 0) g_ctx.baseTime = sampleTime;

                LONGLONG adjustedTime = sampleTime - g_ctx.baseTime;
                hr = pSample->GetSampleDuration(&sampleDuration);
                if (FAILED(hr)) {
                    pSample->Release();
                    break;
                }

                pSample->SetSampleTime(adjustedTime);
                pSample->SetSampleDuration(sampleDuration);
                g_ctx.sinkWriter->WriteSample(g_ctx.audioStreamIndex, pSample);
                pSample->Release();
            }
        }
        });

    // 11. Liberar tipos ya no necesarios
    if (nativeVideoType) nativeVideoType->Release();
    if (nativeAudioType) nativeAudioType->Release();
    if (outVideoType) outVideoType->Release();
    if (outAudioType) outAudioType->Release();

    return true;

error:
    if (nativeVideoType) nativeVideoType->Release();
    if (nativeAudioType) nativeAudioType->Release();
    if (outVideoType) outVideoType->Release();
    if (outAudioType) outAudioType->Release();
    if (g_ctx.videoSource) { g_ctx.videoSource->Release(); g_ctx.videoSource = nullptr; }
    if (g_ctx.audioSource) { g_ctx.audioSource->Release(); g_ctx.audioSource = nullptr; }
    if (g_ctx.videoReader) { g_ctx.videoReader->Release(); g_ctx.videoReader = nullptr; }
    if (g_ctx.audioReader) { g_ctx.audioReader->Release(); g_ctx.audioReader = nullptr; }
    if (g_ctx.sinkWriter) { g_ctx.sinkWriter->Release(); g_ctx.sinkWriter = nullptr; }

    ResetContext();

    std::cout << "[ERROR] StartRecording falló con hr=0x" << std::hex << hr << "\n";
    return false;
}

bool PauseRecorder() {
    g_ctx.isPaused = true;
    return true;
}

bool ResumeRecorder() {
    g_ctx.isPaused = false;
    return true;
}

bool GetSupportedFormats(int deviceIndex, CameraFormat** formats, int* count) {
    *formats = nullptr;
    *count = 0;

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return false;

    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 numDevices = 0;
    std::vector<CameraFormat> result;

    hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr)) hr = MFEnumDeviceSources(pAttributes, &ppDevices, &numDevices);

    if (SUCCEEDED(hr) && deviceIndex < (int)numDevices) {
        IMFMediaSource* pSource = nullptr;
        hr = ppDevices[deviceIndex]->ActivateObject(IID_PPV_ARGS(&pSource));

        if (SUCCEEDED(hr)) {
            IMFSourceReader* pReader = nullptr;
            hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &pReader);
            if (SUCCEEDED(hr)) {
                DWORD i = 0;
                while (true) {
                    IMFMediaType* pType = nullptr;
                    hr = pReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType);
                    if (FAILED(hr)) break;

                    UINT32 width = 0, height = 0, num = 0, den = 0;
                    MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
                    MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den);

                    result.push_back({ width, height, num, den });
                    pType->Release();
                    ++i;
                }
                pReader->Release();
            }
            pSource->Release();
        }
    }

    for (UINT32 i = 0; i < numDevices; i++) ppDevices[i]->Release();
    CoTaskMemFree(ppDevices);
    if (pAttributes) pAttributes->Release();
    MFShutdown();

    if (!result.empty()) {
        *count = static_cast<int>(result.size());
        *formats = new CameraFormat[*count];
        memcpy(*formats, result.data(), sizeof(CameraFormat) * (*count));
        return true;
    }

    return false;
}

void FreeFormats(CameraFormat* formats)
{
    delete[] formats;
}