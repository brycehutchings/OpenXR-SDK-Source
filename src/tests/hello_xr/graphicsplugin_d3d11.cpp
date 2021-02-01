// Copyright (c) 2017-2020 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "pch.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"

#if defined(XR_USE_GRAPHICS_API_D3D11)
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Graphics/GraphicsEngineD3D11/interface/RenderDeviceD3D11.h"
#include "Graphics/GraphicsEngineD3DBase/include/DXGITypeConversions.hpp"
#endif

#if defined(XR_USE_GRAPHICS_API_D3D12)
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h"
#include "Graphics/GraphicsEngineD3D12/interface/DeviceContextD3D12.h"
#include "Graphics/GraphicsEngineD3DBase/include/DXGITypeConversions.hpp"
#endif

#if defined(XR_USE_GRAPHICS_API_VULKAN)
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Graphics/GraphicsEngineVulkan/interface/RenderDeviceVk.h"
#include "Graphics/GraphicsEngineVulkan/interface/DeviceContextVk.h"
#include "Graphics/GraphicsEngineVulkan/include/VulkanTypeConversions.hpp"
#endif

#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"

#include "options.h"
#include "graphicsplugin.h"

using namespace Diligent;

#if defined(XR_USE_GRAPHICS_API_D3D11)
namespace {
struct D3D11GraphicsInterop : IGraphicsInterop {
    D3D11GraphicsInterop(RefCntAutoPtr<IRenderDeviceD3D11> device, RefCntAutoPtr<IDeviceContext> immediateContext)
        : m_device(device), m_immediateContext(immediateContext) {
        m_d3d11GraphicsBinding.device = m_device->GetD3D11Device();
        m_supportedColorFormats = {
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        };
    }

    IRenderDevice* RenderDevice() override { return m_device; }
    IDeviceContext* ImmediateContext() override { return m_immediateContext; }

    std::vector<int64_t> SupportedColorFormats() const override { return m_supportedColorFormats; }

    const XrBaseInStructure* GetSessionGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_d3d11GraphicsBinding);
    }

    TEXTURE_FORMAT GetTextureFormat(uint64_t nativeFormat) const override {
        return DXGI_FormatToTexFormat((DXGI_FORMAT)nativeFormat);
    }

    std::vector<RefCntAutoPtr<ITexture>> GetSwapchainTextures(XrSwapchain swapchain, const XrSwapchainCreateInfo&) override {
        std::vector<XrSwapchainImageD3D11KHR> swapchainImages =
            GetSwapchainImages<XrSwapchainImageD3D11KHR, XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR>(swapchain);

        std::vector<RefCntAutoPtr<ITexture>> swapchainTextures;
        for (const XrSwapchainImageD3D11KHR& swapchainImage : swapchainImages) {
            RefCntAutoPtr<ITexture> swapchainTexture;
            m_device.RawPtr<IRenderDeviceD3D11>()->CreateTexture2DFromD3DResource(swapchainImage.texture, RESOURCE_STATE_UNDEFINED,
                                                                                  &swapchainTexture);
            swapchainTextures.emplace_back(swapchainTexture);
        }

        return swapchainTextures;
    }

   private:
    RefCntAutoPtr<IRenderDeviceD3D11> m_device;
    RefCntAutoPtr<IDeviceContext> m_immediateContext;

    XrGraphicsBindingD3D11KHR m_d3d11GraphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    std::vector<int64_t> m_supportedColorFormats;
};
}  // namespace

std::unique_ptr<IGraphicsInterop> TryCreateD3D11Interop(XrInstance instance, XrSystemId systemId) {
    PFN_xrGetD3D11GraphicsRequirementsKHR pfnGetD3D11GraphicsRequirementsKHR = nullptr;
    HXR_CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetD3D11GraphicsRequirementsKHR)));

    XrGraphicsRequirementsD3D11KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    HXR_CHECK_XRCMD(pfnGetD3D11GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));

#if ENGINE_DLL
    GetEngineFactoryD3D11Type GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#endif

    EngineD3D11CreateInfo EngineCI;
    // TODO: EngineCI.AdapterId = graphicsRequirements.adapterLuid;
    // TODO: EngineCI.MinimumFeatureLevel = graphicsRequirements.minFeatureLevel;

#if !defined(NDEBUG)
    EngineCI.DebugFlags |= D3D11_DEBUG_FLAG_CREATE_DEBUG_DEVICE | D3D11_DEBUG_FLAG_VERIFY_COMMITTED_SHADER_RESOURCES;
#endif

    IEngineFactoryD3D11* factoryD3D11 = GetEngineFactoryD3D11();
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> immediateContext;
    factoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &device, &immediateContext);
    if (!device || !immediateContext) {
        return nullptr;
    }

    return std::make_unique<D3D11GraphicsInterop>(device.Cast<IRenderDeviceD3D11>(IID_RenderDeviceD3D11), immediateContext);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(XR_USE_GRAPHICS_API_D3D12)
namespace {
struct D3D12GraphicsInterop : IGraphicsInterop {
    D3D12GraphicsInterop(RefCntAutoPtr<IRenderDeviceD3D12> device, RefCntAutoPtr<IDeviceContextD3D12> immediateContext)
        : m_device(device), m_immediateContext(immediateContext) {
        m_d3D12GraphicsBinding.device = m_device->GetD3D12Device();

        // Diligent requires the caller to lock the queue mutex to get access to the command queue, but we ensure there are no
        // simultaneous access, so we can cache it and unlock.
        m_d3D12GraphicsBinding.queue = m_immediateContext->LockCommandQueue()->GetD3D12CommandQueue();
        m_immediateContext->UnlockCommandQueue();

        m_supportedColorFormats = {
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        };
    }

    IRenderDevice* RenderDevice() override { return m_device; }
    IDeviceContext* ImmediateContext() override { return m_immediateContext; }

    std::vector<int64_t> SupportedColorFormats() const override { return m_supportedColorFormats; }

    const XrBaseInStructure* GetSessionGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_d3D12GraphicsBinding);
    }

    TEXTURE_FORMAT GetTextureFormat(uint64_t nativeFormat) const override {
        return DXGI_FormatToTexFormat((DXGI_FORMAT)nativeFormat);
    }

    std::vector<RefCntAutoPtr<ITexture>> GetSwapchainTextures(XrSwapchain swapchain, const XrSwapchainCreateInfo&) override {
        std::vector<XrSwapchainImageD3D12KHR> swapchainImages =
            GetSwapchainImages<XrSwapchainImageD3D12KHR, XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR>(swapchain);

        std::vector<RefCntAutoPtr<ITexture>> swapchainTextures;
        for (const XrSwapchainImageD3D12KHR& swapchainImage : swapchainImages) {
            RefCntAutoPtr<ITexture> swapchainTexture;
            m_device.RawPtr<IRenderDeviceD3D12>()->CreateTextureFromD3DResource(swapchainImage.texture, RESOURCE_STATE_UNDEFINED,
                                                                                &swapchainTexture);
            swapchainTextures.emplace_back(swapchainTexture);
        }

        return swapchainTextures;
    }

   private:
    RefCntAutoPtr<IRenderDeviceD3D12> m_device;
    RefCntAutoPtr<IDeviceContextD3D12> m_immediateContext;

    XrGraphicsBindingD3D12KHR m_d3D12GraphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D12_KHR};
    std::vector<int64_t> m_supportedColorFormats;
};
}  // namespace

std::unique_ptr<IGraphicsInterop> TryCreateD3D12Interop(XrInstance instance, XrSystemId systemId) {
    PFN_xrGetD3D12GraphicsRequirementsKHR pfnGetD3D12GraphicsRequirementsKHR = nullptr;
    HXR_CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetD3D12GraphicsRequirementsKHR",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetD3D12GraphicsRequirementsKHR)));

    XrGraphicsRequirementsD3D12KHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR};
    HXR_CHECK_XRCMD(pfnGetD3D12GraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));

#if ENGINE_DLL
    GetEngineFactoryD3D12Type GetEngineFactoryD3D12 = LoadGraphicsEngineD3D12();
#endif

    EngineD3D12CreateInfo EngineCI;
    // TODO: EngineCI.AdapterId = graphicsRequirements.adapterLuid;
    // TODO: EngineCI.MinimumFeatureLevel = graphicsRequirements.minFeatureLevel;
#if !defined(NDEBUG)
    EngineCI.EnableDebugLayer = true;
#endif

    IEngineFactoryD3D12* factoryD3D12 = GetEngineFactoryD3D12();
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> immediateContext;
    factoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &device, &immediateContext);
    if (!device || !immediateContext) {
        return nullptr;
    }

    return std::make_unique<D3D12GraphicsInterop>(device.Cast<IRenderDeviceD3D12>(IID_RenderDeviceD3D12),
                                                  immediateContext.Cast<IDeviceContextD3D12>(IID_DeviceContextD3D12));
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0 // defined(XR_USE_GRAPHICS_API_VULKAN)
// 
namespace {
struct VulkanGraphicsInterop : IGraphicsInterop {
    VulkanGraphicsInterop(RefCntAutoPtr<IRenderDeviceVk> device, RefCntAutoPtr<IDeviceContextVk> immediateContext)
        : m_device(device), m_immediateContext(immediateContext) {
        m_vulkanGraphicsBinding.physicalDevice = m_device->GetVkPhysicalDevice();
        m_vulkanGraphicsBinding.device = m_device->GetVkDevice();
        m_vulkanGraphicsBinding.instance = m_device->GetVkInstance();

        // Diligent requires the caller to lock the queue mutex to get access to the command queue, but we ensure there are no
        // simultaneous access, so we can cache it and unlock.
        ICommandQueueVk* commandQueue = m_immediateContext->LockCommandQueue();
        m_vulkanGraphicsBinding.queueFamilyIndex = commandQueue->GetQueueFamilyIndex();

        // TODO: Need to find index of queue
        m_vulkanGraphicsBinding.queueIndex = 0;  // commandQueue->GetVkQueue();
        m_immediateContext->UnlockCommandQueue();

        m_supportedColorFormats = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM,
                                   VK_FORMAT_R8G8B8A8_UNORM};
    }

    IRenderDevice* RenderDevice() override { return m_device; }
    IDeviceContext* ImmediateContext() override { return m_immediateContext; }

    std::vector<int64_t> SupportedColorFormats() const override { return m_supportedColorFormats; }

    const XrBaseInStructure* GetSessionGraphicsBinding() const override {
        return reinterpret_cast<const XrBaseInStructure*>(&m_vulkanGraphicsBinding);
    }

    TEXTURE_FORMAT GetTextureFormat(uint64_t nativeFormat) const override { return VkFormatToTexFormat((VkFormat)nativeFormat); }

    std::vector<RefCntAutoPtr<ITexture>> GetSwapchainTextures(XrSwapchain swapchain,
                                                              const XrSwapchainCreateInfo& createInfo) override {
        std::vector<XrSwapchainImageVulkanKHR> swapchainImages =
            GetSwapchainImages<XrSwapchainImageVulkanKHR, XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR>(swapchain);

        std::vector<RefCntAutoPtr<ITexture>> swapchainTextures;
        for (const XrSwapchainImageVulkanKHR& swapchainImage : swapchainImages) {
            RefCntAutoPtr<ITexture> swapchainTexture;
            Diligent::TextureDesc imageDesc;
            imageDesc.Type = RESOURCE_DIM_TEX_2D;
            imageDesc.Format = GetTextureFormat(createInfo.format);
            imageDesc.Width = createInfo.width;
            imageDesc.Height = createInfo.height;
            imageDesc.ArraySize = createInfo.arraySize;
            imageDesc.MipLevels = createInfo.mipCount;
            imageDesc.SampleCount = createInfo.sampleCount;

            if (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_SAMPLED_BIT) {
                imageDesc.BindFlags |= BIND_SHADER_RESOURCE;
            }
            if (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) {
                imageDesc.BindFlags |= BIND_RENDER_TARGET;
            }
            if (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                imageDesc.BindFlags |= BIND_DEPTH_STENCIL;
            }
            if (createInfo.usageFlags & XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT) {
                imageDesc.BindFlags |= BIND_UNORDERED_ACCESS;
            }

            m_device.RawPtr<IRenderDeviceVk>()->CreateTextureFromVulkanImage(swapchainImage.image, imageDesc,
                                                                             RESOURCE_STATE_UNDEFINED, &swapchainTexture);
            swapchainTextures.emplace_back(swapchainTexture);
        }

        return swapchainTextures;
    }

   private:
    RefCntAutoPtr<IRenderDeviceVk> m_device;
    RefCntAutoPtr<IDeviceContextVk> m_immediateContext;

    XrGraphicsBindingVulkanKHR m_vulkanGraphicsBinding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    std::vector<int64_t> m_supportedColorFormats;
};
}  // namespace

std::unique_ptr<IGraphicsInterop> TryCreateVulkanInterop(XrInstance /*instance*/, XrSystemId /*systemId*/) {
    // PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = nullptr;
    // HXR_CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirementsKHR",
    //                                       reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetVulkanGraphicsRequirementsKHR)));
    // XrGraphicsRequirementsVulkanKHR graphicsRequirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    // HXR_CHECK_XRCMD(pfnGetVulkanGraphicsRequirementsKHR(instance, systemId, &graphicsRequirements));

#if ENGINE_DLL
    GetEngineFactoryVkType GetEngineFactoryVulkan = LoadGraphicsEngineVk();
#endif

    EngineVkCreateInfo EngineCI;
    // TODO: EngineCI.AdapterId = graphicsRequirements.adapterLuid;
#if !defined(NDEBUG)
    EngineCI.EnableValidation = true;
#endif

    IEngineFactoryVk* factoryVulkan = GetEngineFactoryVulkan();
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> immediateContext;
    /* TODO: Must use AttachToVulkanDevice(std::shared_ptr<VulkanUtilities::VulkanInstance> Instance,
                                                         std::unique_ptr<VulkanUtilities::VulkanPhysicalDevice> PhysicalDevice,
                                                         std::shared_ptr<VulkanUtilities::VulkanLogicalDevice> LogicalDevice,
                                                         size_t CommandQueueCount, ICommandQueueVk * *ppCommandQueues,
                                                         const EngineVkCreateInfo& EngineCI, IRenderDevice** ppDevice,
                                                         IDeviceContext** ppContexts);  // override final;
                                                         */
    factoryVulkan->CreateDeviceAndContextsVk(EngineCI, &device, &immediateContext);
    // factoryVulkan->Attach
    if (!device || !immediateContext) {
        return nullptr;
    }

    return std::make_unique<VulkanGraphicsInterop>(device.Cast<IRenderDeviceVk>(IID_RenderDeviceVk),
                                                   immediateContext.Cast<IDeviceContextVk>(IID_DeviceContextVk));
}
#endif
