/*
 * Copyright (C) 2013-2021 libhybris
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ws.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <sys/auxv.h>
#include "logging.h"
#include "logging.h"

static struct ws_module *ws = NULL;

static void _init_ws()
{
    if (ws == NULL) {
        char ws_name[2048];
        char *vulkan_platform;

        vulkan_platform=getenv("HYBRIS_VULKANPLATFORM");

        if (vulkan_platform == NULL)
            vulkan_platform = "wayland";

        const char *vulkanplatform_dir = PKGLIBDIR;
        const char *user_vulkanplatform_dir = getauxval(AT_SECURE)
                                           ? NULL
                                           : getenv("HYBRIS_VULKANPLATFORM_DIR");
        if (user_vulkanplatform_dir)
            vulkanplatform_dir = user_vulkanplatform_dir;

        snprintf(ws_name, 2048, "%s/vulkanplatform_%s.so", vulkanplatform_dir, vulkan_platform);

        void *wsmod = (void *) dlopen(ws_name, RTLD_LAZY);
        if (wsmod==NULL) {
            fprintf(stderr, "ERROR: %s\n\t%s\n", ws_name, dlerror());
            assert(0);
        }
        ws = dlsym(wsmod, "ws_module_info");
        assert(ws != NULL);
        ws->init_module(&hybris_vulkan_interface);
    }
}

VkResult ws_vkEnumerateInstanceExtensionProperties(const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
{
    _init_ws();
    return ws->vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties);
}

VkResult ws_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
    _init_ws();
    return ws->vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

PFN_vkVoidFunction ws_vkGetDeviceProcAddr(VkDevice device, const char *procname)
{
    _init_ws();
    return ws->vkGetDeviceProcAddr(device, procname);
}

PFN_vkVoidFunction ws_vkGetInstanceProcAddr(VkInstance instance, const char *procname)
{
    _init_ws();
    return ws->vkGetInstanceProcAddr(instance, procname);
}

void ws_vkSetInstanceProcAddrFunc(PFN_vkVoidFunction addr)
{
    _init_ws();
    return ws->vkSetInstanceProcAddrFunc(addr);
}

// vim:ts=4:sw=4:noexpandtab
