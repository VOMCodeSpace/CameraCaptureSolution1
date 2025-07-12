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
#include <gdiplus.h>
#include <wincodec.h>
#include <atlbase.h>
#include <wrl/client.h>
#include <fstream>
#include <map>

using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

DEFINE_GUID(MF_READWRITE_ENABLE_VIDEO_PROCESSING,
    0xA9D2A93C, 0xF2B7, 0x4D4C, 0x9C, 0x4F, 0xBC, 0xD3, 0x3C, 0x4B, 0x07, 0x6A);


using namespace Gdiplus;
using Microsoft::WRL::ComPtr;


std::thread videoThread;
std::thread audioThread;

// --- Variables globales ---
static IMFActivate** g_ppDevices = nullptr;
static UINT32 g_deviceCount = 0;
static std::vector<IMFActivate*> g_audioDevices;
static ErrorCallbackFunc g_errorCallback = nullptr;
static LONGLONG MIN_TIME_BEFORE_PAUSE_ALLOWED = 30000000;

std::string GUIDToSubtypeString(const GUID& subtype) {
    if (subtype == MFVideoFormat_RGB24)        return "RGB24";
    if (subtype == MFVideoFormat_RGB32)        return "RGB32";
    if (subtype == MFVideoFormat_ARGB32)       return "ARGB32";
    if (subtype == MFVideoFormat_YUY2)         return "YUY2";
    if (subtype == MFVideoFormat_UYVY)         return "UYVY";
    if (subtype == MFVideoFormat_NV12)         return "NV12";
    if (subtype == MFVideoFormat_YV12)         return "YV12";
    if (subtype == MFVideoFormat_MJPG)         return "MJPEG";
    if (subtype == MFVideoFormat_H264)         return "H264";
    if (subtype == MFVideoFormat_HEVC)         return "HEVC";
    if (subtype == MFVideoFormat_H265)         return "H265";
    if (subtype == MFVideoFormat_H264_ES)      return "H264-ES";
    if (subtype == MFVideoFormat_HEVC_ES)      return "HEVC-ES";
    if (subtype == MFVideoFormat_MP4S)         return "MP4S";
    if (subtype == MFVideoFormat_MP43)         return "MP43";
    if (subtype == MFVideoFormat_MP4S)         return "MP4S";
    if (subtype == MFVideoFormat_MPG1)         return "MPEG1";
    if (subtype == MFVideoFormat_MPEG2)        return "MPEG2";
    if (subtype == MFVideoFormat_VP80)         return "VP8";
    if (subtype == MFVideoFormat_VP90)         return "VP9";
    if (subtype == MFVideoFormat_WMV1)         return "WMV1";
    if (subtype == MFVideoFormat_WMV2)         return "WMV2";
    if (subtype == MFVideoFormat_WMV3)         return "WMV3";
    if (subtype == MFVideoFormat_WVC1)         return "WVC1";
    if (subtype == MFVideoFormat_DV25)         return "DV25";
    if (subtype == MFVideoFormat_DV50)         return "DV50";
    if (subtype == MFVideoFormat_DVC)          return "DVC";
    if (subtype == MFVideoFormat_DVH1)         return "DVH1";
    if (subtype == MFVideoFormat_DVHD)         return "DVHD";
    if (subtype == MFVideoFormat_DVSD)         return "DVSD";
    if (subtype == MFVideoFormat_DVSL)         return "DVSL";
    if (subtype == MFVideoFormat_H263)         return "H263";
    if (subtype == MFVideoFormat_L8)           return "L8";
    if (subtype == MFVideoFormat_L16)          return "L16";
    if (subtype == MFVideoFormat_D16)          return "D16";
    if (subtype == MFVideoFormat_420O)         return "420O";
    if (subtype == MFVideoFormat_AI44)         return "AI44";
    if (subtype == MFVideoFormat_MSS1)         return "MSS1";
    if (subtype == MFVideoFormat_MSS2)         return "MSS2";
    if (subtype == MFVideoFormat_MPG1)         return "MPEG1";

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


struct CameraInstance {
    IMFMediaSource* mediaSource = nullptr;
    IMFSourceReader* sourceReader = nullptr;
    IMFMediaType* nativeVideoType = nullptr;
    GUID videoSubtype = GUID_NULL;
    std::thread previewThread;
    HWND previewHwnd = nullptr;
    bool stopPreview = false;
};

std::mutex g_instancesMutex;
std::map<std::wstring, CameraInstance*> g_instances;

extern "C" __declspec(dllexport) void __stdcall SetErrorCallback(ErrorCallbackFunc callback) {
    g_errorCallback = callback;
}


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

void ReportError(const std::wstring& message) {
    if (g_errorCallback) {
        try { g_errorCallback(message.c_str()); }
        catch (...) {
            // Silenciar excepciones del lado .NET si el callback falla
        }
    }
}


HRESULT GetCameraDeviceActivate(const std::wstring& targetName, IMFActivate** ppActivateOut) {
    if (!ppActivateOut) return E_POINTER;
    *ppActivateOut = nullptr;

    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) goto cleanup;

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    if (FAILED(hr)) goto cleanup;

    for (UINT32 i = 0; i < count; ++i) {
        WCHAR* name = nullptr;
        UINT32 cchName = 0;
        hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &cchName);
        if (SUCCEEDED(hr)) {
            if (targetName == name) {
                *ppActivateOut = ppDevices[i];
                (*ppActivateOut)->AddRef();
                CoTaskMemFree(name);
                hr = S_OK;
                goto cleanup;
            }
            CoTaskMemFree(name);
        }
    }

    hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

cleanup:
    for (UINT32 i = 0; i < count; ++i) {
        if (ppDevices[i]) ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    if (pAttributes) pAttributes->Release();

    return hr;
}

HRESULT GetAudioDeviceActivate(const std::wstring& targetName, IMFActivate** ppActivateOut) {
    if (!ppActivateOut) return E_POINTER;
    *ppActivateOut = nullptr;

    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
    if (FAILED(hr)) goto cleanup;

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    if (FAILED(hr)) goto cleanup;

    for (UINT32 i = 0; i < count; ++i) {
        WCHAR* name = nullptr;
        UINT32 cchName = 0;
        hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &cchName);
        if (SUCCEEDED(hr)) {
            if (targetName == name) {
                *ppActivateOut = ppDevices[i];
                (*ppActivateOut)->AddRef();
                CoTaskMemFree(name);
                hr = S_OK;
                goto cleanup;
            }
            CoTaskMemFree(name);
        }
    }

    hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

cleanup:
    for (UINT32 i = 0; i < count; ++i) {
        if (ppDevices[i]) ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);
    if (pAttributes) pAttributes->Release();

    return hr;
}

// --- Vista previa ---
HRESULT CreateMediaSourceFromDevice(const std::wstring& friendlyName, IMFMediaSource** ppSource) {
    if (!ppSource) return E_POINTER;
    *ppSource = nullptr;

    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) goto cleanup;

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    if (FAILED(hr)) goto cleanup;

    for (UINT32 i = 0; i < count; ++i) {
        WCHAR* name = nullptr;
        UINT32 cchName = 0;
        hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &cchName);
        if (SUCCEEDED(hr)) {
            if (friendlyName == name) {
                hr = ppDevices[i]->ActivateObject(IID_PPV_ARGS(ppSource));
                CoTaskMemFree(name);
                goto cleanup;
            }
            CoTaskMemFree(name);
        }
    }

    hr = MF_E_NOT_FOUND;

cleanup:
    if (ppDevices) {
        for (UINT32 i = 0; i < count; ++i)
            if (ppDevices[i]) ppDevices[i]->Release();
        CoTaskMemFree(ppDevices);
    }
    if (pAttributes) pAttributes->Release();

    return hr;
}

// Variables globales
static HWND g_hwnd = nullptr;
static std::atomic<bool> g_previewRunning(false); // Usar atomic para acceso seguro entre hilos
static std::thread g_previewThread;
HBITMAP g_hBitmap = nullptr;
HGDIOBJ g_oldBitmap = nullptr;
HDC g_memDC = nullptr;
BITMAPINFO g_bmi = {};
void* g_pBits = nullptr;
static std::vector<std::string> g_cameraNames;

void FreeCameraList(wchar_t** names, int count) {
    for (int i = 0; i < count; ++i) {
        delete[] names[i];
    }
    delete[] names;
}

bool StopRecorder() {
    if (!g_ctx.isRecording) return false;

    g_ctx.isRecording = false;
    // g_ctx.isPaused = false;

    // Esperar que los hilos terminen
    if (g_ctx.recordingThread.joinable()) g_ctx.recordingThread.join();
    if (g_ctx.audioThread.joinable()) g_ctx.audioThread.join();

    HRESULT hr = S_OK;

    if (g_ctx.sinkWriter) {
        hr = g_ctx.sinkWriter->Finalize();
        if (FAILED(hr)) ReportError(L"falló creando el escritor de medios.");
        g_ctx.sinkWriter->Release();
    }

    if (g_ctx.videoReader) {
        g_ctx.videoReader->Release();
    }

    if (g_ctx.videoReader2) {
        g_ctx.videoReader2->Release();
    }

    if (g_ctx.audioReader) {
        g_ctx.audioReader->Release();
    }

    if (g_ctx.videoSource) {
        g_ctx.videoSource->Shutdown();
        g_ctx.videoSource->Release();
    }

    if (g_ctx.videoSource2) {
        g_ctx.videoSource2->Shutdown();
        g_ctx.videoSource2->Release();
    }

    if (g_ctx.audioSource) {
        g_ctx.audioSource->Shutdown();
        g_ctx.audioSource->Release();
    }

    ResetContext();
    return true;
}

// --- Inicialización y limpieza ---
void InitializeCameraSystem()
{
    if (g_ctx.videoReader != nullptr) return; // ya está inicializado
    MFStartup(MF_VERSION);
}

void ShutdownCameraSystem() {
    StopRecorder();
    ResetContext();

    for (auto dev : g_audioDevices) dev->Release();
    g_audioDevices.clear();

    if (g_ppDevices) {
        for (UINT32 i = 0; i < g_deviceCount; i++) g_ppDevices[i]->Release();
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
    if (!g_ppDevices || index < 0 || index >= (int)g_deviceCount) return;

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
        for (UINT32 i = 0; i < count; i++) g_audioDevices.push_back(ppDevices[i]);
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

void ConvertNV12ToRGB32(const BYTE* nv12, BYTE* rgb, int width, int height) {
    const BYTE* yPlane = nv12;
    const BYTE* uvPlane = nv12 + width * height;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int Y = yPlane[y * width + x];
            int U = uvPlane[(y / 2) * width + (x & ~1)];
            int V = uvPlane[(y / 2) * width + (x & ~1) + 1];

            int C = Y - 16;
            int D = U - 128;
            int E = V - 128;

            int R = (298 * C + 409 * E + 128) >> 8;
            int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            int B = (298 * C + 516 * D + 128) >> 8;

            R = R < 0 ? 0 : R > 255 ? 255 : R;
            G = G < 0 ? 0 : G > 255 ? 255 : G;
            B = B < 0 ? 0 : B > 255 ? 255 : B;

            int offset = (y * width + x) * 4;
            rgb[offset + 0] = (BYTE)B;
            rgb[offset + 1] = (BYTE)G;
            rgb[offset + 2] = (BYTE)R;
            rgb[offset + 3] = 255;
        }
    }
}


void ConvertYUY2ToRGB32(const BYTE* src, BYTE* dst, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 2) {
            int index = y * width * 2 + x * 2;

            BYTE Y0 = src[index];
            BYTE U = src[index + 1];
            BYTE Y1 = src[index + 2];
            BYTE V = src[index + 3];

            auto Convert = [](BYTE y, BYTE u, BYTE v) -> DWORD {
                int c = y - 16;
                int d = u - 128;
                int e = v - 128;
                int r = (298 * c + 409 * e + 128) >> 8;
                int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                int b = (298 * c + 516 * d + 128) >> 8;
                r = min(max(r, 0), 255);
                g = min(max(g, 0), 255);
                b = min(max(b, 0), 255);
                return RGB(b, g, r);
                };

            DWORD pixel1 = Convert(Y0, U, V);
            DWORD pixel2 = Convert(Y1, U, V);

            int dstIndex = y * width * 4 + x * 4;
            ((DWORD*)(dst + dstIndex))[0] = pixel1;
            ((DWORD*)(dst + dstIndex))[1] = pixel2;
        }
    }
}

int FindClosestCompatibleVideoFormat(IMFSourceReader* reader, int preferredIndex, IMFMediaType** outType, GUID* outSubtype) {
    std::vector<int> compatibleIndices;
    std::vector<IMFMediaType*> allTypes;

    // Recorrer todos los formatos
    for (DWORD i = 0; ; i++) {
        IMFMediaType* mt = nullptr;
        HRESULT hr = reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &mt);
        if (FAILED(hr) || !mt) break;

        GUID subtype = GUID_NULL;
        if (SUCCEEDED(mt->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            if (subtype == MFVideoFormat_NV12 || subtype == MFVideoFormat_YUY2) {
                compatibleIndices.push_back(i);
            }
        }

        allTypes.push_back(mt); // lo guardamos para liberar luego
    }

    if (compatibleIndices.empty()) {
        ReportError(L"No se encontró ningún formato compatible (NV12/YUY2)");
        return -1;
    }

    // Buscar el índice más cercano
    int closest = compatibleIndices[0];
    int minDistance = abs(preferredIndex - closest);

    for (int idx : compatibleIndices) {
        int dist = abs(preferredIndex - idx);
        if (dist < minDistance) {
            closest = idx;
            minDistance = dist;
        }
    }

    *outType = allTypes[closest];
    (*outType)->AddRef(); // Referencia extra para quien la usa
    if (outSubtype) (*outSubtype = GUID_NULL, (*outType)->GetGUID(MF_MT_SUBTYPE, outSubtype));

    // Liberar todos menos el seleccionado
    for (size_t i = 0; i < allTypes.size(); ++i) {
        if ((int)i != closest) allTypes[i]->Release();
    }

    return closest;
}

bool StartPreview(wchar_t* cameraName, int indexCam, HWND hwndPreview) {
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    HRESULT hr;

    if (FAILED(MFCreateAttributes(&pAttributes, 1))) return false;
    pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(MFEnumDeviceSources(pAttributes, &ppDevices, &count))) {
        pAttributes->Release();
        return false;
    }

    CameraInstance* instance = new CameraInstance();
    bool found = false;

    for (UINT32 i = 0; i < count; i++) {
        WCHAR* name = nullptr;
        UINT32 len = 0;
        if (SUCCEEDED(ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len))) {
            if (wcscmp(name, cameraName) == 0) {
                if (SUCCEEDED(ppDevices[i]->ActivateObject(IID_PPV_ARGS(&instance->mediaSource)))) {
                    found = true;
                }
                CoTaskMemFree(name);
                break;
            }
            CoTaskMemFree(name);
        }
    }

    for (UINT32 i = 0; i < count; i++) ppDevices[i]->Release();
    CoTaskMemFree(ppDevices);
    pAttributes->Release();

    if (!found || !instance->mediaSource) {
        delete instance;
        return false;
    }

    if (FAILED(MFCreateSourceReaderFromMediaSource(instance->mediaSource, nullptr, &instance->sourceReader))) {
        instance->mediaSource->Release();
        delete instance;
        return false;
    }

    IMFMediaType* selectedType = nullptr;
    GUID subtype = GUID_NULL;


    int selectedIndex = FindClosestCompatibleVideoFormat(instance->sourceReader, indexCam, &instance->nativeVideoType, nullptr);
    if (selectedIndex < 0) {
        return false;
    }
    IMFMediaType* pType = instance->nativeVideoType;

    UINT32 width = 0, height = 0;
    hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
        pType->Release();
        return false;
    }

    hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr)) {
        pType->Release();
        return false;
    }

    if (subtype == MFVideoFormat_NV12 || subtype == MFVideoFormat_YUY2) {
        selectedType = pType;  // Se usará más tarde y se liberará más adelante
    }
    else {
        ReportError(L"Formato no soportado");
        pType->Release();  // No es un formato soportado, liberamos de inmediato
    }

    if (!selectedType) {
        instance->sourceReader->Release();
        instance->mediaSource->Release();
        delete instance;
        return false;
    }

    instance->sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, selectedType);
    instance->videoSubtype = subtype;  // Guardamos el tipo para uso posterior
    selectedType->Release();

    instance->previewHwnd = hwndPreview;
    instance->stopPreview = false;

    instance->previewThread = std::thread([instance, width, height]() {
        HDC hdc = GetDC(instance->previewHwnd);
        BYTE* rgbBuffer = new BYTE[width * height * 4];

        Gdiplus::Graphics graphics(hdc);
        Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppRGB);
        Gdiplus::Rect rect(0, 0, width, height);

        while (!instance->stopPreview) {
            IMFSample* sample = nullptr;
            DWORD streamIdx = 0, flags = 0;
            LONGLONG ts = 0;

            if (SUCCEEDED(instance->sourceReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIdx, &flags, &ts, &sample)) && sample) {

                IMFMediaBuffer* buffer = nullptr;
                if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
                    BYTE* data = nullptr;
                    DWORD maxLen = 0, curLen = 0;

                    if (SUCCEEDED(buffer->Lock(&data, &maxLen, &curLen))) {
                        // Convertir según el tipo de video
                        if (instance->videoSubtype == MFVideoFormat_NV12) {
                            ConvertNV12ToRGB32(data, rgbBuffer, width, height);
                        }
                        else if (instance->videoSubtype == MFVideoFormat_YUY2) {
                            ConvertYUY2ToRGB32(data, rgbBuffer, width, height);
                        }
                        else {
                            ReportError(L"Formato no soportado");
                            return;
                        }

                        BitmapData bmpData;
                        if (bitmap.LockBits(&rect, ImageLockModeWrite, PixelFormat32bppRGB, &bmpData) == Ok) {
                            memcpy(bmpData.Scan0, rgbBuffer, width * height * 4);
                            bitmap.UnlockBits(&bmpData);

                            RECT rc;
                            GetClientRect(instance->previewHwnd, &rc);
                            int wndWidth = rc.right - rc.left;
                            int wndHeight = rc.bottom - rc.top;

                            graphics.DrawImage(&bitmap, 0, 0, wndWidth, wndHeight);
                        }

                        buffer->Unlock();
                    }
                    buffer->Release();
                }
                sample->Release();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }

        delete[] rgbBuffer;
        ReleaseDC(instance->previewHwnd, hdc);
        });

    std::lock_guard<std::mutex> lock(g_instancesMutex);
    g_instances[cameraName] = instance;
    return true;
}



//bool StartPreview(wchar_t* cameraFriendlyName, HWND hwnd, PreviewSession** outSession)
//{
//    if (!outSession) return false;
//
//    PreviewSession* session = new PreviewSession();
//    ZeroMemory(session, sizeof(PreviewSession));
//    session->hwndPreview = hwnd;
//    StopPreview(&session);
//
//    IMFPresentationDescriptor* pPD = nullptr;
//    IMFStreamDescriptor* pSD = nullptr;
//    IMFActivate* pSinkActivate = nullptr;
//    IMFTopologyNode* pSourceNode = nullptr;
//    IMFTopologyNode* pOutputNode = nullptr;
//
//    HRESULT hr;
//
//    if (FAILED(hr = CreateMediaSourceFromDevice(cameraFriendlyName, &session->pSource))) return false;
//    if (FAILED(hr = MFCreateMediaSession(nullptr, &session->pSession))) return false;
//    if (FAILED(hr = session->pSource->CreatePresentationDescriptor(&pPD))) return false;
//
//    BOOL fSelected;
//    if (FAILED(hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD))) return false;
//    if (FAILED(hr = MFCreateVideoRendererActivate(hwnd, &pSinkActivate))) return false;
//    if (FAILED(hr = MFCreateTopology(&session->pTopology))) return false;
//
//    if (FAILED(hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode))) return false;
//    pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, session->pSource);
//    pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
//    pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
//    session->pTopology->AddNode(pSourceNode);
//
//    if (FAILED(hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pOutputNode))) return false;
//    pOutputNode->SetObject(pSinkActivate);
//    session->pTopology->AddNode(pOutputNode);
//    pSourceNode->ConnectOutput(0, pOutputNode, 0);
//
//    hr = session->pSession->SetTopology(0, session->pTopology);
//
//    PROPVARIANT varStart;
//    PropVariantInit(&varStart);
//    hr = session->pSession->Start(&GUID_NULL, &varStart);
//    PropVariantClear(&varStart);
//
//    // Limpieza local (no limpia la sesión que sigue viva)
//    pPD->Release();
//    pSD->Release();
//    pSinkActivate->Release();
//    pSourceNode->Release();
//    pOutputNode->Release();
//
//    *outSession = session;
//
//    return SUCCEEDED(hr);
//}

//void StopPreview(PreviewSession** session)
//{
//    if (!session || !*session) return;
//    PreviewSession* s = *session;
//
//    if (s->pSession) {
//        HRESULT hr;
//
//        // Detener sesión
//        hr = s->pSession->Stop();
//        if (FAILED(hr)) ReportError(L"[ERROR] Función Detener falló\n");
//
//        // Cerrar sesión
//        hr = s->pSession->Close();
//        if (FAILED(hr)) ReportError(L"[ERROR] Función Cerrar falló\n");
//
//        // Esperar a que se cierre
//        IMFMediaEvent* pEvent = nullptr;
//        MediaEventType meType = MEUnknown;
//
//        while (SUCCEEDED(s->pSession->GetEvent(0, &pEvent))) {
//            if (SUCCEEDED(pEvent->GetType(&meType))) {
//                if (meType == MESessionClosed) {
//                    pEvent->Release();
//                    break;
//                }
//            }
//            pEvent->Release();
//        }
//
//        s->pSession->Shutdown();
//        s->pSession->Release();
//        s->pSession = nullptr;
//    }
//
//    if (s->hwndPreview) {
//        InvalidateRect(s->hwndPreview, NULL, TRUE);
//        UpdateWindow(s->hwndPreview);
//        s->hwndPreview = nullptr;
//    }
//
//    if (s->pSource) {
//        s->pSource->Release();
//        s->pSource = nullptr;
//    }
//
//    if (s->pTopology) {
//        s->pTopology->Release();
//        s->pTopology = nullptr;
//    }
//}

//bool PauseRecorder() {
//    if (g_ctx.isRecording) {
//        if (!g_ctx.isPaused)
//        {
//            LONGLONG currentTime = MFGetSystemTime();
//            if ((currentTime - g_ctx.recordingStartTime) < MIN_TIME_BEFORE_PAUSE_ALLOWED) return false;
//            g_ctx.pauseStartTime = MFGetSystemTime();
//            g_ctx.isPaused = true;
//        }
//        else return false;
//    }
//    else return false;
//    return true;
//}

//bool ContinueRecorder() {
//    if (g_ctx.isRecording) {
//        if (g_ctx.isPaused)
//        {
//            LONGLONG resumeTime = MFGetSystemTime();
//            LONGLONG currentPauseDuration = resumeTime - g_ctx.pauseStartTime;
//            LONGLONG oldTotalPausedDuration = g_ctx.totalPausedDuration;
//            LONGLONG now = MFGetSystemTime();
//            g_ctx.totalPausedDuration += (now - g_ctx.pauseStartTime);
//            g_ctx.isPaused = false;
//        }
//        else return false;
//    }
//    else return false;
//    return true;
//}

//bool PauseRecording() { return g_ctx.isRecording ? PauseRecorder() : false; }
//bool ContinueRecording() { return g_ctx.isRecording ? ContinueRecorder() : false; }

bool StopRecording() { return g_ctx.isRecording ? StopRecorder() : false; }

bool SetPreferredMediaType(
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
        MFVideoFormat_YUY2,
        // Si encuentras otros GUIDs para formatos comunes como H.264 o otros YUV que la cámara
        // soporte y que creas que MF pueda transcodificar a NV12, puedes añadirlos aquí.
    };

    // Primero, intenta buscar el tipo de medio ideal (NV12 con resolución y FPS exactos)
    for (DWORD i = 0; ; ++i) {
        hr = pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pMediaType);
        if (FAILED(hr)) {
            if (hr == MF_E_NO_MORE_TYPES) hr = S_OK;
            break;
        }

        GUID subtypeGuid = GUID_NULL;
        UINT32 width_native = 0, height_native = 0, num_native = 0, den_native = 0;

        if (SUCCEEDED(pMediaType->GetGUID(MF_MT_SUBTYPE, &subtypeGuid)) &&
            SUCCEEDED(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width_native, &height_native)) &&
            SUCCEEDED(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &num_native, &den_native))) {

            // ... rest of your SetPreferredMediaType logic for finding preferred types ...
            // If it's the ideal NV12
            if (width_native == targetWidth && height_native == targetHeight &&
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
    if (FAILED(hr)) goto cleanup;

    hr = pFoundType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) goto cleanup;
    hr = pFoundType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeSize(pFoundType, MF_MT_FRAME_SIZE, targetWidth, targetHeight);
    if (FAILED(hr)) goto cleanup;
    hr = MFSetAttributeRatio(pFoundType, MF_MT_FRAME_RATE, targetFpsNum, targetFpsDen);
    if (FAILED(hr)) goto cleanup;

found_type:
    // Una vez que tengamos pFoundType (ya sea nativo o recién creado), intentamos aplicarlo
    if (pFoundType) {
        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pFoundType);
        if (FAILED(hr)) {
            ReportError(L"Falló confgurando el formato");
            goto cleanup;
        }
    }
    else {
        hr = E_UNEXPECTED; // No se encontró ningún tipo de medio para intentar
        ReportError(L"No se encontró ningún tipo de medio para intentar");
        goto cleanup;
    }
    return true;

cleanup:
    if (pFoundType) { pFoundType->Release(); }
    return false;
}

bool StartRecording(wchar_t* cameraFriendlyName, wchar_t* micFriendlyName, const wchar_t* outputPath, int bitrate) {
    if (g_ctx.isRecording) {
        ReportError(L"Error: La grabación ya está en curso.");
        return false;
    }
    StopRecorder();
    ResetContext();
    BOOL moved;
    HRESULT hr = S_OK;

    UINT32 num = 0, den = 0;
    UINT32 width = 0, height = 0;
    IMFAttributes* pAttributes = nullptr;

    IMFMediaType* outVideoType = nullptr;
    IMFActivate* pVideoActivate = nullptr;

    IMFMediaType* nativeAudioType = nullptr;
    IMFMediaType* outAudioType = nullptr;
    IMFActivate* pAudioActivate = nullptr;

    if (g_ctx.isRecording.load()) return false;

    CameraInstance* instance = g_instances[cameraFriendlyName];
    if (!instance || !instance->sourceReader || !instance->mediaSource) return false;
    IMFSourceReader* pReader = instance->sourceReader;

    if (!instance->mediaSource)
    {
        ReportError(L"Error: instancia de la camara 1 no existe");
        return false;
    }

    if (!instance->sourceReader)
    {
        ReportError(L"Error: instancia lectora de la camara 1 no existe");
        return false;
    }

    hr = GetAudioDeviceActivate(micFriendlyName, &pAudioActivate);
    if (FAILED(hr)) goto error;
    hr = pAudioActivate->ActivateObject(IID_PPV_ARGS(&g_ctx.audioSource));
    pAudioActivate->Release();
    if (FAILED(hr)) goto error;

    hr = MFCreateSourceReaderFromMediaSource(g_ctx.audioSource, nullptr, &g_ctx.audioReader);
    if (FAILED(hr)) goto error;

    /*hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, nativeVideoType);
    if (FAILED(hr)) goto error;
    ReportError(L"Error: 7");*/

    // 4. Obtener tipo nativo audio
    hr = g_ctx.audioReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &nativeAudioType);
    if (FAILED(hr)) goto error;

    hr = g_ctx.audioReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, nativeAudioType);
    if (FAILED(hr)) goto error;

    hr = MFCreateAttributes(&pAttributes, 1); if (FAILED(hr)) return hr;

    // Habilitar CBR explícitamente
    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE); // Opcional, para usar HW
    hr = pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE); // Opcional, si es necesario

    // 5. Crear SinkWriter con archivo temporal
    wchar_t tempPath[MAX_PATH];
    wchar_t outputPathCopy[MAX_PATH];
    wcsncpy_s(outputPathCopy, outputPath, MAX_PATH);

    // Elimina extensión original y añade .tmp.mp4
    PathRemoveExtensionW(outputPathCopy);
    swprintf_s(tempPath, L"%s.tmp.mp4", outputPathCopy);

    hr = MFCreateSinkWriterFromURL(tempPath, nullptr, pAttributes, &g_ctx.sinkWriter);
    if (FAILED(hr)) goto error;

    // 6. Configurar tipo salida video (H264)
    hr = MFCreateMediaType(&outVideoType); if (FAILED(hr)) goto error;
    hr = outVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); if (FAILED(hr)) goto error;
    hr = outVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264); if (FAILED(hr)) goto error;
    hr = outVideoType->SetUINT32(MF_MT_AVG_BITRATE, bitrate); if (FAILED(hr)) goto error;
    hr = outVideoType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive); if (FAILED(hr)) goto error;

    // Extraer resolución y frame rate del tipo nativo seleccionado
    hr = MFGetAttributeSize(instance->nativeVideoType, MF_MT_FRAME_SIZE, &width, &height); if (FAILED(hr)) goto error;
    hr = MFGetAttributeRatio(instance->nativeVideoType, MF_MT_FRAME_RATE, &num, &den); if (FAILED(hr)) {
        num = 30; den = 1; // Valor por defecto si falla
    }

    // Aplicar los mismos valores al tipo de salida
    hr = MFSetAttributeSize(outVideoType, MF_MT_FRAME_SIZE, width, height); if (FAILED(hr)) goto error;
    hr = MFSetAttributeRatio(outVideoType, MF_MT_FRAME_RATE, num, den); if (FAILED(hr)) goto error;
    hr = MFSetAttributeRatio(outVideoType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1); if (FAILED(hr)) goto error;

    // Crear stream de salida de video
    hr = g_ctx.sinkWriter->AddStream(outVideoType, &g_ctx.videoStreamIndex); if (FAILED(hr)) goto error;

    // Asignar el tipo de entrada al sinkWriter tal como viene del dispositivo
    hr = g_ctx.sinkWriter->SetInputMediaType(g_ctx.videoStreamIndex, instance->nativeVideoType, nullptr); if (FAILED(hr)) goto error;

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
    g_ctx.recordingThread = std::thread([num, den, pReader]() {
        DWORD streamIndex, flags;
        LONGLONG sampleTime = 0;
        LONGLONG frameDuration = (10 * 1000 * 1000 * den) / num;

        while (g_ctx.isRecording) {
            if (g_ctx.isPaused) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            IMFSample* pSample = nullptr;
            HRESULT hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &sampleTime, &pSample);

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
    if (nativeAudioType) nativeAudioType->Release();
    if (outVideoType) outVideoType->Release();
    if (outAudioType) outAudioType->Release();

    moved = MoveFileExW(tempPath, outputPath, MOVEFILE_REPLACE_EXISTING);
    if (!moved) {
        DeleteFileW(tempPath);
        ReportError(L"No se pudo mover el archivo temporal al destino final.");
    }
    return true;

error:
    if (nativeAudioType) nativeAudioType->Release();
    if (outVideoType) outVideoType->Release();
    if (outAudioType) outAudioType->Release();
    if (instance->mediaSource) { instance->mediaSource->Release(); instance->mediaSource = nullptr; }
    if (g_ctx.audioSource) { g_ctx.audioSource->Release(); g_ctx.audioSource = nullptr; }
    if (pReader) { pReader->Release(); pReader = nullptr; }
    if (g_ctx.audioReader) { g_ctx.audioReader->Release(); g_ctx.audioReader = nullptr; }
    if (g_ctx.sinkWriter) { g_ctx.sinkWriter->Release(); g_ctx.sinkWriter = nullptr; }

    DeleteFileW(tempPath);
    ResetContext();
    ReportError(L"Error: La grabación no pudo ejecutarse.");
    return false;
}


void FindCompatibleCommonFormat(wchar_t* selectedNameCam1, wchar_t* selectedNameCam2, int* indicesOut, int* countOut) {
    *countOut = 0;

    if (g_instances.size() < 2) {
        ReportError(L"Se requieren al menos dos cámaras activas para comparar formatos.");
        return;
    }

    CameraInstance* cam1 = g_instances[selectedNameCam1];
    CameraInstance* cam2 = g_instances[selectedNameCam2];

    if (!cam1 || !cam2 || !cam1->sourceReader || !cam2->sourceReader) {
        ReportError(L"No se encontraron readers válidos para ambas cámaras.");
        return;
    }

    struct Format {
        UINT32 w, h, fpsNum, fpsDen;
        GUID subtype;
        DWORD index;
    };

    std::vector<Format> formats1, formats2;

    auto CollectFormats = [](IMFSourceReader* reader, std::vector<Format>& list) {
        for (DWORD i = 0;; ++i) {
            ComPtr<IMFMediaType> type;
            if (FAILED(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &type)))
                break;

            GUID subtype = GUID_NULL;
            UINT32 w = 0, h = 0, fpsNum = 0, fpsDen = 0;

            if (SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) &&
                SUCCEEDED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &w, &h)) &&
                SUCCEEDED(MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &fpsNum, &fpsDen))) {

                if ((subtype == MFVideoFormat_NV12 || subtype == MFVideoFormat_YUY2)) {
                    list.push_back({ w, h, fpsNum, fpsDen, subtype, i });
                }
            }
        }
        };

    CollectFormats(cam1->sourceReader, formats1);
    CollectFormats(cam2->sourceReader, formats2);

    // 1. Buscar específicamente 640x480 a 30fps
    for (const auto& f1 : formats1) {
        if (f1.w == 640 && f1.h == 480 && f1.fpsNum == 30 && f1.fpsDen == 1) {
            for (const auto& f2 : formats2) {
                if (f2.w == 640 && f2.h == 480 &&
                    f2.fpsNum == 30 && f2.fpsDen == 1 &&
                    f1.subtype == f2.subtype) {

                    indicesOut[0] = static_cast<int>(f1.index);
                    indicesOut[1] = static_cast<int>(f2.index);
                    *countOut = 2;
                    return;
                }
            }
        }
    }

    // 2. Buscar el primer formato compatible entre las dos cámaras
    for (const auto& f1 : formats1) {
        for (const auto& f2 : formats2) {
            if (f1.w == f2.w &&
                f1.h == f2.h &&
                f1.subtype == f2.subtype &&
                abs((int)f1.fpsNum - (int)f2.fpsNum) <= 5 &&
                f1.fpsDen == f2.fpsDen) {

                indicesOut[0] = static_cast<int>(f1.index);
                indicesOut[1] = static_cast<int>(f2.index);
                *countOut = 2;
                return;
            }
        }
    }

    ReportError(L"No se encontró ningún formato compatible entre ambas cámaras.");
}




void PrintMediaTypeInfo(IMFMediaType* mediaType, const wchar_t* label = L"MediaType") {
    if (!mediaType) {
        ReportError(L"MediaType es nulo");
        return;
    }

    UINT32 width = 0, height = 0;
    UINT32 fpsNum = 0, fpsDen = 0;
    GUID subtype = GUID_NULL;

    HRESULT hr = S_OK;

    hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
        ReportError(L"Error al obtener resolución.");
    }

    hr = MFGetAttributeRatio(mediaType, MF_MT_FRAME_RATE, &fpsNum, &fpsDen);
    if (FAILED(hr)) {
        ReportError(L"Error al obtener framerate.");
    }

    hr = mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr)) {
        ReportError(L"Error al obtener subtype.");
    }

    const wchar_t* formatName = L"Desconocido";
    if (subtype == MFVideoFormat_NV12) formatName = L"NV12";
    else if (subtype == MFVideoFormat_YUY2) formatName = L"YUY2";
    else if (subtype == MFVideoFormat_MJPG) formatName = L"MJPEG";
    else if (subtype == MFVideoFormat_RGB32) formatName = L"RGB32";

    wchar_t msg[200];
    swprintf(msg, 200, L"%s: %ux%u @ %u/%u fps - Formato: %s", label, width, height, fpsNum, fpsDen, formatName);
    ReportError(msg);
}

void ConvertYUY2ToNV12(const BYTE* source, BYTE* destY, BYTE* destUV, int width, int height) {
    const int srcStride = width * 2;
    const int dstYStride = width;
    const int dstUVStride = width;

    for (int y = 0; y < height; ++y) {
        const BYTE* srcLine = source + y * srcStride;
        BYTE* dstYLine = destY + y * dstYStride;

        for (int x = 0; x < width; x += 2) {
            BYTE Y0 = srcLine[x * 2 + 0];
            BYTE U = srcLine[x * 2 + 1];
            BYTE Y1 = srcLine[x * 2 + 2];
            BYTE V = srcLine[x * 2 + 3];

            dstYLine[x + 0] = Y0;
            dstYLine[x + 1] = Y1;

            if ((y % 2) == 0) {
                int uvIndex = (y / 2) * dstUVStride + x;
                destUV[uvIndex + 0] = U;
                destUV[uvIndex + 1] = V;
            }
        }
    }
}


bool StartRecordingTwoCameras(wchar_t* cameraFriendlyName, wchar_t* cameraFriendlyName2, wchar_t* micFriendlyName, const wchar_t* outputPath, int bitrate) {
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
    ResetContext();
    // Si StopRecorder() no resetea isRecording, hazlo aquí.
    HRESULT hr = S_OK;

    // --- Punteros COM (Inicialización a nullptr para limpieza segura) ---
    // Usaremos SAFE_RELEASE en el 'error' block.
    IMFAttributes* pAttributes = nullptr;
    IMFMediaType* outVideoType = nullptr;
    IMFMediaType* outAudioType = nullptr;
    IMFActivate* pAudioActivate = nullptr;
    IMFActivate* pVideoActivate1 = nullptr;
    IMFActivate* pVideoActivate2 = nullptr;
    IMFMediaType* sinkInputVideoType = nullptr;
    IMFMediaType* nativeVideoType = nullptr;
    IMFMediaType* nativeAudioType = nullptr;

    CameraInstance* instance = g_instances[cameraFriendlyName];
    if (!instance || !instance->sourceReader || !instance->mediaSource)
        return false;

    IMFSourceReader* pReader = instance->sourceReader;

    if (!instance->mediaSource)
    {
        ReportError(L"Error: instancia de la camara 1 no existe");
        return false;
    }

    CameraInstance* instance2 = g_instances[cameraFriendlyName2];
    if (!instance2 || !instance2->sourceReader || !instance2->mediaSource)
        return false;

    IMFSourceReader* pReader2 = instance2->sourceReader;

    if (!instance2->mediaSource)
    {
        ReportError(L"Error: instancia de la camara 2 no existe");
        return false;
    }

    IMFMediaType* nativeVideoType11 = nullptr;

    UINT32 width = 0, height = 0, fpsNum = 0, fpsDen = 0;

    hr = MFGetAttributeSize(instance2->nativeVideoType, MF_MT_FRAME_SIZE, &width, &height);
    hr = MFGetAttributeRatio(instance2->nativeVideoType, MF_MT_FRAME_RATE, &fpsNum, &fpsDen);

    // --- Parámetros de Video de Salida Compuesto ---
    UINT32 singleCamInputWidth = width;
    UINT32 singleCamInputHeight = height;
    UINT32 finalOutputWidth = width;
    UINT32 finalOutputHeight = singleCamInputHeight * 2;
    UINT32 finalOutputFpsNum = fpsNum; // Numerador de FPS del video compuesto
    UINT32 finalOutputFpsDen = fpsDen;  // Denominador de FPS del video compuesto (ej: 30/1 = 30 FPS)

    // Obtener activador de audio (si micIndex es válido)
    if (micFriendlyName) { // Asumo que micIndex < 0 significa no grabar audio
        hr = GetAudioDeviceActivate(micFriendlyName, &pAudioActivate); CHECK_HR(hr, "GetAudioDeviceActivate");
        hr = pAudioActivate->ActivateObject(IID_PPV_ARGS(&g_ctx.audioSource)); CHECK_HR(hr, "ActivateObject (Audio)");
    }
    else {
        g_ctx.audioSource = nullptr; // Asegurarse de que sea nulo si no se usa
        g_ctx.audioReader = nullptr; // Asegurarse de que sea nulo si no se usa
    }

    if (g_ctx.audioSource) { // Solo si hay fuente de audio
        hr = MFCreateSourceReaderFromMediaSource(g_ctx.audioSource, nullptr, &g_ctx.audioReader); CHECK_HR(hr, "MFCreateSourceReaderFromMediaSource (Audio)");
    }

    // 4. Obtener tipo nativo audio
    hr = g_ctx.audioReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &nativeAudioType);
    if (FAILED(hr)) goto error;
    hr = g_ctx.audioReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, nativeAudioType);
    if (FAILED(hr)) goto error;

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

    // 5. Crear SinkWriter con archivo temporal
    wchar_t tempPath[MAX_PATH];
    wchar_t outputPathCopy[MAX_PATH];
    wcsncpy_s(outputPathCopy, outputPath, MAX_PATH);

    // Elimina extensión original y añade .tmp.mp4
    PathRemoveExtensionW(outputPathCopy);
    swprintf_s(tempPath, L"%s.tmp.mp4", outputPathCopy);

    hr = MFCreateSinkWriterFromURL(tempPath, nullptr, pAttributes, &g_ctx.sinkWriter);
    CHECK_HR(hr, "MFCreateSinkWriterFromURL");
    SAFE_RELEASE(pAttributes);


    // 6. Configurar tipo de salida de video (H.264)
    hr = MFCreateMediaType(&outVideoType); CHECK_HR(hr, "MFCreateMediaType for outVideoType");
    hr = outVideoType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); CHECK_HR(hr, "SetGUID MF_MT_MAJOR_TYPE (Video)");
    hr = outVideoType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264); CHECK_HR(hr, "SetGUID MF_MT_SUBTYPE (H264)");
    hr = outVideoType->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
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
    if (pReader && pReader2)
    {
        g_ctx.recordingThread = std::thread([
            finalOutputFpsNum, finalOutputFpsDen, finalOutputWidth, finalOutputHeight,
            singleCamInputWidth, singleCamInputHeight, pReader, pReader2, instance, instance2]()
            {
                HRESULT hr_thread = S_OK;
                DWORD streamIndex, flags;
                LONGLONG sampleTime = 0, sampleTime2 = 0;
                LONGLONG frameDuration = (10 * 1000 * 1000 * finalOutputFpsDen) / finalOutputFpsNum;

                UINT32 singleCamYSize = singleCamInputWidth * singleCamInputHeight;
                UINT32 singleCamUVSize = singleCamYSize / 2;
                UINT32 totalCombinedYSize = finalOutputWidth * finalOutputHeight;
                UINT32 totalCombinedUVSize = totalCombinedYSize / 2;
                UINT32 totalCombinedFrameSize = totalCombinedYSize + totalCombinedUVSize;

                bool cam1IsYUY2 = (instance->videoSubtype == MFVideoFormat_YUY2);
                bool cam2IsYUY2 = (instance->videoSubtype == MFVideoFormat_YUY2);

                while (g_ctx.isRecording.load()) {
                    if (g_ctx.isPaused.load()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    }

                    IMFSample* pSample1 = nullptr;
                    IMFSample* pSample2 = nullptr;

                    hr_thread = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &sampleTime, &pSample1);
                    if (FAILED(hr_thread) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;

                    hr_thread = pReader2->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &sampleTime2, &pSample2);
                    if (FAILED(hr_thread) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;

                    if (pSample1 && pSample2) {
                        if (g_ctx.baseTime == -1 && sampleTime > 0) g_ctx.baseTime = sampleTime;

                        LONGLONG adjustedTime1 = (sampleTime - g_ctx.baseTime) - g_ctx.totalPausedDuration;
                        LONGLONG adjustedTime2 = (sampleTime2 - g_ctx.baseTime) - g_ctx.totalPausedDuration;

                        IMFSample* pCombinedSample = nullptr;
                        IMFMediaBuffer* pCombinedBuffer = nullptr;

                        hr_thread = MFCreateSample(&pCombinedSample);
                        if (FAILED(hr_thread)) break;
                        hr_thread = MFCreateMemoryBuffer(totalCombinedFrameSize, &pCombinedBuffer);
                        if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedSample); break; }
                        hr_thread = pCombinedSample->AddBuffer(pCombinedBuffer);
                        if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pCombinedBuffer); break; }

                        BYTE* pCombinedData = nullptr;
                        DWORD cbCombinedMaxLength = 0, cbCombinedCurrentLength = 0;
                        hr_thread = pCombinedBuffer->Lock(&pCombinedData, &cbCombinedMaxLength, &cbCombinedCurrentLength);
                        if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pCombinedBuffer); break; }

                        BYTE* combinedY = pCombinedData;
                        BYTE* combinedUV = pCombinedData + totalCombinedYSize;

                        // === Cámara 1 ===
                        IMFMediaBuffer* pBuffer1 = nullptr;
                        hr_thread = pSample1->ConvertToContiguousBuffer(&pBuffer1);
                        if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample1); SAFE_RELEASE(pSample2); continue; }
                        BYTE* pData1 = nullptr;
                        hr_thread = pBuffer1->Lock(&pData1, nullptr, nullptr);
                        if (FAILED(hr_thread)) { SAFE_RELEASE(pBuffer1); SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample1); SAFE_RELEASE(pSample2); continue; }

                        if (cam1IsYUY2) {
                            ConvertYUY2ToNV12(pData1, combinedY, combinedUV, singleCamInputWidth, singleCamInputHeight);
                        }
                        else {
                            memcpy(combinedY, pData1, singleCamYSize);
                            memcpy(combinedUV, pData1 + singleCamYSize, singleCamUVSize);
                        }

                        pBuffer1->Unlock(); SAFE_RELEASE(pBuffer1); SAFE_RELEASE(pSample1);

                        // === Cámara 2 ===
                        IMFMediaBuffer* pBuffer2 = nullptr;
                        hr_thread = pSample2->ConvertToContiguousBuffer(&pBuffer2);
                        if (FAILED(hr_thread)) { SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample2); continue; }
                        BYTE* pData2 = nullptr;
                        hr_thread = pBuffer2->Lock(&pData2, nullptr, nullptr);
                        if (FAILED(hr_thread)) { SAFE_RELEASE(pBuffer2); SAFE_RELEASE(pCombinedBuffer); SAFE_RELEASE(pCombinedSample); SAFE_RELEASE(pSample2); continue; }

                        if (cam2IsYUY2) {
                            ConvertYUY2ToNV12(pData2, combinedY + singleCamYSize, combinedUV + singleCamUVSize,
                                singleCamInputWidth, singleCamInputHeight);
                        }
                        else {
                            memcpy(combinedY + singleCamYSize, pData2, singleCamYSize);
                            memcpy(combinedUV + singleCamUVSize, pData2 + singleCamYSize, singleCamUVSize);
                        }

                        pBuffer2->Unlock(); SAFE_RELEASE(pBuffer2); SAFE_RELEASE(pSample2);

                        pCombinedBuffer->SetCurrentLength(totalCombinedFrameSize);
                        pCombinedBuffer->Unlock(); SAFE_RELEASE(pCombinedBuffer);

                        pCombinedSample->SetSampleTime(adjustedTime1);
                        pCombinedSample->SetSampleDuration(frameDuration);
                        hr_thread = g_ctx.sinkWriter->WriteSample(g_ctx.videoStreamIndex, pCombinedSample);
                        SAFE_RELEASE(pCombinedSample);

                        if (FAILED(hr_thread)) {
                            g_ctx.isRecording.store(false);
                            break;
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    else {
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
                    if (g_ctx.baseTime == -1 && sampleTime > 0) g_ctx.baseTime.store(sampleTime);

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

    MoveFileExW(tempPath, outputPath, MOVEFILE_REPLACE_EXISTING);
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
    SAFE_RELEASE(instance->mediaSource);
    SAFE_RELEASE(instance2->mediaSource);
    SAFE_RELEASE(g_ctx.audioSource);
    SAFE_RELEASE(pReader);
    SAFE_RELEASE(pReader2);
    SAFE_RELEASE(g_ctx.audioReader);
    SAFE_RELEASE(g_ctx.sinkWriter);

    // Asegurarse de que el contexto se resetee y la bandera de grabación se ponga a false
    DeleteFileW(tempPath);
    g_ctx.isRecording.store(false);
    ResetContext(); // Si ResetContext libera los anteriores, algunos SAFE_RELEASE serían redundantes

    // NOTA: Los hilos `recordingThread` y `audioThread` NO se unen/esperan aquí en caso de error
    // porque es probable que el error ocurra ANTES de que se lancen, o el error está
    // en una inicialización que los hilos usarían y fallarían de inmediato.
    // StopRecorder() se encargará de unirlos en una llamada posterior si están running.

    return false; // Indicar que StartRecording falló
}

bool GetSupportedFormats(wchar_t* cameraFriendlyName, CameraFormat** formats, int* count) {
    *formats = nullptr;
    *count = 0;

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return false;

    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 numDevices = 0;
    std::vector<CameraFormat> result;

    IMFMediaSource* pSource = nullptr;
    IMFPresentationDescriptor* pPD = nullptr;
    IMFStreamDescriptor* pSD = nullptr;
    IMFMediaTypeHandler* pHandler = nullptr;

    DWORD typeCount = 0;
    BOOL selected = FALSE;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (SUCCEEDED(hr)) hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (SUCCEEDED(hr)) hr = MFEnumDeviceSources(pAttributes, &ppDevices, &numDevices);
    if (FAILED(hr)) goto cleanup;

    for (UINT32 i = 0; i < numDevices; ++i) {
        WCHAR* name = nullptr;
        UINT32 cchName = 0;
        hr = ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &cchName);
        if (SUCCEEDED(hr)) {
            if (wcscmp(cameraFriendlyName, name) == 0) {
                hr = ppDevices[i]->ActivateObject(IID_PPV_ARGS(&pSource));
                CoTaskMemFree(name);
                break;
            }
            CoTaskMemFree(name);
        }
    }

    hr = pSource->CreatePresentationDescriptor(&pPD);
    if (FAILED(hr)) goto cleanup;

    hr = pPD->GetStreamDescriptorByIndex(0, &selected, &pSD);
    if (FAILED(hr)) goto cleanup;

    hr = pSD->GetMediaTypeHandler(&pHandler);
    if (FAILED(hr)) goto cleanup;

    hr = pHandler->GetMediaTypeCount(&typeCount);
    if (FAILED(hr)) goto cleanup;

    for (DWORD i = 0; i < typeCount; ++i) {
        IMFMediaType* pType = nullptr;
        GUID subtype = GUID_NULL;
        UINT32 width = 0, height = 0, num = 0, den = 0;

        hr = pHandler->GetMediaTypeByIndex(i, &pType);
        if (FAILED(hr)) continue;

        CameraFormat fmt = {};

        if (SUCCEEDED(pType->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            std::string s_subtype = GUIDToSubtypeString(subtype);
            snprintf(fmt.subtype, MAX_SUBTYPE_NAME_LEN, "%s", s_subtype.c_str());
        }

        if (SUCCEEDED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height))) {
            fmt.width = width;
            fmt.height = height;
        }

        if (SUCCEEDED(MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den))) {
            fmt.fps_num = num;
            fmt.fps_den = den;
        }

        result.push_back(fmt);
        pType->Release();
    }

cleanup:
    if (pHandler) pHandler->Release();
    if (pSD) pSD->Release();
    if (pPD) pPD->Release();
    if (pSource) pSource->Release();
    for (UINT32 j = 0; j < numDevices; ++j) {
        if (ppDevices[j]) ppDevices[j]->Release();
    }
    CoTaskMemFree(ppDevices);
    if (pAttributes) pAttributes->Release();
    MFShutdown();

    if (!result.empty()) {
        *count = (int)result.size();
        *formats = new CameraFormat[*count];
        memcpy(*formats, result.data(), sizeof(CameraFormat) * (*count));
        return true;
    }
    return false;
}


void FreeFormats(CameraFormat* formats) { delete[] formats; }

bool SaveBMP(wchar_t* filename, BYTE* data, LONG width, LONG height, DWORD stride) {
    BITMAPFILEHEADER bfh = {};
    BITMAPINFOHEADER bih = {};

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = -height; // negativo para top-down
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    DWORD imageSize = stride * height;
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    file.write((const char*)&bfh, sizeof(bfh));
    file.write((const char*)&bih, sizeof(bih));
    file.write((const char*)data, imageSize);
    return true;
}
// adsfs
bool CaptureSnapshott(wchar_t* cameraFriendlyName, wchar_t* outputPath) {
    HRESULT hr = S_OK;

    CameraInstance* instance = g_instances[cameraFriendlyName];
    if (!instance || !instance->sourceReader || !instance->mediaSource)
        return false;

    IMFSourceReader* pReader = instance->sourceReader;

    // Leer un frame
    ComPtr<IMFSample> pSample;
    DWORD streamIndex = 0, flags = 0;
    LONGLONG timestamp = 0;
    for (int tries = 0; tries < 10; ++tries) {
        hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &timestamp, &pSample);
        if (FAILED(hr)) break;
        if (pSample) break;
        Sleep(100);
    }
    if (!pSample) return false;

    // Obtener tipo de entrada actual
    ComPtr<IMFMediaType> pCurrentType;
    hr = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
    if (FAILED(hr)) return false;

    UINT32 width = 0, height = 0;
    MFGetAttributeSize(pCurrentType.Get(), MF_MT_FRAME_SIZE, &width, &height);

    GUID subtype = {};
    hr = pCurrentType->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (FAILED(hr)) return false;

    // Crear transform para convertir a RGB32
    ComPtr<IMFTransform> pTransform;
    hr = CoCreateInstance(CLSID_CColorConvertDMO, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pTransform));
    if (FAILED(hr)) return false;

    ComPtr<IMFMediaType> pInputType;
    MFCreateMediaType(&pInputType);
    pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pInputType->SetGUID(MF_MT_SUBTYPE, subtype); // usa el tipo actual (ej. NV12 o YUY2)
    MFSetAttributeSize(pInputType.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(pInputType.Get(), MF_MT_FRAME_RATE, 30, 1);
    hr = pTransform->SetInputType(0, pInputType.Get(), 0);
    if (FAILED(hr)) return false;

    ComPtr<IMFMediaType> pOutputType;
    MFCreateMediaType(&pOutputType);
    hr = pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) return false;
    hr = pOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(hr)) return false;
    hr = pOutputType->SetUINT32(MF_MT_AVG_BITRATE, 8000000);
    if (FAILED(hr)) return false;
    MFSetAttributeSize(pOutputType.Get(), MF_MT_FRAME_SIZE, width, height);
    MFSetAttributeRatio(pOutputType.Get(), MF_MT_FRAME_RATE, 30, 1);
    hr = pTransform->SetOutputType(0, pOutputType.Get(), 0);
    if (FAILED(hr)) return false;

    // Configurar transform
    pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    pTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    hr = pTransform->ProcessInput(0, pSample.Get(), 0);
    if (FAILED(hr)) return false;

    // Preparar sample de salida
    MFT_OUTPUT_STREAM_INFO streamInfo = {};
    hr = pTransform->GetOutputStreamInfo(0, &streamInfo);
    if (FAILED(hr)) return false;

    ComPtr<IMFMediaBuffer> pBufferOut;
    hr = MFCreateMemoryBuffer(streamInfo.cbSize, &pBufferOut);
    if (FAILED(hr)) return false;

    ComPtr<IMFSample> pSampleOut;
    MFCreateSample(&pSampleOut);
    pSampleOut->AddBuffer(pBufferOut.Get());

    MFT_OUTPUT_DATA_BUFFER outputData = {};
    outputData.dwStreamID = 0;
    outputData.pSample = pSampleOut.Get();

    DWORD status = 0;
    hr = pTransform->ProcessOutput(0, 1, &outputData, &status);
    if (FAILED(hr)) return false;

    // Guardar BMP
    BYTE* pData = nullptr;
    DWORD maxLen = 0, currLen = 0;
    hr = pBufferOut->Lock(&pData, &maxLen, &currLen);
    if (FAILED(hr) || currLen == 0) return false;

    LONG stride = width * 4;
    bool result = SaveBMP(outputPath, pData, width, height, stride);

    pBufferOut->Unlock();
    return result;
}

void StopPreview(wchar_t* cameraFriendlyName)
{
    if (g_ctx.isRecording == true)
    {
        ReportError(L"primero detenga la grabacion.");
        return;
    }

    CameraInstance* instance = g_instances[cameraFriendlyName];
    if (!instance) return;

    // Señal para detener el hilo
    instance->stopPreview = true;

    // Esperar a que termine el hilo de preview
    if (instance->previewThread.joinable()) {
        instance->previewThread.join();
    }

    // Liberar Media Foundation resources
    if (instance->sourceReader) {
        instance->sourceReader->Release();
        instance->sourceReader = nullptr;
    }

    if (instance->mediaSource) {
        instance->mediaSource->Shutdown();
        instance->mediaSource->Release();
        instance->mediaSource = nullptr;
    }

    // Limpieza de ventana
    if (instance->previewHwnd) {
        InvalidateRect(instance->previewHwnd, NULL, TRUE);
        UpdateWindow(instance->previewHwnd);
        instance->previewHwnd = nullptr;
    }

    // Eliminar instancia
    delete instance;
}