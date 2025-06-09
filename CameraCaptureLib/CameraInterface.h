#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfapi.h> 
#include <guiddef.h> // Necesario para GUID

struct PreviewSession {
    IMFMediaSession* pSession = nullptr;
    IMFMediaSource* pSource = nullptr;
    IMFTopology* pTopology = nullptr;
    HWND hwndPreview = nullptr;
};
#define MAX_SUBTYPE_NAME_LEN 32
struct CameraFormat {
    UINT32 width;
    UINT32 height;
    UINT32 fps_num;
    UINT32 fps_den;
    char subtype[MAX_SUBTYPE_NAME_LEN]; // �CAMBIO CLAVE: array de char de tama�o fijo!
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
    __declspec(dllexport) bool StartRecordingTwoCameras(int camIndex, int cam2Index, int micIndex, const wchar_t* outputPath);
    __declspec(dllexport) bool PauseRecording();
    __declspec(dllexport) bool ContinueRecording();
    __declspec(dllexport) bool StopRecording();

    // Obtener formatos de camara
    __declspec(dllexport) bool GetSupportedFormats(int deviceIndex, CameraFormat** formats, int* count);
    __declspec(dllexport) void FreeFormats(CameraFormat* formats);

    typedef void(__stdcall* ErrorCallbackFunc)(const wchar_t* message);
    __declspec(dllexport) void __stdcall SetErrorCallback(ErrorCallbackFunc callback);

    // Ejemplo de funci�n exportada
    __declspec(dllexport) int __stdcall GetCameraCount();

#ifdef __cplusplus
}
#endif
