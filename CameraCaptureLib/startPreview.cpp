#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <mfplay.h>
#include <mferror.h>
#include <Mftransform.h>
#include <vector>
#include <iostream>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfplay.lib")

IMFMediaSource* g_pSource = nullptr;
IMFMediaSession* g_pSession = nullptr;
IMFTopology* g_pTopology = nullptr;

HRESULT InitMediaFoundationAndCreateSource(int deviceIndex = 0) {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return hr;

    IMFAttributes* pAttributes = nullptr;
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) return hr;

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->Release();

    if (FAILED(hr) || count == 0) return E_FAIL;

    // Seleccionamos el dispositivo según el índice
    hr = ppDevices[deviceIndex]->ActivateObject(__uuidof(IMFMediaSource), (void**)&g_pSource);

    for (UINT32 i = 0; i < count; i++) {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    return hr;
}

HRESULT BuildPreviewAndRecordingTopology(HWND hwndPreview, const WCHAR* outputPath) {
    HRESULT hr = S_OK;

    IMFPresentationDescriptor* pPD = nullptr;
    IMFStreamDescriptor* pSD = nullptr;
    IMFTopologyNode* pSourceNode = nullptr;
    IMFTopologyNode* pPreviewNode = nullptr;
    IMFTopologyNode* pRecordNode = nullptr;
    IMFActivate* pEVRActivate = nullptr;
    IMFByteStream* pByteStream = nullptr;
    IMFMediaSink* pSink = nullptr;
    IMFActivate* pSinkActivate = nullptr;
    BOOL fSelected = FALSE;

    // 1. Crear Presentation Descriptor
    hr = g_pSource->CreatePresentationDescriptor(&pPD);
    if (FAILED(hr)) goto done;

    // 2. Obtener el primer stream
    hr = pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD);
    if (FAILED(hr) || !fSelected) goto done;

    // 3. Crear la topología
    hr = MFCreateTopology(&g_pTopology);
    if (FAILED(hr)) goto done;

    // 4. Crear nodo fuente
    hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pSourceNode);
    if (FAILED(hr)) goto done;

    hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, g_pSource);
    hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
    hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
    hr = g_pTopology->AddNode(pSourceNode);
    if (FAILED(hr)) goto done;

    // 5. Nodo de preview (EVR)
    hr = MFCreateVideoRendererActivate(hwndPreview, &pEVRActivate);
    if (FAILED(hr)) goto done;

    hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pPreviewNode);
    hr = pPreviewNode->SetObject(pEVRActivate);
    hr = g_pTopology->AddNode(pPreviewNode);
    if (FAILED(hr)) goto done;

    // 6. Nodo de grabación (archivo .mp4)
    hr = MFCreateFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, MF_FILEFLAGS_NONE,
        outputPath, &pByteStream);
    if (FAILED(hr)) goto done;

    hr = MFCreateMPEG4MediaSink(pByteStream, nullptr, nullptr, &pSink);
    if (FAILED(hr)) goto done;

    hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pRecordNode);
    hr = pRecordNode->SetObject(pSink);
    hr = g_pTopology->AddNode(pRecordNode);
    if (FAILED(hr)) goto done;

    // 7. Conectar SourceNode con ambos nodos de salida
    hr = pSourceNode->ConnectOutput(0, pPreviewNode, 0);
    hr = pSourceNode->ConnectOutput(0, pRecordNode, 0);
    if (FAILED(hr)) goto done;

done:
    if (pPD) pPD->Release();
    if (pSD) pSD->Release();
    if (pEVRActivate) pEVRActivate->Release();
    if (pByteStream) pByteStream->Release();
    if (pSink) pSink->Release();
    if (pSourceNode) pSourceNode->Release();
    if (pPreviewNode) pPreviewNode->Release();
    if (pRecordNode) pRecordNode->Release();

    return hr;
}

HRESULT StartSessionAndRunTopology() {
    HRESULT hr = S_OK;

    PROPVARIANT varStart;
    PropVariantInit(&varStart); // Indica "start desde el principio"

    // 1. Crear sesión
    hr = MFCreateMediaSession(nullptr, &g_pSession);
    if (FAILED(hr)) return hr;

    // 2. Cargar la topología en la sesión
    hr = g_pSession->SetTopology(0, g_pTopology);
    if (FAILED(hr)) return hr;

    // 3. Iniciar reproducción y grabación
    hr = g_pSession->Start(&GUID_NULL, &varStart);

    PropVariantClear(&varStart);
    return hr;
}
