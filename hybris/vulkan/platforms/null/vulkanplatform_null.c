/****************************************************************************************
 **
 ** Copyright (C) 2021 Jolla Ltd.
 ** All rights reserved.
 **
 ** This file is part of Wayland enablement for libhybris
 **
 ** You may use this file under the terms of the GNU Lesser General
 ** Public License version 2.1 as published by the Free Software Foundation
 ** and appearing in the file license.lgpl included in the packaging
 ** of this file.
 **
 ** This library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public
 ** License version 2.1 as published by the Free Software Foundation
 ** and appearing in the file license.lgpl included in the packaging
 ** of this file.
 **
 ** This library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ****************************************************************************************/

#include <android-config.h>
#include <ws.h>

#include <vulkanplatformcommon.h>
#include <vulkanhybris.h>

#include <vulkan/vulkan.h>

#include <hybris/common/binding.h>

static VkResult (*_vkEnumerateInstanceExtensionProperties)(const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties) = NULL;
static VkResult (*_vkCreateInstance)(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) = NULL;
static PFN_vkVoidFunction (*_vkGetInstanceProcAddr)(VkInstance instance, const char *pName) = NULL;

static void nullws_init_module(struct ws_vulkan_interface *vulkan_iface)
{
    vulkanplatformcommon_init(vulkan_iface);
}

static VkResult nullws_vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    if (_vkEnumerateInstanceExtensionProperties == NULL) {
        _vkEnumerateInstanceExtensionProperties = (VkResult (*)(const char*, uint32_t*, VkExtensionProperties*))
            (*_vkGetInstanceProcAddr)(NULL, "vkEnumerateInstanceExtensionProperties");
    }
    return (*_vkEnumerateInstanceExtensionProperties)(pLayerName, pPropertyCount, pProperties);
}

VkResult nullws_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
    if (_vkCreateInstance == NULL) {
        _vkCreateInstance = (VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *))
            (*_vkGetInstanceProcAddr)(NULL, "vkCreateInstance");
    }
    return (*_vkCreateInstance)(pCreateInfo, pAllocator, pInstance);
}

#ifdef WANT_WAYLAND
static VkResult nullws_vkCreateWaylandSurfaceKHR(VkInstance instance,
        const VkWaylandSurfaceCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkSurfaceKHR* pSurface)
{
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

static VkBool32 nullws_vkGetPhysicalDeviceWaylandPresentationSupportKHR(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, struct wl_display* display)
{
    return VK_FALSE;
}

static void nullws_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
{
}
#endif

static void nullws_vkSetInstanceProcAddrFunc(PFN_vkVoidFunction addr)
{
    if (_vkGetInstanceProcAddr == NULL)
        _vkGetInstanceProcAddr = (PFN_vkVoidFunction (*)(VkInstance, const char*))addr;
}

struct ws_module ws_module_info = {
    nullws_init_module,
    nullws_vkEnumerateInstanceExtensionProperties,
    nullws_vkCreateInstance,
#ifdef WANT_WAYLAND
    nullws_vkCreateWaylandSurfaceKHR,
    nullws_vkGetPhysicalDeviceWaylandPresentationSupportKHR,
    nullws_vkDestroySurfaceKHR,
#endif
    nullws_vkSetInstanceProcAddrFunc,
};
