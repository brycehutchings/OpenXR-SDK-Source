// Copyright (c) 2017-2020 The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(XR_USE_PLATFORM_WIN32)
#define PLATFORM_WIN32 1  // TODO: Handle UWP (PLATFORM_UNIVERSAL_WINDOWS)
#elif defined(XR_USE_PLATFORM_ANDROID)
#define PLATFORM_ANDROID 1
#elif defined(XR_USE_PLATFORM_WIN32)
#define PLATFORM_LINUX 1
#endif

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Common/interface/RefCntAutoPtr.hpp"

template <typename TXrSwapchainImageStruct, XrStructureType SwapchainImageStructType>
std::vector<TXrSwapchainImageStruct> GetSwapchainImages(XrSwapchain swapchain) {
    uint32_t imageCount;
    HXR_CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
    std::vector<TXrSwapchainImageStruct> swapchainImages(imageCount, {SwapchainImageStructType});
    HXR_CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount,
                                               reinterpret_cast<XrSwapchainImageBaseHeader*>(&swapchainImages[0])));
    return swapchainImages;
}

struct IGraphicsInterop {
    virtual ~IGraphicsInterop() = default;

    virtual std::vector<int64_t> SupportedColorFormats() const = 0;
    virtual Diligent::TEXTURE_FORMAT GetTextureFormat(uint64_t nativeFormat) const = 0;

    virtual Diligent::IRenderDevice* RenderDevice() = 0;
    virtual Diligent::IDeviceContext* ImmediateContext() = 0;

    virtual const XrBaseInStructure* GetSessionGraphicsBinding() const = 0;

    virtual std::vector<Diligent::RefCntAutoPtr<Diligent::ITexture>> GetSwapchainTextures(
        XrSwapchain swapchain, const XrSwapchainCreateInfo& createInfo) = 0;
};

std::unique_ptr<IGraphicsInterop> TryCreateD3D11Interop(XrInstance instance, XrSystemId systemId);
std::unique_ptr<IGraphicsInterop> TryCreateD3D12Interop(XrInstance instance, XrSystemId systemId);
std::unique_ptr<IGraphicsInterop> TryCreateVulkanInterop(XrInstance instance, XrSystemId systemId);
