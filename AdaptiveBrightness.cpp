#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <openvr.h>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <fstream>
#include <string>
#include <tuple>
#include <algorithm>
#include <windows.h>
#include <shellapi.h>

#define TRAY_ICON_ID 1

// Global variable to store the handle to the console window
HWND g_hConsoleWindow = nullptr;

#pragma region Methods for Initialization
// Function to load a compiled shader from file
ID3DBlob* LoadCompiledShader(const std::wstring& filename) {
    std::ifstream shaderFile(filename, std::ios::binary);
    if (!shaderFile.good()) {
        std::cerr << "Failed to load shader file: " << std::string(filename.begin(), filename.end()) << std::endl;
        return nullptr;
    }

    shaderFile.seekg(0, std::ios::end);
    size_t size = shaderFile.tellg();
    shaderFile.seekg(0, std::ios::beg);

    std::vector<char> byteCode(size);
    shaderFile.read(byteCode.data(), size);

    ID3DBlob* shaderBlob = nullptr;
    HRESULT hr = D3DCreateBlob(size, &shaderBlob);
    if (FAILED(hr)) {
        std::cerr << "Failed to create shader blob." << std::endl;
        return nullptr;
    }

    memcpy(shaderBlob->GetBufferPointer(), byteCode.data(), size);

    return shaderBlob;
}

// Function to create DirectX 11 device and context
std::tuple<ID3D11Device*, ID3D11DeviceContext*> CreateD3DDeviceAndContext() {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0,
        D3D11_SDK_VERSION, &device, &featureLevel, &context
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create DirectX 11 device." << std::endl;
        return std::make_tuple(nullptr, nullptr);
    }

    return std::make_tuple(device, context);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_USER + 1: // Tray icon event
        if (LOWORD(lParam) == WM_LBUTTONDOWN) { // Check if left button was clicked
            // Should show the console window again, but it doesn't :(
            ShowWindow(g_hConsoleWindow, SW_SHOW);
            SetForegroundWindow(g_hConsoleWindow);
        }
        break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
#pragma endregion

#pragma region Methods for Brightness
void AdjustScreenBrightness(float analogGain) {
    // Define the section and key for the brightness setting
    const char* k_pch_SteamVR_Section = "steamvr";
    const char* k_pch_SteamVR_Brightness_Key = "analogGain";

    // Clamp the brightness between minValue and maxValue of the supported headset display brightness
    float clampedAnalogGain = std::clamp(analogGain, 0.028f, 1.6f);

    // Set the brightness value
    vr::VRSettings()->SetFloat(k_pch_SteamVR_Section, k_pch_SteamVR_Brightness_Key, clampedAnalogGain);
}

float percentToAlphaGain(float percent) {
    //float alphaGain = 0.8 * pow(2 * percent, 3) + 2 * pow(2 * percent, 2) + 0.045 * percent - 0.56;
    float alphaGain = 0.1 * pow(2 * percent, 3) + 0.3 * pow(2 * percent, 2) + 0.2 * percent;
    return alphaGain;
}

float CalculateTargetBrightness(unsigned int combinedBrightness, int textureWidth, int textureHeight) {
    
    int pixels = textureWidth * textureHeight;
    float averageBrightness = combinedBrightness * 10 / pixels / 1000.0f;

    float analogGain = percentToAlphaGain(averageBrightness * 2);

    return analogGain;
}
#pragma endregion

int main() {

    #pragma region Initialization

    // Init OpenVR
    vr::EVRInitError vr_error;
    vr::IVRSystem* vr_system = vr::VR_Init(&vr_error, vr::VRApplication_Background);

    if (!vr_system)
    {
        printf("Failed to load OpenVR: %s", vr::VR_GetVRInitErrorAsEnglishDescription(vr_error));
        return -1;
    }

    // Register application so it will be launched automagically next time
    if (!vr::VRApplications()->IsApplicationInstalled("virus.adaptiveBrightness"))
    {
        DWORD length = GetCurrentDirectory(0, NULL);
        wchar_t* path = new wchar_t[length];
        length = GetCurrentDirectoryW(length, path);

        if (length != 0)
        {
            std::wstring path_str(path);
            path_str.append(L"\\AdaptiveBrightness.vrmanifest");

            std::string path_str_utf8(path_str.begin(), path_str.end());

            vr::EVRApplicationError app_error;
            app_error = vr::VRApplications()->AddApplicationManifest(path_str_utf8.c_str());

            if (app_error == vr::VRApplicationError_None)
            {
                vr::VRApplications()->SetApplicationAutoLaunch("virus.adaptiveBrightness", true);
            }
            else
            {
                printf("Failed to add application manifest: %d\n", app_error);
            }
        }

        delete[] path;
    }

    vr::IVRCompositor* vrCompositor = vr::VRCompositor();

    // Create DirectX 11 device and context
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    std::tie(device, context) = CreateD3DDeviceAndContext();
    if (!device || !context) {
        std::cerr << "Failed to create DirectX 11 device and context." << std::endl;
        return -1;
    }

    // Load compiled compute shader
    ID3DBlob* computeShaderBlob = LoadCompiledShader(L"ImageBrightnessComputeShader.cso");
    if (!computeShaderBlob) {
        computeShaderBlob = LoadCompiledShader(L"x64\\Debug\\ImageBrightnessComputeShader.cso");
        if (!computeShaderBlob) {
            ID3DBlob* computeShaderBlob = LoadCompiledShader(L"x64\\Release\\ImageBrightnessComputeShader.cso");
            if (!computeShaderBlob) {
                std::cerr << "Failed to load compiled compute shader." << std::endl;
                device->Release();
                context->Release();
                return -2;
            }
        }
    }

    // Create compute shader
    ID3D11ComputeShader* computeShader = nullptr;
    device->CreateComputeShader(computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize(), nullptr, &computeShader);
    computeShaderBlob->Release();
    if (!computeShader) {
        std::cerr << "Failed to create compute shader." << std::endl;
        device->Release();
        context->Release();
        return -3;
    }

    // Create a structured buffer
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(unsigned int); // Change to uint size
    bufferDesc.ByteWidth = sizeof(unsigned int) * 1; // Ensure the buffer size is sufficient

    ID3D11Buffer* outputBuffer = nullptr;
    HRESULT hr = device->CreateBuffer(&bufferDesc, nullptr, &outputBuffer);
    if (FAILED(hr)) {
        std::cerr << "Failed to create buffer." << std::endl;
        device->Release();
        context->Release();
        return -4;
    }

    // Create UAV for the output buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 1; // Number of elements in the buffer

    ID3D11UnorderedAccessView* outputBufferUAV = nullptr;
    hr = device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputBufferUAV);
    if (FAILED(hr)) {
        std::cerr << "Failed to create UAV." << std::endl;
        outputBuffer->Release();
        device->Release();
        context->Release();
        return -5;
    }

    // Create DirectX 11 shader resource view
    ID3D11ShaderResourceView* renderTextureSRV = nullptr;

    // Obtain mirror texture from OpenVR
    vr::EVRCompositorError compositorError = vrCompositor->GetMirrorTextureD3D11(vr::Eye_Right, device, reinterpret_cast<void**>(&renderTextureSRV));
    if (compositorError != vr::VRCompositorError_None || !renderTextureSRV) {
        std::cerr << "Failed to obtain mirror texture from OpenVR. Error: " << compositorError << std::endl;
        outputBufferUAV->Release();
        outputBuffer->Release();
        computeShader->Release();
        device->Release();
        context->Release();
        return -6;
    }

    // Retrieve the ID3D11Resource from the SRV
    ID3D11Resource* renderTextureResource = nullptr;
    renderTextureSRV->GetResource(&renderTextureResource);
    if (!renderTextureResource) {
        std::cerr << "Failed to obtain resource from shader resource view." << std::endl;
        renderTextureSRV->Release();
        outputBufferUAV->Release();
        outputBuffer->Release();
        computeShader->Release();
        device->Release();
        context->Release();
        return -7;
    }

    ID3D11Texture2D* tex2D = nullptr;
    hr = renderTextureResource->QueryInterface<ID3D11Texture2D>(&tex2D);
    if (FAILED(hr)) {
        std::cerr << "Failed to query texture interface." << std::endl;
        return -11;
    }

    D3D11_TEXTURE2D_DESC desc;
    tex2D->GetDesc(&desc);

    // Figure out how many groups to dispatch.
    int textureWidth = desc.Width;
    int textureHeight = desc.Height;

    // Match these values in the shader
    int threadsPerGroupX = 16; // Make sure this value mathes the value from the shader
    int threadsPerGroupY = 16;
    int downSample = 4; // Since we take the average anyway we don't need to be super detailed
    int cropX = 1; // We only want to sample the center so we only need half. 2 and
    int cropY = 1; // We only want to sample the center so we only need half. 3 seam like good

    int downScaledCroppedTextureWidth = textureWidth / cropX / downSample;
    int downScaledCroppedTextureHeight = textureHeight / cropY / downSample;

    int groupCountX = (downScaledCroppedTextureWidth + threadsPerGroupX - 1) / threadsPerGroupX;
    int groupCountY = (downScaledCroppedTextureHeight + threadsPerGroupY - 1) / threadsPerGroupY;

    // Release the interface
    tex2D->Release();

    // More shader stuff
    context->CSSetShader(computeShader, nullptr, 0);
    context->CSSetShaderResources(0, 1, &renderTextureSRV);
    context->CSSetUnorderedAccessViews(0, 1, &outputBufferUAV, nullptr);

    // Minimize the console window
    g_hConsoleWindow = GetConsoleWindow();
    ShowWindow(g_hConsoleWindow, SW_HIDE); // Hide the console window

    // Create and show the system tray icon
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = g_hConsoleWindow;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_USER + 1;  // Custom message for tray icon events
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"Adaptive Brightness"); // Tooltip text

    // Add the icon to the system tray
    Shell_NotifyIcon(NIM_ADD, &nid);

    // Optionally, remove the console window from the taskbar
    LONG_PTR style = GetWindowLongPtr(g_hConsoleWindow, GWL_EXSTYLE);
    style &= ~WS_EX_APPWINDOW;
    style |= WS_EX_TOOLWINDOW;
    SetWindowLongPtr(g_hConsoleWindow, GWL_EXSTYLE, style);

    // Define target loop frequency (100 Hz) // Get headset hz instead
    const double targetFrequency = 30.0;
    const std::chrono::milliseconds sleepDuration(static_cast<int>(1000.0 / targetFrequency));
    #pragma endregion

    
    while (true) {

        // Capture the time at the start of the loop to adjust the sleep time of this thread
        auto loopStartTime = std::chrono::high_resolution_clock::now();

        #pragma region GetVRBrightness
        // Clear the output buffer
        UINT clearValue = 0;
        context->ClearUnorderedAccessViewUint(outputBufferUAV, &clearValue);
        context->CSSetUnorderedAccessViews(0, 1, &outputBufferUAV, nullptr);
        // Run the compute shader
        context->Dispatch(groupCountX, groupCountY, 1);

        // Ensure the GPU finishes processing the compute shader using Flush and Query
        ID3D11Query* pQuery = nullptr;
        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;
        queryDesc.MiscFlags = 0;

        HRESULT hr = device->CreateQuery(&queryDesc, &pQuery);
        if (FAILED(hr)) {
            std::cerr << "Failed to create query." << std::endl;
            renderTextureResource->Release();
            renderTextureSRV->Release();
            outputBufferUAV->Release();
            outputBuffer->Release();
            computeShader->Release();
            device->Release();
            context->Release();
            return -8;
        }

        // Issue the query
        context->Flush();
        context->End(pQuery);

        // Wait for the GPU to complete the query
        while (context->GetData(pQuery, NULL, 0, 0) == S_FALSE) {
            // Busy-wait and optionally add a small sleep
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        // Clean up the query object
        pQuery->Release();

        // Map the output buffer
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT mapResult = context->Map(outputBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
        if (FAILED(mapResult)) {
            std::cerr << "Failed to map output buffer. HRESULT: " << mapResult << std::endl;
            outputBufferUAV->Release();
            outputBuffer->Release();
            computeShader->Release();
            device->Release();
            context->Release();
            return -9;
        }

        // Retrieve the results from the output buffer
        unsigned int combinedBrightness = 0;
        if (mappedResource.pData) {
            unsigned int* pBuffer = reinterpret_cast<unsigned int*>(mappedResource.pData);
            combinedBrightness = pBuffer[0];
            context->Unmap(outputBuffer, 0);
        }
        else {
            std::cerr << "Failed to access mapped resource." << std::endl;
            outputBufferUAV->Release();
            outputBuffer->Release();
            computeShader->Release();
            device->Release();
            context->Release();
            return -10;
        }

        std::cout << "Combined brightness: " << combinedBrightness << std::endl;
        #pragma endregion

        #pragma region Converting and Setting Brightness
        float targetAnalogGain = CalculateTargetBrightness(combinedBrightness, downScaledCroppedTextureWidth, downScaledCroppedTextureHeight);
        std::cout << "Target analog gain: " << targetAnalogGain << std::endl;
        AdjustScreenBrightness(targetAnalogGain);
        #pragma endregion
        
        auto loopEndTime = std::chrono::high_resolution_clock::now();
        auto loopDuration = std::chrono::duration_cast<std::chrono::microseconds>(loopEndTime - loopStartTime);

        // Adjust sleep duration based on the time it took to complete the loop
        auto adjustedSleepDuration = sleepDuration - loopDuration;
        if (adjustedSleepDuration > std::chrono::microseconds(0)) {
            std::this_thread::sleep_for(adjustedSleepDuration);
        } // Only sleep if it was fast enough.
    }

    #pragma region CleanUp
    outputBufferUAV->Release();
    renderTextureSRV->Release();
    outputBuffer->Release();
    computeShader->Release();
    device->Release();
    context->Release();
    vr::VR_Shutdown();
    #pragma endregion

    return 0;
}