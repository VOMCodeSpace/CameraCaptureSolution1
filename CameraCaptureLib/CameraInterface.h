#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfapi.h> 

struct PreviewSession {
    IMFMediaSession* pSession = nullptr;
    IMFMediaSource* pSource = nullptr;
    IMFTopology* pTopology = nullptr;
    HWND hwndPreview = nullptr;
};
struct CameraFormat {
    UINT32 width;
    UINT32 height;
    UINT32 fpsNumerator;
    UINT32 fpsDenominator;
};

#ifdef __cplusplus
extern "C" {
#endif

    // Inicializaci�n y limpieza del sistema de c�maras
    __declspec(dllexport) void InitializeCameraSystem();
    __declspec(dllexport) void ShutdownCameraSystem();

    // Enumeraci�n de c�maras
    __declspec(dllexport) int GetCameraCount();
    __declspec(dllexport) void GetCameraName(int index, wchar_t* nameBuffer, int bufferLength);

    // Vista previa de c�mara
    __declspec(dllexport) bool StartPreview(int index, HWND hwnd, PreviewSession* session);
    __declspec(dllexport) void StopPreview(PreviewSession* session);

    // Enumeraci�n de micr�fonos
    __declspec(dllexport) int GetMicrophoneCount();
    __declspec(dllexport) void GetMicrophoneName(int index, wchar_t* nameBuffer, int bufferLength);

    // Control de grabaci�n
    __declspec(dllexport) bool StartRecording(int camIndex, int micIndex, const wchar_t* outputPath);
    __declspec(dllexport) bool PauseRecording();
    __declspec(dllexport) bool ResumeRecording();
    __declspec(dllexport) bool StopRecording();

    // Obtener formatos de camara
    __declspec(dllexport) bool GetSupportedFormats(int deviceIndex, CameraFormat** formats, int* count);
    __declspec(dllexport) void FreeFormats(CameraFormat* formats);

#ifdef __cplusplus
}
#endif
