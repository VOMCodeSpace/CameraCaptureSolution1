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
#include <algorithm>

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
static ErrorCallbackFunc g_errorCallback = nullptr;
static LONGLONG MIN_TIME_BEFORE_PAUSE_ALLOWED = 30000000;

std::string GUIDToSubtypeString(const GUID& subtype) {
    if (subtype == MFVideoFormat_RGB24) return "RGB24";
    if (subtype == MFVideoFormat_RGB32) return "RGB32";
    if (subtype == MFVideoFormat_ARGB32) return "ARGB32";
    if (subtype == MFVideoFormat_YUY2) return "YUY2";
    if (subtype == MFVideoFormat_NV12) return "NV12";
    if (subtype == MFVideoFormat_MPEG2) return "MJPEG";
    if (subtype == MFVideoFormat_H264) return "H264";
    if (subtype == MFVideoFormat_UYVY) return "UYVY";
    if (subtype == MFVideoFormat_YV12) return "YV12";
    // Agrega más formatos si los necesitas
    return "UNKNOWN";
}

// --- Variables globales para grabación ---
struct MediaCaptureContext {
    IMFMediaSource* videoSource = nullptr;
    IMFMediaSource* videoSource2 = nullptr;
    IMFMediaSource* audioSource = nullptr;
    IMFSourceReader* videoReader = nullptr;
    IMFSourceReader* videoReader2 = nullptr;
    IMFSourceReader* audioReader = nullptr;
    IMFSinkWriter* sinkWriter = nullptr;
    DWORD videoStreamIndex = 0;
    DWORD audioStreamIndex = 0;
    std::thread audioThread;
    std::thread recordingThread;
    std::atomic<bool> isRecording = false;
    std::atomic<bool> isPaused = false;
    std::atomic<LONGLONG> baseTime{ -1 };
    LONGLONG pauseStartTime = 0;
    LONGLONG totalPausedDuration = 0;
    std::atomic<LONGLONG> recordingStartTime = 0;
};
static MediaCaptureContext g_ctx;

// --- Prototipos internos ---
bool InitializeRecorder(int videoIndex, int audioIndex, const wchar_t* outputPath);

void ResetContext() {
    g_ctx.videoStreamIndex = 0;
    g_ctx.audioStreamIndex = 0;
    g_ctx.baseTime = -1;
    g_ctx.videoSource = nullptr;
    g_ctx.audioSource = nullptr;
    g_ctx.videoSource2 = nullptr;
    g_ctx.videoReader = nullptr;
    g_ctx.audioReader = nullptr;
    g_ctx.videoReader2 = nullptr;
    g_ctx.sinkWriter = nullptr;
    g_ctx.isRecording = false;
    g_ctx.isPaused = false;
    g_ctx.baseTime = -1;
}

extern "C" __declspec(dllexport) void __stdcall SetErrorCallback(ErrorCallbackFunc callback) {
    g_errorCallback = callback;
}


void ReportError(const std::wstring& message) {
    if (g_errorCallback) {
        try {
            g_errorCallback(message.c_str());
        }
        catch (...) {
            // Silenciar excepciones del lado .NET si el callback falla
        }
    }
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

    if (g_ctx.videoReader2) {
        g_ctx.videoReader2->Release();
        g_ctx.videoReader2 = nullptr;
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

    if (g_ctx.videoSource2) {
        g_ctx.videoSource2->Shutdown();
        g_ctx.videoSource2->Release();
        g_ctx.videoSource2 = nullptr;
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

bool PauseRecorder() {
    if (g_ctx.isRecording) {
        if (!g_ctx.isPaused)
        {
            LONGLONG currentTime = MFGetSystemTime();
            if ((currentTime - g_ctx.recordingStartTime) < MIN_TIME_BEFORE_PAUSE_ALLOWED) {
                return false;
            }
            g_ctx.pauseStartTime = MFGetSystemTime();
            g_ctx.isPaused = true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool ContinueRecorder() {
    if (g_ctx.isRecording) {
        if (g_ctx.isPaused)
        {
            LONGLONG resumeTime = MFGetSystemTime();
            LONGLONG currentPauseDuration = resumeTime - g_ctx.pauseStartTime;
            LONGLONG oldTotalPausedDuration = g_ctx.totalPausedDuration;
            LONGLONG now = MFGetSystemTime();
            g_ctx.totalPausedDuration += (now - g_ctx.pauseStartTime);
            g_ctx.isPaused = false;
        }
        else
        {
            return false;
        }
    }
    else {
        return false;
    }
    return true;
}

bool PauseRecording() { return g_ctx.isRecording ? PauseRecorder() : false; }
bool ContinueRecording() { return g_ctx.isRecording ? ContinueRecorder() : false; }
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

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(punk) \
   if ((punk) != NULL)  \
      { (punk)->Release(); (punk) = NULL; }
#endif

#ifndef CHECK_HR
#define CHECK_HR(hr_val, msg) \
    if (FAILED(hr_val)) { \
        std::cerr << "Error in " << msg << ": 0x" << std::hex << hr_val << std::endl; \
        goto error; \
    }
#endif

HRESULT SetPreferredMediaType(
    IMFSourceReader* pReader,
    UINT32 targetWidth,
    UINT32 targetHeight,
    UINT32 targetFpsNum,
    UINT32 targetFpsDen
) {
    HRESULT hr = E_FAIL;
    IMFMediaType* pMediaType = nullptr;
    IMFMediaType* pFoundType = nullptr; // Para el tipo de medio que seleccionemos

    // Prioridad de subtipos de video que aceptamos (en orden descendente)
    // NV12 es preferido porque es el que usaremos para la composición.
    // MJPEG y YUY2 son comunes y el SourceReader a menudo puede transcodificarlos a NV12.
    const GUID preferredSubtypes[] = {
        MFVideoFormat_NV12,
        MFVideoFormat_MJPG,
        MFVideoFormat_YUY2,
        // Si encuentras otros GUIDs para formatos comunes como H.264 o otros YUV que la cámara
        // soporte y que creas que MF pueda transcodificar a NV12, puedes añadirlos aquí.
    };

    // Primero, intenta buscar el tipo de medio ideal (NV12 con resolución y FPS exactos)
    for (DWORD i = 0; ; ++i) {
        hr = pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pMediaType);
        if (FAILED(hr)) {
            if (hr == MF_E_NO_MORE_TYPES) {
                // std::cout << "No more native media types found.\n"; // Optional
                hr = S_OK;
            }
            break;
        }

        GUID subtypeGuid = GUID_NULL;
        UINT32 width_native = 0, height_native = 0, num_native = 0, den_native = 0;

        if (SUCCEEDED(pMediaType->GetGUID(MF_MT_SUBTYPE, &subtypeGuid)) &&
            SUCCEEDED(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width_native, &height_native)) &&
            SUCCEEDED(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &num_native, &den_native))) {

            // --- Add this line for debugging! ---
            std::cout << "DEBUG: Native format found: " << width_native << "x" << height_native << " @ "
                << num_native << "/" << den_native << " (" << GUIDToSubtypeString(subtypeGuid) << ")\n";
            // --- End debug line ---

            // ... rest of your SetPreferredMediaType logic for finding preferred types ...
            // If it's the ideal NV12
            if (subtypeGuid == MFVideoFormat_NV12 &&
                width_native == targetWidth && height_native == targetHeight &&
                num_native == targetFpsNum && den_native == targetFpsDen) {
                // ... (your existing logic for pFoundType and goto found_type)
            }
            // ... (your existing logic for other preferred subtypes)
        }
        SAFE_RELEASE(pMediaType); // Make sure to release in the loop
    }

    // Si no se encontró el formato ideal (NV12 exacto), intentar con otros subtipos preferidos
    // (MJPEG, YUY2) a la resolución y FPS exactos, esperando que MF los convierta.
    for (int s = 0; s < _countof(preferredSubtypes); ++s) {
        if (pFoundType) break; // Ya encontramos uno
        const GUID currentPreferredSubtype = preferredSubtypes[s];

        for (DWORD i = 0; ; ++i) {
            hr = pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pMediaType);
            if (FAILED(hr)) {
                if (hr == MF_E_NO_MORE_TYPES) hr = S_OK;
                break;
            }

            GUID subtypeGuid = GUID_NULL;
            UINT32 width = 0, height = 0, num = 0, den = 0;

            if (SUCCEEDED(pMediaType->GetGUID(MF_MT_SUBTYPE, &subtypeGuid)) &&
                SUCCEEDED(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height)) &&
                SUCCEEDED(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &num, &den))) {

                if (subtypeGuid == currentPreferredSubtype &&
                    width == targetWidth && height == targetHeight &&
                    num == targetFpsNum && den == targetFpsDen) {

                    pFoundType = pMediaType;
                    pFoundType->AddRef();
                    pMediaType->Release();
                    std::cout << "Found preferred format: " << GUIDToSubtypeString(currentPreferredSubtype) << " "
                        << targetWidth << "x" << targetHeight << " @ " << targetFpsNum << "/" << targetFpsDen << "\n";
                    hr = S_OK;
                    goto found_type;
                }
            }
            pMediaType->Release();
            pMediaType = nullptr;
        }
    }

    // Si todavía no se ha encontrado un formato, intentar configurar el tipo de salida del SourceReader
    // directamente al formato deseado (NV12) y dejar que Media Foundation intente la transcodificación.
    // Esto es lo que estabas haciendo antes, pero ahora es un último recurso.
    hr = MFCreateMediaType(&pFoundType);
    if (FAILED(hr)) {
        std::cerr << "Error al crear pFoundType para el último intento: 0x" << std::hex << hr << "\n";
        goto cleanup;
    }

    hr = pFoundType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) goto cleanup;
    hr = pFoundType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeSize(pFoundType, MF_MT_FRAME_SIZE, targetWidth, targetHeight);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(pFoundType, MF_MT_FRAME_RATE, targetFpsNum, targetFpsDen);
    if (FAILED(hr)) goto cleanup;

    std::cout << "Attempting to set requested NV12 format directly as a fallback.\n";

found_type:
    // Una vez que tengamos pFoundType (ya sea nativo o recién creado), intentamos aplicarlo
    if (pFoundType) {
        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pFoundType);
        if (FAILED(hr)) {
            std::cerr << "FAILED to set media type to " << targetWidth << "x" << targetHeight
                << " @ " << targetFpsNum << "/" << targetFpsDen << " (NV12 or preferred): 0x" << std::hex << hr << "\n";
        }
        else {
            std::cout << "Successfully set video media type for reader.\n";
        }
    }
    else {
        hr = E_UNEXPECTED; // No se encontró ningún tipo de medio para intentar
        std::cerr << "Error: No suitable media type found or created for reader.\n";
    }

cleanup:
    if (pFoundType) { pFoundType->Release(); }
    return hr;
}

bool StartRecording(int camIndex, int micIndex, const wchar_t* outputPath) {
    if (g_ctx.isRecording) {
        ReportError(L"Error: La grabación ya está en curso.");
        return false;
    }
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
                LONGLONG adjustedTime = (sampleTime - g_ctx.baseTime) - g_ctx.totalPausedDuration;
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

                LONGLONG adjustedTime = (sampleTime - g_ctx.baseTime) - g_ctx.totalPausedDuration;
                pSample->SetSampleTime(adjustedTime);
                pSample->SetSampleDuration(sampleDuration);
                g_ctx.sinkWriter->WriteSample(g_ctx.audioStreamIndex, pSample);
                pSample->Release();
            }
        }
    });
    
    g_ctx.recordingStartTime.store(MFGetSystemTime());

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
    ReportError(L"Error: La grabación no pudo ejecutarse.");
    return false;
}

bool StartRecordingTwoCameras(int camIndex, int cam2Index, int micIndex, const wchar_t* outputPath) {
    if (g_ctx.isRecording) {
        ReportError(L"Error: La grabación ya está en curso.");
        return false;
    }
    g_ctx.baseTime.store(-1);
    g_ctx.totalPausedDuration = 0;
    g_ctx.pauseStartTime = 0;
    g_ctx.isPaused = false;
    g_ctx.isRecording = true;
 
    // 0. Limpiar estado anterior y verificar si ya está grabando
    StopRecorder(); // Asegura que los hilos anteriores estén detenidos y los recursos liberados
    // Si StopRecorder() no resetea isRecording, hazlo aquí.
    HRESULT hr = S_OK;

    // --- Parámetros de Video de Salida Compuesto ---
    // Ajusta estos valores a la resolución y FPS que quieres para el archivo final.
    UINT32 finalOutputWidth = 1280; // Ancho total del video compuesto (ej: 2 cámaras una al lado de la otra)
    UINT32 finalOutputHeight = (cam2Index >= 0) ? (720 * 2) : 720; // Alto total del video compuesto
    UINT32 finalOutputFpsNum = 30; // Numerador de FPS del video compuesto
    UINT32 finalOutputFpsDen = 1;  // Denominador de FPS del video compuesto (ej: 30/1 = 30 FPS)


    // --- Parámetros de Video de Entrada de CADA Cámara ---
    // Esta es la resolución y FPS que pedirás a cada cámara individualmente.
    // Si cam2Index >= 0, las cámaras se apilan verticalmente, por lo que cada una aporta la mitad de la altura.
    UINT32 singleCamInputWidth = 1280;
    UINT32 singleCamInputHeight = finalOutputHeight / 2;

    // --- Punteros COM (Inicialización a nullptr para limpieza segura) ---
    // Usaremos SAFE_RELEASE en el 'error' block.
    IMFAttributes* pAttributes = nullptr;
    IMFMediaType* outVideoType = nullptr;
    IMFMediaType* outAudioType = nullptr;
    IMFActivate* pAudioActivate = nullptr;
    IMFActivate* pVideoActivate1 = nullptr;
    IMFActivate* pVideoActivate2 = nullptr;
    IMFMediaType* sinkInputVideoType = nullptr;

    // 1. Obtener los activadores de los dispositivos
    if (camIndex >= 0)
    {
        hr = GetCameraDeviceActivate(camIndex, &pVideoActivate1); CHECK_HR(hr, "GetCameraDeviceActivate (Cam1)");
        hr = pVideoActivate1->ActivateObject(IID_PPV_ARGS(&g_ctx.videoSource)); CHECK_HR(hr, "ActivateObject (Cam1)");
    }
    else 
    {
        g_ctx.videoSource = nullptr; // Asegurarse de que sea nulo si no se usa
        g_ctx.videoReader = nullptr; // Asegurarse de que sea nulo si no se usa
    }

    if (cam2Index >= 0 and cam2Index != camIndex) {
        hr = GetCameraDeviceActivate(cam2Index, &pVideoActivate2); CHECK_HR(hr, "GetCameraDeviceActivate (Cam2)");
        hr = pVideoActivate2->ActivateObject(IID_PPV_ARGS(&g_ctx.videoSource2)); CHECK_HR(hr, "ActivateObject (Cam2)");
    }
    else
    {
        g_ctx.videoSource2 = nullptr; // Asegurarse de que sea nulo si no se usa
        g_ctx.videoReader2 = nullptr; // Asegurarse de que sea nulo si no se usa
    }

    // Obtener activador de audio (si micIndex es válido)
    if (micIndex >= 0) { // Asumo que micIndex < 0 significa no grabar audio
        hr = GetAudioDeviceActivate(micIndex, &pAudioActivate); CHECK_HR(hr, "GetAudioDeviceActivate");
        hr = pAudioActivate->ActivateObject(IID_PPV_ARGS(&g_ctx.audioSource)); CHECK_HR(hr, "ActivateObject (Audio)");
    }
    else {
        g_ctx.audioSource = nullptr; // Asegurarse de que sea nulo si no se usa
        g_ctx.audioReader = nullptr; // Asegurarse de que sea nulo si no se usa
    }


    // 2. Crear SourceReaders desde los MediaSources
    if (camIndex and g_ctx.videoSource)
    {
        hr = MFCreateSourceReaderFromMediaSource(g_ctx.videoSource, nullptr, &g_ctx.videoReader); CHECK_HR(hr, "MFCreateSourceReaderFromMediaSource (Cam1)");
    }
    if (cam2Index >= 0 and g_ctx.videoSource2) {
        hr = MFCreateSourceReaderFromMediaSource(g_ctx.videoSource2, nullptr, &g_ctx.videoReader2); CHECK_HR(hr, "MFCreateSourceReaderFromMediaSource (Cam2)");
    }
    if (g_ctx.audioSource) { // Solo si hay fuente de audio
        hr = MFCreateSourceReaderFromMediaSource(g_ctx.audioSource, nullptr, &g_ctx.audioReader); CHECK_HR(hr, "MFCreateSourceReaderFromMediaSource (Audio)");
    }

    // 3. Configurar tipos de entrada de video para ambas cámaras (usando SetPreferredMediaType)
    if (camIndex >= 0)
    {
        hr = SetPreferredMediaType(g_ctx.videoReader, singleCamInputWidth, singleCamInputHeight, finalOutputFpsNum, finalOutputFpsDen);
        CHECK_HR(hr, "SetPreferredMediaType for primary camera");
    }

    if (cam2Index >= 0) {
        hr = SetPreferredMediaType(g_ctx.videoReader2, singleCamInputWidth, singleCamInputHeight, finalOutputFpsNum, finalOutputFpsDen);
        CHECK_HR(hr, "SetPreferredMediaType for secondary camera");
    }

    // 4. Configurar el SourceReader de audio
    if (g_ctx.audioReader) {
        // En audio, a menudo es mejor usar el tipo nativo directo si es compatible con el SinkWriter.
        // O puedes pedir un formato específico (ej: 44100 Hz, 2 canales, 16 bits PCM).
        // Por ahora, obtenemos el primer tipo nativo y lo establecemos.
        IMFMediaType* nativeAudioType = nullptr;
        hr = g_ctx.audioReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &nativeAudioType);
        CHECK_HR(hr, "GetNativeMediaType for audio");

        hr = g_ctx.audioReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, nativeAudioType);
        CHECK_HR(hr, "SetCurrentMediaType for audio");
        SAFE_RELEASE(nativeAudioType); // Liberar el tipo nativo
    }


    // 5. Crear SinkWriter para el archivo de salida
    hr = MFCreateAttributes(&pAttributes, 1); 
    CHECK_HR(hr, "MFCreateAttributes for SinkWriter");
    
    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE); // Habilitar transformaciones por hardware
    hr = pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);

    hr = MFCreateSinkWriterFromURL(outputPath, nullptr, pAttributes, &g_ctx.sinkWriter); 
    CHECK_HR(hr, "MFCreateSinkWriterFromURL");
    SAFE_RELEASE(pAttributes);


    // 6. Configurar tipo de salida de video (H.264)
    hr = MFCreateMediaType(&outVideoType); CHECK_HR(hr, "MFCreateMediaType for outVideoType");
    hr = outVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); CHECK_HR(hr, "SetGUID MF_MT_MAJOR_TYPE (Video)");
    hr = outVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264); CHECK_HR(hr, "SetGUID MF_MT_SUBTYPE (H264)");
    hr = outVideoType->SetUINT32(MF_MT_AVG_BITRATE, 128000);
    hr = outVideoType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive); CHECK_HR(hr, "SetUINT32 MF_MT_INTERLACE_MODE");
    hr = MFSetAttributeRatio(outVideoType, MF_MT_FRAME_RATE, finalOutputFpsNum, finalOutputFpsDen); CHECK_HR(hr, "MFSetAttributeRatio (FPS)");
    hr = MFSetAttributeRatio(outVideoType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1); CHECK_HR(hr, "MFSetAttributeRatio (PAR)");
    hr = MFSetAttributeSize(outVideoType, MF_MT_FRAME_SIZE, finalOutputWidth, finalOutputHeight); CHECK_HR(hr, "MFSetAttributeSize (Resolution)");

    hr = g_ctx.sinkWriter->AddStream(outVideoType, &g_ctx.videoStreamIndex); CHECK_HR(hr, "AddStream (Video)");
    SAFE_RELEASE(outVideoType); // Liberar outVideoType


    // 7. Configurar tipo de entrada del SinkWriter para el stream de video (NV12)
    hr = MFCreateMediaType(&sinkInputVideoType); CHECK_HR(hr, "MFCreateMediaType for sinkInputVideoType");
    hr = sinkInputVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); CHECK_HR(hr, "SetGUID MF_MT_MAJOR_TYPE (SinkInput)");
    hr = sinkInputVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12); CHECK_HR(hr, "SetGUID MF_MT_SUBTYPE (SinkInput NV12)");
    hr = MFSetAttributeSize(sinkInputVideoType, MF_MT_FRAME_SIZE, finalOutputWidth, finalOutputHeight); CHECK_HR(hr, "MFSetAttributeSize (SinkInput Resolution)");
    hr = MFSetAttributeRatio(sinkInputVideoType, MF_MT_FRAME_RATE, finalOutputFpsNum, finalOutputFpsDen); CHECK_HR(hr, "MFSetAttributeRatio (SinkInput FPS)");
    hr = MFSetAttributeRatio(sinkInputVideoType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1); CHECK_HR(hr, "MFSetAttributeRatio (SinkInput PAR)");
    hr = sinkInputVideoType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive); CHECK_HR(hr, "SetUINT32 MF_MT_INTERLACE_MODE (SinkInput)");

    hr = g_ctx.sinkWriter->SetInputMediaType(g_ctx.videoStreamIndex, sinkInputVideoType, nullptr); CHECK_HR(hr, "SetInputMediaType (Video)");
    SAFE_RELEASE(sinkInputVideoType); // Liberar sinkInputVideoType


    // 8. Configurar salida de audio (AAC)
    if (g_ctx.audioReader) { // Solo si hay stream de audio
        hr = MFCreateMediaType(&outAudioType); CHECK_HR(hr, "MFCreateMediaType for outAudioType");
        hr = outAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio); CHECK_HR(hr, "SetGUID MF_MT_MAJOR_TYPE (Audio)");
        hr = outAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC); CHECK_HR(hr, "SetGUID MF_MT_SUBTYPE (AAC)");
        hr = outAudioType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100); CHECK_HR(hr, "SetUINT32 MF_MT_AUDIO_SAMPLES_PER_SECOND");
        hr = outAudioType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2); CHECK_HR(hr, "SetUINT32 MF_MT_AUDIO_NUM_CHANNELS");
        hr = outAudioType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16); CHECK_HR(hr, "SetUINT32 MF_MT_AUDIO_BITS_PER_SAMPLE");
        hr = outAudioType->SetUINT32(MF_MT_AVG_BITRATE, 128000); // 128 kbps CHECK_HR(hr, "SetUINT32 MF_MT_AVG_BITRATE (Audio)");
        hr = outAudioType->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29); CHECK_HR(hr, "SetUINT32 MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION");

        hr = g_ctx.sinkWriter->AddStream(outAudioType, &g_ctx.audioStreamIndex);
        CHECK_HR(hr, "AddStream (Audio)");
        SAFE_RELEASE(outAudioType);

        // Para el audio, a menudo se usa el tipo nativo del SourceReader directamente como entrada del SinkWriter,
        // o se convierte a un formato compatible (ej. PCM). Aquí asumo que el nativeAudioType del SourceReader
        // es directamente compatible o MF lo convierte.
        // NOTA: Si nativeAudioType no es compatible con AAC, necesitarás un transcodificador aquí.
        // Una forma más robusta sería crear un pcmAudioType y luego establecerlo.
        // Pero para simplificar, usaremos el mismo tipo nativo que el source reader está entregando.
        // Si el GetNativeMediaType de audio ya tiene el formato esperado por el SinkWriter, se usa.
        // Si no, deberías crear un nuevo IMFMediaType con el formato deseado (ej. AudioFormat_PCM)
        // y establecerlo como inputMediaType para el SinkWriter de audio.

        // Obtenemos el tipo actual del audio reader (ya establecido previamente)
        IMFMediaType* audioReaderCurrentType = nullptr;
        hr = g_ctx.audioReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &audioReaderCurrentType);
        CHECK_HR(hr, "GetCurrentMediaType for audio reader");
        hr = g_ctx.sinkWriter->SetInputMediaType(g_ctx.audioStreamIndex, audioReaderCurrentType, nullptr);
        CHECK_HR(hr, "SetInputMediaType (Audio)");
        SAFE_RELEASE(audioReaderCurrentType);
    }

    // 9. Iniciar escritura del archivo .mp4
    hr = g_ctx.sinkWriter->BeginWriting(); CHECK_HR(hr, "SinkWriter->BeginWriting");

    // Inicializar el tiempo base ANTES de iniciar los hilos
    g_ctx.baseTime = -1; // Se inicializa a 0, y se ajustará con el primer SampleTime si es > 0

    g_ctx.isRecording.store(true); // Se marca como grabando antes de lanzar los hilos

    // 10. Hilo de video
    if (g_ctx.videoReader && g_ctx.videoReader2)
    {
        g_ctx.recordingThread = std::thread([finalOutputFpsNum, finalOutputFpsDen,finalOutputWidth, finalOutputHeight, cam2Index,singleCamInputWidth, singleCamInputHeight]() 
        {
            HRESULT hr_thread = S_OK; // Usar una HR local para el hilo
            DWORD streamIndex, flags;
            LONGLONG sampleTime = 0;
            LONGLONG sampleTime2 = 0;

            LONGLONG frameDuration = (10 * 1000 * 1000 * finalOutputFpsDen) / finalOutputFpsNum; // Duración del frame en 100ns

            // Calcular tamaños para la composición de frames (NV12)
            UINT32 singleCamYSize = singleCamInputWidth * singleCamInputHeight;
            UINT32 singleCamUVSize = singleCamYSize / 2; // UV plane is half the size of Y for NV12
            UINT32 totalCombinedYSize = finalOutputWidth * finalOutputHeight;
            UINT32 totalCombinedUVSize = totalCombinedYSize / 2;
            UINT32 totalCombinedFrameSize = totalCombinedYSize + totalCombinedUVSize; // Total buffer size for combined NV12 frame


            while (g_ctx.isRecording.load()) {
                if (g_ctx.isPaused.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                IMFSample* pSample1 = nullptr;
                IMFSample* pSample2 = nullptr;
                // --- Leer frame de la PRIMERA cámara ---
                // Solo intentamos leer si pSample1 es nullptr (ya se procesó el anterior)
                hr_thread = g_ctx.videoReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &sampleTime, &pSample1);
                if (FAILED(hr_thread) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;

                hr_thread = g_ctx.videoReader2->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &sampleTime2, &pSample2);
                if (FAILED(hr_thread) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;

                // Si ambas cámaras están activas, esperar a tener ambos frames y luego combinarlos
                if (pSample1 && pSample2) {
                    if (g_ctx.baseTime == -1 && sampleTime > 0) g_ctx.baseTime = sampleTime; // Inicializar baseTime una vez

                    LONGLONG currentBaseTime = g_ctx.baseTime;

                    LONGLONG adjustedTime1 = (sampleTime - currentBaseTime) - g_ctx.totalPausedDuration;
                    LONGLONG adjustedTime2 = (sampleTime2 - currentBaseTime) - g_ctx.totalPausedDuration;

                    LONGLONG time1;
                    hr_thread = pSample1->GetSampleTime(&time1);
                    if (FAILED(hr_thread)) {
                        SAFE_RELEASE(pSample1);
                        pSample1 = nullptr;
                        g_ctx.isRecording.store(false); break;
                    }
                    pSample1->SetSampleTime(adjustedTime1);
                    pSample1->SetSampleDuration(frameDuration);
                    pSample2->SetSampleTime(adjustedTime2);
                    pSample2->SetSampleDuration(frameDuration);


                    LONGLONG combinedSampleTime = adjustedTime1; // Usar el tiempo de la primera cámara como referencia

                    IMFSample* pCombinedSample = nullptr;
                    IMFMediaBuffer* pCombinedBuffer = nullptr;

                    hr_thread = MFCreateSample(&pCombinedSample);
                    if (FAILED(hr_thread)) { break; }
                    hr_thread = MFCreateMemoryBuffer(totalCombinedFrameSize, &pCombinedBuffer);
                    if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedSample); break; }
                    hr_thread = pCombinedSample->AddBuffer(pCombinedBuffer);
                    if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pCombinedBuffer); break; }

                    BYTE* pCombinedData = nullptr;
                    DWORD cbCombinedMaxLength = 0, cbCombinedCurrentLength = 0;
                    hr_thread = pCombinedBuffer->Lock(&pCombinedData, &cbCombinedMaxLength, &cbCombinedCurrentLength);
                    if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pCombinedBuffer); break; }

                    // --- Copiar datos de la PRIMERA CÁMARA (arriba) ---
                    IMFMediaBuffer* pBuffer1 = nullptr;
                    hr_thread = pSample1->ConvertToContiguousBuffer(&pBuffer1);
                    if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample1); SAFE_RELEASE(pSample2); continue; }
                    BYTE* pData1 = nullptr; DWORD cbCurrentLength1 = 0;
                    hr_thread = pBuffer1->Lock(&pData1, nullptr, &cbCurrentLength1);
                    if (FAILED(hr_thread)) { SAFE_RELEASE(pBuffer1); SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample1); SAFE_RELEASE(pSample2); continue; }

                    // Copiar el plano Y (luminancia) de la primera cámara a la parte superior del buffer combinado
                    memcpy(pCombinedData, pData1, singleCamYSize);
                    // Copiar el plano UV (crominancia) de la primera cámara después de TODA la luminancia Y combinada
                    // Esto es: pCombinedData + (ancho_total * alto_total) + UV_offset_para_primera_cam
                    memcpy(pCombinedData + totalCombinedYSize, pData1 + singleCamYSize, singleCamUVSize);

                    pBuffer1->Unlock();
                    SAFE_RELEASE(pBuffer1);
                    SAFE_RELEASE(pSample1); // Liberar el sample de la primera cámara una vez copiado
                    pSample1 = nullptr; // Reiniciar para el siguiente frame

                    // --- Copiar datos de la SEGUNDA CÁMARA (abajo) ---
                    IMFMediaBuffer* pBuffer2 = nullptr;
                    hr_thread = pSample2->ConvertToContiguousBuffer(&pBuffer2);
                    if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample2); continue; }
                    BYTE* pData2 = nullptr; DWORD cbCurrentLength2 = 0;
                    hr_thread = pBuffer2->Lock(&pData2, nullptr, &cbCurrentLength2);
                    if (FAILED(hr_thread)) { SAFE_RELEASE(pBuffer2); SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample2); continue; }

                    // Copiar el plano Y (luminancia) de la segunda cámara DESPUÉS del plano Y de la primera
                    memcpy(pCombinedData + singleCamYSize, pData2, singleCamYSize);
                    // Copiar el plano UV (crominancia) de la segunda cámara DESPUÉS de las UVs de la primera
                    memcpy(pCombinedData + totalCombinedYSize + singleCamUVSize, pData2 + singleCamYSize, singleCamUVSize);

                    pBuffer2->Unlock();
                    SAFE_RELEASE(pBuffer2);
                    SAFE_RELEASE(pSample2); // Liberar el sample de la segunda cámara una vez copiado
                    pSample2 = nullptr; // Reiniciar para el siguiente frame

                    pCombinedBuffer->SetCurrentLength(totalCombinedFrameSize); // Establecer la longitud final del buffer
                    pCombinedBuffer->Unlock();
                    SAFE_RELEASE(pCombinedBuffer); // Liberar el buffer combinado, ya está añadido al sample

                    hr_thread = pCombinedSample->SetSampleTime(combinedSampleTime);
                    hr_thread = pCombinedSample->SetSampleDuration(frameDuration);

                    hr_thread = g_ctx.sinkWriter->WriteSample(g_ctx.videoStreamIndex, pCombinedSample);
                    SAFE_RELEASE(pCombinedSample); // Liberar el sample combinado
                    if (FAILED(hr_thread)) {
                        g_ctx.isRecording.store(false); // Si la escritura falla, detener la grabación
                        break;
                    }
                    SAFE_RELEASE(pSample1);
                    SAFE_RELEASE(pSample2);

                    // Pequeña espera para no consumir CPU en exceso
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else { // Si aún no tenemos ambos frames (y cam2Index >= 0), esperar un poco
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        });
    }
    // 11. Hilo de audio
    if (g_ctx.audioReader) { // Solo si hay un lector de audio
        g_ctx.audioThread = std::thread([]() {
            HRESULT hr_thread;
            DWORD streamIndex, flags;
            LONGLONG sampleTime = 0;
            LONGLONG sampleDuration = 0;

            while (g_ctx.isRecording.load()) {
                if (g_ctx.isPaused.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                IMFSample* pSample = nullptr;
                hr_thread = g_ctx.audioReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &sampleTime, &pSample);
                if (FAILED(hr_thread) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
          
                if (pSample) {
                    // ¡NUEVO! Obtener la duración del sample de audio
                    hr_thread = pSample->GetSampleDuration(&sampleDuration);
                    if (FAILED(hr_thread)) {
                        // Manejar error, quizás registrarlo y liberar pSample
                        SAFE_RELEASE(pSample);
                        continue; // Saltar este sample e intentar el siguiente
                    }

                    // Solo inicializar baseTime una vez, idealmente por el primer stream que obtiene un tiempo
                    if (g_ctx.baseTime == -1 && sampleTime > 0) {
                        g_ctx.baseTime.store(sampleTime);
                    }

                    // Usar la sampleDuration correctamente obtenida
                    LONGLONG adjustedTime = (sampleTime - g_ctx.baseTime) - g_ctx.totalPausedDuration;

                    pSample->SetSampleTime(adjustedTime);
                    pSample->SetSampleDuration(sampleDuration); // ¡Usar la duración real!
                    hr_thread = g_ctx.sinkWriter->WriteSample(g_ctx.audioStreamIndex, pSample);
                    SAFE_RELEASE(pSample);
                    if (FAILED(hr_thread)) {
                        g_ctx.isRecording.store(false);
                        break;
                    }
                }
            }
        });
    }
    g_ctx.recordingStartTime.store(MFGetSystemTime());
    return true; // Si todo el setup fue exitoso

error:
    // La macro CHECK_HR ya imprime el mensaje de error.
    // Liberar todos los punteros COM en caso de error, usando SAFE_RELEASE
    SAFE_RELEASE(pVideoActivate1);
    SAFE_RELEASE(pVideoActivate2);
    SAFE_RELEASE(pAudioActivate);
    SAFE_RELEASE(pAttributes); // Asegúrate de liberar si se creó antes del error
    SAFE_RELEASE(outVideoType);
    SAFE_RELEASE(sinkInputVideoType);
    SAFE_RELEASE(outAudioType);

    // Liberar los miembros de g_ctx si se asignaron
    SAFE_RELEASE(g_ctx.videoSource);
    SAFE_RELEASE(g_ctx.videoSource2);
    SAFE_RELEASE(g_ctx.audioSource);
    SAFE_RELEASE(g_ctx.videoReader);
    SAFE_RELEASE(g_ctx.videoReader2);
    SAFE_RELEASE(g_ctx.audioReader);
    SAFE_RELEASE(g_ctx.sinkWriter);

    // Asegurarse de que el contexto se resetee y la bandera de grabación se ponga a false
    g_ctx.isRecording.store(false);
    ResetContext(); // Si ResetContext libera los anteriores, algunos SAFE_RELEASE serían redundantes

    // NOTA: Los hilos `recordingThread` y `audioThread` NO se unen/esperan aquí en caso de error
    // porque es probable que el error ocurra ANTES de que se lancen, o el error está
    // en una inicialización que los hilos usarían y fallarían de inmediato.
    // StopRecorder() se encargará de unirlos en una llamada posterior si están running.

    return false; // Indicar que StartRecording falló
}

bool GetSupportedFormats(int deviceIndex, CameraFormat** formats, int* count) {
    *formats = nullptr;
    *count = 0;

    HRESULT hr = S_OK; // Inicializa hr

    // MFStartup y MFShutdown deben ser gestionados a nivel global de la aplicación
    // Si no tienes un MFStartup y MFShutdown en tu main o en la inicialización/desinicialización global,
    // puedes descomentar estas líneas, pero asegúrate de que no se llamen múltiples veces.
    // hr = MFStartup(MF_VERSION);
    // if (FAILED(hr)) {
    //     std::cerr << "Error en MFStartup: 0x" << std::hex << hr << std::endl;
    //     return false;
    // }

    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 numDevices = 0;
    std::vector<CameraFormat> result;
    IMFMediaSource* pSource = nullptr;
    IMFSourceReader* pReader = nullptr;
    DWORD i = 0;

    hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) {
        hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    }
    if (FAILED(hr)) {
        std::cerr << "Error al crear atributos para enumerar dispositivos: 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &numDevices);
    if (FAILED(hr)) {
        std::cerr << "Error al enumerar dispositivos de video: 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    if (deviceIndex < 0 || deviceIndex >= (int)numDevices) {
        std::cerr << "Error: Índice de dispositivo fuera de rango. deviceIndex = " << deviceIndex << ", numDevices = " << numDevices << std::endl;
        hr = E_INVALIDARG; // Establece un HRESULT apropiado
        goto cleanup;
    }

    hr = ppDevices[deviceIndex]->ActivateObject(IID_PPV_ARGS(&pSource));
    if (FAILED(hr)) {
        std::cerr << "Error al activar el dispositivo de video " << deviceIndex << ": 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    // El segundo parámetro (pAttributes) puede ser nullptr para el SourceReader si no se requieren atributos especiales.
    hr = MFCreateSourceReaderFromMediaSource(pSource, nullptr, &pReader);
    if (FAILED(hr)) {
        std::cerr << "Error al crear SourceReader para el dispositivo " << deviceIndex << ": 0x" << std::hex << hr << std::endl;
        goto cleanup;
    }

    while (true) {
        IMFMediaType* pType = nullptr;
        GUID subtypeGuid = GUID_NULL;
        UINT32 width = 0, height = 0, num = 0, den = 0;

        // Intentar obtener el tipo de medio nativo
        hr = pReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pType);
        if (FAILED(hr)) {
            if (hr == MF_E_NO_MORE_TYPES) {
                hr = S_OK; // No hay más tipos, salir con éxito del bucle
            }
            else {
                std::cerr << "Error al obtener NativeMediaType " << i << " para el dispositivo " << deviceIndex << ": 0x" << std::hex << hr << std::endl;
            }
            break; // Salir del bucle
        }

        // Crear una instancia de CameraFormat para almacenar los datos
        CameraFormat currentFormat = {}; // Inicializa a cero para evitar basura

        // Obtener el subtipo de video
        if (SUCCEEDED(pType->GetGUID(MF_MT_SUBTYPE, &subtypeGuid))) {
            std::string s_subtype = GUIDToSubtypeString(subtypeGuid);
            // Copia segura de la cadena al array de char.
            // snprintf es más seguro que strcpy. Asegura terminación nula.
            snprintf(currentFormat.subtype, MAX_SUBTYPE_NAME_LEN, "%s", s_subtype.c_str());
        }
        else {
            // Si no se puede obtener el GUID del subtipo, usa un valor por defecto
            snprintf(currentFormat.subtype, MAX_SUBTYPE_NAME_LEN, "UNKNOWN_GUID");
        }

        // Obtener tamaño
        if (SUCCEEDED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height))) {
            currentFormat.width = width;
            currentFormat.height = height;
        }
        else {
            // Si falla la obtención de tamaño, usa 0 o un valor indicativo
            currentFormat.width = 0;
            currentFormat.height = 0;
            std::cerr << "Advertencia: No se pudo obtener el tamaño del frame para el tipo de medio " << i << ". HRESULT: 0x" << std::hex << pType->GetGUID(MF_MT_MAJOR_TYPE, &subtypeGuid) << std::endl;
        }

        // Obtener tasa de cuadros
        if (SUCCEEDED(MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den))) {
            currentFormat.fps_num = num;
            currentFormat.fps_den = den;
        }
        else {
            // Si falla la obtención de FPS, usa 0 o un valor indicativo
            currentFormat.fps_num = 0;
            currentFormat.fps_den = 1; // Evitar división por cero
            std::cerr << "Advertencia: No se pudo obtener el FPS para el tipo de medio " << i << ". HRESULT: 0x" << std::hex << pType->GetGUID(MF_MT_MAJOR_TYPE, &subtypeGuid) << std::endl;
        }

        result.push_back(currentFormat); // Añadir el formato a nuestro vector
        pType->Release(); // Liberar el tipo de medio
        pType = nullptr;
        ++i;
    }

cleanup: // Etiqueta para la limpieza de recursos
    if (pReader) { pReader->Release(); }
    if (pSource) { pSource->Release(); }
    for (UINT32 j = 0; j < numDevices; j++) {
        if (ppDevices[j]) ppDevices[j]->Release();
    }
    CoTaskMemFree(ppDevices); // Libera el array de punteros
    if (pAttributes) { pAttributes->Release(); }

    // MFShutdown() solo si MFStartup() se llamó aquí y no se gestiona globalmente.
    // if (SUCCEEDED(hr)) { // Si MFStartup fue exitoso y se gestiona aquí
    //     MFShutdown();
    // }


    if (!result.empty()) {
        *count = static_cast<int>(result.size());
        // Asigna memoria en el heap y copia los resultados
        *formats = new CameraFormat[*count];
        memcpy(*formats, result.data(), sizeof(CameraFormat) * (*count));
        return true; // Éxito
    }

    return FAILED(hr) ? false : (result.empty() ? false : true); // Retorna false si hubo un error o si no se encontraron formatos
}

void FreeFormats(CameraFormat* formats)
{
    delete[] formats;
}