#pragma once

#include <Windows.h>  // Asegura la definici�n de HWND

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
    __declspec(dllexport) bool StartPreview(int index, HWND hwnd);
    __declspec(dllexport) void StopPreview();

    // Enumeraci�n de micr�fonos
    __declspec(dllexport) int GetMicrophoneCount();
    __declspec(dllexport) void GetMicrophoneName(int index, wchar_t* nameBuffer, int bufferLength);

    // Control de grabaci�n
    __declspec(dllexport) bool StartRecording(int camIndex, int micIndex, const wchar_t* outputPath);
    __declspec(dllexport) bool PauseRecording();
    __declspec(dllexport) bool ResumeRecording();
    __declspec(dllexport) bool StopRecording();

#ifdef __cplusplus
}
#endif
