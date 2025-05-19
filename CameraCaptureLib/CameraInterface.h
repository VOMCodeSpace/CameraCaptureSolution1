#pragma once

#include <Windows.h>  // Asegura la definición de HWND

#ifdef __cplusplus
extern "C" {
#endif

    // Inicialización y limpieza del sistema de cámaras
    __declspec(dllexport) void InitializeCameraSystem();
    __declspec(dllexport) void ShutdownCameraSystem();

    // Enumeración de cámaras
    __declspec(dllexport) int GetCameraCount();
    __declspec(dllexport) void GetCameraName(int index, wchar_t* nameBuffer, int bufferLength);

    // Vista previa de cámara
    __declspec(dllexport) bool StartPreview(int index, HWND hwnd);
    __declspec(dllexport) void StopPreview();

    // Enumeración de micrófonos
    __declspec(dllexport) int GetMicrophoneCount();
    __declspec(dllexport) void GetMicrophoneName(int index, wchar_t* nameBuffer, int bufferLength);

    // Control de grabación
    __declspec(dllexport) bool StartRecording(int camIndex, const wchar_t* outputPath);
    __declspec(dllexport) bool PauseRecording();
    __declspec(dllexport) bool ResumeRecording();
    __declspec(dllexport) bool StopRecording();

#ifdef __cplusplus
}
#endif
