/*
 * test_vulkan: Test Vulkan implementation
 * Copyright (c) 2021 Jolla Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include <wayland-client.h>
#include <wayland-server.h>
#include <wayland-client-protocol.h>

#define VK_USE_PLATFORM_WAYLAND_KHR 1
#include <vulkan/vulkan.h>

const int MAX_FRAMES_IN_FLIGHT = 2;

VkDevice device;
VkPhysicalDevice usedPhysicalDevice;
VkQueue graphicsQueue;
VkQueue presentQueue;
VkSwapchainKHR swapChain;
VkImage *swapChainImages = nullptr;
uint32_t imageCount;
VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;
//std::vector<VkImageView> swapChainImageViews;
//std::vector<VkFramebuffer> swapChainFramebuffers;

std::vector<VkSemaphore> imageAvailableSemaphores;
std::vector<VkSemaphore> renderFinishedSemaphores;
std::vector<VkFence> inFlightFences;
std::vector<VkFence> imagesInFlight;
int32_t graphicsFamily = -1;
int32_t presentFamily = -1;
VkSurfaceKHR surface;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct wl_display *wldisplay = NULL;
struct wl_compositor *wlcompositor = NULL;
struct wl_surface *wlsurface;
struct wl_region *wlregion;
struct wl_shell *wlshell;
struct wl_shell_surface *wlshell_surface;

VkCommandPool commandPool;
VkCommandBuffer *commandBuffers;

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

/*
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }
*/
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            printf("failed to create synchronization objects for a frame!");
            exit(1);
        }
    }
}

static void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    printf("Got a registry event for %s id %u\n", interface, id);
    if (strcmp(interface, "wl_compositor") == 0) {
        wlcompositor = (wl_compositor *)wl_registry_bind(registry,
                        id,
                        &wl_compositor_interface,
                        1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        wlshell = (wl_shell *)wl_registry_bind(registry, id,
                    &wl_shell_interface, 1);
    }
}

static void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id)
{
    printf("Got a registry losing event for %u\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

static void get_server_references(void)
{
    wldisplay = wl_display_connect(NULL);
    if (wldisplay == NULL) {
        fprintf(stderr, "Can't connect to display\n");
        exit(1);
    }
    printf("connected to display\n");

    struct wl_registry *registry = wl_display_get_registry(wldisplay);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    wl_display_dispatch(wldisplay);
    wl_display_roundtrip(wldisplay);

    if (wlcompositor == NULL || wlshell == NULL) {
        fprintf(stderr, "Can't find compositor or shell\n");
        exit(1);
    } else {
        fprintf(stderr, "Found compositor and shell\n");
    }
}

VkResult vkGetBestGraphicsQueue(VkPhysicalDevice physicalDevice) {
    VkResult ret = VK_ERROR_INITIALIZATION_FAILED;
    uint32_t queueFamilyPropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, 0);

    VkQueueFamilyProperties* const queueFamilyProperties = (VkQueueFamilyProperties*)malloc(
    sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);
    printf("queueFamilyPropertiesCount %i\n", queueFamilyPropertiesCount);

    uint32_t i;
    for (i = 0; i < queueFamilyPropertiesCount; i++) {
        printf("queueFamilyProperties %i\n", i);
        if (VK_QUEUE_GRAPHICS_BIT & queueFamilyProperties[i].queueFlags) {
            printf("found graphics bit\n");
            graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if (presentSupport) {
            printf("found present support\n");
            presentFamily = i;
        }

        if (graphicsFamily != -1 && presentFamily != -1) {
            ret = VK_SUCCESS;
            break;
        }
    }

    return ret;
}

#define check_result(result) \
    if (VK_SUCCESS != (result)) { fprintf(stderr, "Failure at %u %s with result %i\n", __LINE__, __FILE__, result); exit(-1); }


static void recordPipelineImageBarrier(VkCommandBuffer commandBuffer,
                                       VkAccessFlags sourceAccessMask,
                                       VkAccessFlags destAccessMask,
                                       VkImageLayout sourceLayout,
                                       VkImageLayout destLayout,
                                       VkImage image)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = sourceAccessMask;
    barrier.dstAccessMask = destAccessMask;
    barrier.oldLayout = sourceLayout;
    barrier.newLayout = destLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier);
}

static void rerecordCommandBuffer(uint32_t frameIndex, const VkClearColorValue *clearColor)
{
    VkCommandBuffer commandBuffer = commandBuffers[frameIndex];
    VkImage image = swapChainImages[frameIndex];
    VkCommandBufferBeginInfo beginInfo = {};
    VkImageSubresourceRange clearRange = {};
    printf("rerecordCommandBuffer 1 %p\n", commandBuffer);

    check_result(vkResetCommandBuffer(commandBuffer, 0));

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    check_result(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    printf("rerecordCommandBuffer 2\n");

    recordPipelineImageBarrier(commandBuffer,
                               0,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               image);
    printf("rerecordCommandBuffer 3\n");
    clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = 1;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = 1;
    vkCmdClearColorImage(
        commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, clearColor, 1, &clearRange);
    printf("rerecordCommandBuffer 4\n");
    recordPipelineImageBarrier(commandBuffer,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_MEMORY_READ_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                               image);
    printf("rerecordCommandBuffer 5\n");
    check_result(vkEndCommandBuffer(commandBuffer));
    printf("rerecordCommandBuffer 6\n");
}

bool createSwapchain()
{
//    if () {
//        ;
//    }
    /*
     * Select mode and format
     */
    printf("Create swap chain!\n");
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(usedPhysicalDevice);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);

    imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    printf("Create swap chain! imageCount %u\n", imageCount);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = (VkExtent2D){0xFFFFFFFF, 0xFFFFFFFF};
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};

    if (graphicsFamily != presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        printf("failed to create swap chain!\n");
        return false;
    }

    check_result(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr));
    printf("imageCount %u\n", imageCount);

    swapChainImages = (VkImage *)malloc(sizeof(VkImage) * imageCount);
//    swapChainImages.resize(imageCount);
    check_result(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages));
    printf("imageCount %u\n", imageCount);

    printf("Create swap chain! imageCount real %u\n", imageCount);
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = (VkExtent2D){0xFFFFFFFF, 0xFFFFFFFF};

    /*
     * Create command pool
     */
    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolCreateInfo.queueFamilyIndex = graphicsFamily;
    check_result(vkCreateCommandPool(device, &poolCreateInfo, NULL, &commandPool));

    /*
     * Create command buffers
     */
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = imageCount;
    commandBuffers = (VkCommandBuffer *)malloc(sizeof(VkCommandBuffer) * imageCount);
    check_result(vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers));
        printf("commandBuffers %p\n", commandBuffers);

    return true;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Wayland Setup */
    get_server_references();

    /*
     * Create instance
     */
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "test_vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t inst_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_count, NULL);

    // Enumerate the instance extensions
    VkExtensionProperties* inst_exts =
        (VkExtensionProperties *)malloc(inst_ext_count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &inst_ext_count, inst_exts);

    char **enabledExtensions = (char **)malloc(VK_MAX_EXTENSION_NAME_SIZE * inst_ext_count * sizeof(char));
    enabledExtensions = (char **)malloc(inst_ext_count * sizeof(char *));

    for (uint32_t i = 0; i < inst_ext_count; i++) {
        enabledExtensions[i] = (char *)malloc(VK_MAX_EXTENSION_NAME_SIZE * sizeof(char));
        strncpy(enabledExtensions[i], inst_exts[i].extensionName, VK_MAX_EXTENSION_NAME_SIZE);
    }

    printf("Found %i instance extensions\n", inst_ext_count);

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    instanceCreateInfo.enabledExtensionCount = inst_ext_count;
    instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.pNext = NULL;

    VkInstance instance;
    check_result(vkCreateInstance(&instanceCreateInfo, 0, &instance));

    /*
     * Find physical device 
     */
    uint32_t physicalDeviceCount = 0;
    check_result(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0));

    printf("Found %i physical devices\n", physicalDeviceCount);

    VkPhysicalDevice* const physicalDevices = (VkPhysicalDevice*)malloc(
        sizeof(VkPhysicalDevice) * physicalDeviceCount);

    check_result(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    uint32_t i = 0;
    if (physicalDeviceCount > 0) {
        /*
         * Prinf device information
         */
        printf("Device %i:\n", i);
        usedPhysicalDevice = physicalDevices[0];

        VkPhysicalDeviceProperties deviceProperties = {};

        vkGetPhysicalDeviceProperties(usedPhysicalDevice, &deviceProperties);
        printf("Device name: %s\n", deviceProperties.deviceName);
        printf("Device type: %u\n", deviceProperties.deviceType);
        printf("API version: %u.%u.%u\n", VK_VERSION_MAJOR(deviceProperties.apiVersion)
                                        , VK_VERSION_MINOR(deviceProperties.apiVersion)
                                        , VK_VERSION_PATCH(deviceProperties.apiVersion));
        printf("Driver version: %u.%u.%u\n", VK_VERSION_MAJOR(deviceProperties.driverVersion)
                                           , VK_VERSION_MINOR(deviceProperties.driverVersion)
                                           , VK_VERSION_PATCH(deviceProperties.driverVersion));
        printf("Device ID %u\n", deviceProperties.deviceID);
        printf("Vendor ID %u\n", deviceProperties.vendorID);

        uint32_t deviceExtensionCount = 0;
        VkResult result =
            vkEnumerateDeviceExtensionProperties(usedPhysicalDevice, NULL, &deviceExtensionCount, NULL);
        printf("deviceExtensionCount: %i\n", deviceExtensionCount);

        /*
         * Create surface
         */
        VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo = {};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.pNext = NULL;
        surfaceCreateInfo.flags = 0;
        surfaceCreateInfo.display = wldisplay;
        surfaceCreateInfo.surface =  wlsurface;
        check_result(vkCreateWaylandSurfaceKHR(instance, &surfaceCreateInfo,
                                               NULL, &surface));

        /*
         * Create device
         */
        uint32_t queueFamilyIndex = 0;
        check_result(vkGetBestGraphicsQueue(usedPhysicalDevice));

        const float queuePrioritory = 1.0f;
        const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            0,
            0,
            queueFamilyIndex,
            1,
            &queuePrioritory
        };

        const VkDeviceCreateInfo deviceCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            0,
            0,
            1,
            &deviceQueueCreateInfo,
            0,
            0,
            0,
            0,
            0
        };
        check_result(vkCreateDevice(usedPhysicalDevice, &deviceCreateInfo, 0, &device));
        printf("Created device\n");

        /*
         * Get queues
         */
        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        if (graphicsFamily != presentFamily) {
            printf("Get present queue\n");
            vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
        }

        /*
         * Create swapchain
         */
        if (!createSwapchain()) {
            printf("Failed to create swapchain\n");
        }

        /*
         * Create sync objects
         */
        createSyncObjects();

        printf("Start render loop\n");
        size_t currentFrame = 0;
        for (currentFrame = 0; currentFrame < 1020*60; /*++currentFrame*/) {
            vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
            VkClearColorValue clearColor = { {0} };

            printf("render loop 1 frame %lu\n", currentFrame);
            uint32_t imageIndex = 0;
            printf("render loop 1 imageIndex %u\n", imageIndex);
            check_result(vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex));

            printf("render loop 2 frame %lu\n", currentFrame);
            if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
                vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
            }
            imagesInFlight[imageIndex] = inFlightFences[currentFrame];

            printf("render loop 3 frame %lu\n", currentFrame);
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            vkResetFences(device, 1, &inFlightFences[currentFrame]);

            printf("render loop 4 frame %lu imageIndex %u\n", currentFrame, imageIndex);
            clearColor.float32[0] = (float)(0.5);
            clearColor.float32[1] = (float)(0.5);
            clearColor.float32[2] = (float)(0.5);
            clearColor.float32[3] = 1;
            rerecordCommandBuffer(imageIndex, &clearColor);

            printf("render loop 5 frame %lu, graphicsQueue %p\n", currentFrame, graphicsQueue);
            if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
                printf("failed to submit draw command buffer!\n");
                exit(1);
            }

            printf("render loop 6 frame %lu\n", currentFrame);
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = {swapChain};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;

            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(presentQueue, &presentInfo);

            printf("render loop 7 frame %lu\n", currentFrame);
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

/*
        VkPhysicalDeviceMemoryProperties properties;

        printf("2\n");
        vkGetPhysicalDeviceMemoryProperties(physicalDevices[i], &properties);

        const int32_t bufferLength = 16384;

        const uint32_t bufferSize = sizeof(int32_t) * bufferLength;

        // we are going to need two buffers from this one memory
        const VkDeviceSize memorySize = bufferSize * 2; 

        // set memoryTypeIndex to an invalid entry in the properties.memoryTypes array
        uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;

        uint32_t k;
        for (k = 0; k < properties.memoryTypeCount; k++) {
            if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & properties.memoryTypes[k].propertyFlags) &&
                (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & properties.memoryTypes[k].propertyFlags) &&
                (memorySize < properties.memoryHeaps[properties.memoryTypes[k].heapIndex].size)) {
                memoryTypeIndex = k;
                break;
            }
        }

        check_result((memoryTypeIndex == VK_MAX_MEMORY_TYPES ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS));

        const VkMemoryAllocateInfo memoryAllocateInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            0,
            memorySize,
            memoryTypeIndex
        };

        printf("3\n");
        VkDeviceMemory memory;
        check_result(vkAllocateMemory(device, &memoryAllocateInfo, 0, &memory));

        printf("4\n");
        int32_t *payload;
        check_result(vkMapMemory(device, memory, 0, memorySize, 0, (void *)&payload));

        for (k = 1; k < memorySize / sizeof(int32_t); k++) {
            payload[k] = rand();
        }

        printf("5\n");
        vkUnmapMemory(device, memory);

        const VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            0,
            0,
            bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            1,
            &queueFamilyIndex
        };

        printf("6\n");
        VkBuffer in_buffer;
        check_result(vkCreateBuffer(device, &bufferCreateInfo, 0, &in_buffer));

        printf("7\n");
        check_result(vkBindBufferMemory(device, in_buffer, memory, 0));

        printf("8\n");
        VkBuffer out_buffer;
        check_result(vkCreateBuffer(device, &bufferCreateInfo, 0, &out_buffer));

        printf("9\n");
        check_result(vkBindBufferMemory(device, out_buffer, memory, bufferSize));

        VkShaderModuleCreateInfo shaderModuleCreateInfo = {
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            0,
            0,
            sizeof(shader),
            shader
        };

        VkShaderModule shader_module;

        printf("10\n");
        check_result(vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shader_module));

        VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
            {
                0,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0
            },
            {
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                1,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0
            }
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            0,
            0,
            2,
            descriptorSetLayoutBindings
        };

        printf("11\n");
        VkDescriptorSetLayout descriptorSetLayout;
        check_result(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            0,
            0,
            1,
            &descriptorSetLayout,
            0,
            0
        };

        printf("12\n");
        VkPipelineLayout pipelineLayout;
        check_result(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, 0, &pipelineLayout));

        VkComputePipelineCreateInfo computePipelineCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            0,
            0,
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                0,
                0,
                VK_SHADER_STAGE_COMPUTE_BIT,
                shader_module,
                "f",
                0
            },
            pipelineLayout,
            0,
            0
        };

        printf("13\n");
        VkPipeline pipeline;
        printf("Address of function vkCreateComputePipelines: %p\n", vkCreateComputePipelines);
        check_result(vkCreateComputePipelines(device, 0, 1, &computePipelineCreateInfo, 0, &pipeline));

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            0,
            0,
            queueFamilyIndex
        };

        VkDescriptorPoolSize descriptorPoolSize = {
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            2
        };

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            0,
            0,
            1,
            1,
            &descriptorPoolSize
        };

        printf("14\n");
        VkDescriptorPool descriptorPool;
        check_result(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool));

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            0,
            descriptorPool,
            1,
            &descriptorSetLayout
        };

        printf("15\n");
        VkDescriptorSet descriptorSet;
        check_result(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

        VkDescriptorBufferInfo in_descriptorBufferInfo = {
            in_buffer,
            0,
            VK_WHOLE_SIZE
        };

        VkDescriptorBufferInfo out_descriptorBufferInfo = {
            out_buffer,
            0,
            VK_WHOLE_SIZE
        };

        VkWriteDescriptorSet writeDescriptorSet[2] = {
            {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                0,
                descriptorSet,
                0,
                0,
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &in_descriptorBufferInfo,
                0
            },
            {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                0,
                descriptorSet,
                1,
                0,
                1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                0,
                &out_descriptorBufferInfo,
                0
            }
        };

        printf("16\n");
        vkUpdateDescriptorSets(device, 2, writeDescriptorSet, 0, 0);

        printf("17\n");
        VkCommandPool commandPool;
        check_result(vkCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool));

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            0,
            commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1
        };

        printf("18\n");
        VkCommandBuffer commandBuffer;
        check_result(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            0,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            0
        };

        printf("19\n");
        check_result(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

        printf("20\n");
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        printf("21\n");
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
            pipelineLayout, 0, 1, &descriptorSet, 0, 0);

        printf("22\n");
        vkCmdDispatch(commandBuffer, bufferSize / sizeof(int32_t), 1, 1);

        printf("23\n");
        check_result(vkEndCommandBuffer(commandBuffer));

        printf("24\n");
        VkQueue queue;
        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            0,
            0,
            0,
            0,
            1,
            &commandBuffer,
            0,
            0
        };

        printf("25\n");
        check_result(vkQueueSubmit(queue, 1, &submitInfo, 0));

        printf("26\n");
        check_result(vkQueueWaitIdle(queue));

        printf("27\n");
        check_result(vkMapMemory(device, memory, 0, memorySize, 0, (void *)&payload));

        uint32_t e;
        for (k = 0, e = bufferSize / sizeof(int32_t); k < e; k++) {
            check_result(payload[k + e] == payload[k] ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY);
        }
        printf("28\n");
*/
    }

    return 0;
}
