#pragma once


#ifdef gptDebug
    // debug layer
    #define gptEnableD3D12DebugLayer

    // GPU based validation
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/using-d3d12-debug-layer-gpu-based-validation
    // note: enabling this can cause problems. in our case, shader resources bound by global root sig become invisible.
    //#define gptEnableD3D12GBV

    // DREAD (this requires Windows SDK 10.0.18362.0 or newer)
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/use-dred
    //#define gptEnableD3D12DREAD

    //#define gptEnableBufferValidation
    //#define gptEnableRenderTargetValidation
    //#define gptForceSoftwareDevice
#endif // gptDebug

#define gptEnableResourceName
#define gptEnableTimestamp
#define gptEnableD3D12StablePowerState
