/* SPDX-License-Identifier: MIT
 * Display discovery adapted from Waddamburo's native/prospero/bridge.c.
 * A linked PS5 Vulkan ICD needs no dlopen or desktop window-system surface.
 */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance, const char*);
extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_common_GetDeviceProcAddr(VkDevice, const char*);
PFN_vkVoidFunction yataidon_ps5_vk_proc(const char* name) {
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)vk_icdGetInstanceProcAddr;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vk_common_GetDeviceProcAddr;
    return vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, name);
}
typedef struct {
    VkPhysicalDevice physical; VkDisplayKHR display; VkDisplayModeKHR mode; VkExtent2D extent;
    uint32_t plane,stack; VkDisplayPlaneAlphaFlagBitsKHR alpha;
} NativeDisplay;
#define PROC(type,name) type name=(type)vk_icdGetInstanceProcAddr(instance,#name)
static VkResult discover(VkInstance instance,NativeDisplay* out) {
    PROC(PFN_vkEnumeratePhysicalDevices,vkEnumeratePhysicalDevices);
    PROC(PFN_vkGetPhysicalDeviceDisplayPropertiesKHR,vkGetPhysicalDeviceDisplayPropertiesKHR);
    PROC(PFN_vkGetDisplayModePropertiesKHR,vkGetDisplayModePropertiesKHR);
    PROC(PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR,vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
    PROC(PFN_vkGetDisplayPlaneSupportedDisplaysKHR,vkGetDisplayPlaneSupportedDisplaysKHR);
    PROC(PFN_vkGetDisplayPlaneCapabilitiesKHR,vkGetDisplayPlaneCapabilitiesKHR);
    if (!vkEnumeratePhysicalDevices || !vkGetPhysicalDeviceDisplayPropertiesKHR || !vkGetDisplayModePropertiesKHR
        || !vkGetPhysicalDeviceDisplayPlanePropertiesKHR || !vkGetDisplayPlaneSupportedDisplaysKHR
        || !vkGetDisplayPlaneCapabilitiesKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    uint32_t count=0;
    if (vkEnumeratePhysicalDevices(instance,&count,NULL)!=VK_SUCCESS || !count) return VK_ERROR_INITIALIZATION_FAILED;
    VkPhysicalDevice* devices=calloc(count,sizeof(*devices)); if (!devices) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkResult result=vkEnumeratePhysicalDevices(instance,&count,devices);
    if (result!=VK_SUCCESS) { free(devices); return result; }
    out->physical=devices[0]; free(devices); count=0;
    if (vkGetPhysicalDeviceDisplayPropertiesKHR(out->physical,&count,NULL)!=VK_SUCCESS || !count) return VK_ERROR_INITIALIZATION_FAILED;
    VkDisplayPropertiesKHR* displays=calloc(count,sizeof(*displays)); if (!displays) return VK_ERROR_OUT_OF_HOST_MEMORY;
    result=vkGetPhysicalDeviceDisplayPropertiesKHR(out->physical,&count,displays);
    if (result!=VK_SUCCESS) { free(displays); return result; }
    out->display=displays[0].display; free(displays); count=0;
    if (vkGetDisplayModePropertiesKHR(out->physical,out->display,&count,NULL)!=VK_SUCCESS || !count) return VK_ERROR_INITIALIZATION_FAILED;
    VkDisplayModePropertiesKHR* modes=calloc(count,sizeof(*modes)); if (!modes) return VK_ERROR_OUT_OF_HOST_MEMORY;
    result=vkGetDisplayModePropertiesKHR(out->physical,out->display,&count,modes);
    if (result!=VK_SUCCESS) { free(modes); return result; }
    uint32_t selected = 0;
    uint64_t best_area = 0;
    for (uint32_t i = 0; i < count; ++i) {
        VkExtent2D extent = modes[i].parameters.visibleRegion;
        uint64_t area = (uint64_t)extent.width * extent.height;
        if (extent.width <= 3840 && extent.height <= 2160 && area > best_area) {
            selected = i;
            best_area = area;
        }
    }
    out->mode = modes[selected].displayMode;
    out->extent = modes[selected].parameters.visibleRegion;
    free(modes); count = 0;
    if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(out->physical,&count,NULL)!=VK_SUCCESS || !count) return VK_ERROR_INITIALIZATION_FAILED;
    VkDisplayPlanePropertiesKHR* planes=calloc(count,sizeof(*planes)); if (!planes) return VK_ERROR_OUT_OF_HOST_MEMORY;
    result=vkGetPhysicalDeviceDisplayPlanePropertiesKHR(out->physical,&count,planes);
    if (result!=VK_SUCCESS) { free(planes); return result; }
    int found=0;
    for (uint32_t i=0;i<count && !found;i++) {
        uint32_t supported_count=0;
        if (vkGetDisplayPlaneSupportedDisplaysKHR(out->physical,i,&supported_count,NULL)!=VK_SUCCESS) continue;
        VkDisplayKHR* supported=calloc(supported_count,sizeof(*supported)); if (!supported) continue;
        if (vkGetDisplayPlaneSupportedDisplaysKHR(out->physical,i,&supported_count,supported)==VK_SUCCESS)
            for (uint32_t j=0;j<supported_count;j++) if (supported[j]==out->display) {
                VkDisplayPlaneCapabilitiesKHR caps;
                if (vkGetDisplayPlaneCapabilitiesKHR(out->physical,out->mode,i,&caps)==VK_SUCCESS && caps.supportedAlpha) {
                    out->plane=i; out->stack=planes[i].currentStackIndex;
                    out->alpha=(VkDisplayPlaneAlphaFlagBitsKHR)(caps.supportedAlpha & (0u-caps.supportedAlpha)); found=1; break;
                }
            }
        free(supported);
    }
    free(planes); return found ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}
int yataidon_ps5_display_size(int* width,int* height) {
    if (!width || !height) return VK_ERROR_INITIALIZATION_FAILED;
    *width=*height=0;
    PFN_vkCreateInstance create=(PFN_vkCreateInstance)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE,"vkCreateInstance");
    if (!create) return VK_ERROR_INITIALIZATION_FAILED;
    const char* extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_DISPLAY_EXTENSION_NAME};
    VkApplicationInfo app={.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,.pApplicationName="YataiDON display",.apiVersion=VK_API_VERSION_1_0};
    VkInstanceCreateInfo info={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,.pApplicationInfo=&app,.enabledExtensionCount=2,.ppEnabledExtensionNames=extensions};
    VkInstance instance=VK_NULL_HANDLE; VkResult result=create(&info,NULL,&instance); if (result!=VK_SUCCESS) return result;
    NativeDisplay display={0}; result=discover(instance,&display);
    if (result==VK_SUCCESS) { *width=(int)display.extent.width; *height=(int)display.extent.height; }
    PROC(PFN_vkDestroyInstance,vkDestroyInstance); if (vkDestroyInstance) vkDestroyInstance(instance,NULL);
    return result;
}
VkResult yataidon_ps5_vk_surface(void* unused,VkInstance instance,const VkAllocationCallbacks* allocator,VkSurfaceKHR* surface) {
    (void)unused;
    NativeDisplay display={0}; VkResult result=discover(instance,&display); if (result!=VK_SUCCESS) return result;
    PROC(PFN_vkCreateDisplayPlaneSurfaceKHR,vkCreateDisplayPlaneSurfaceKHR); if (!vkCreateDisplayPlaneSurfaceKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkDisplaySurfaceCreateInfoKHR info={.sType=VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,.displayMode=display.mode,
        .planeIndex=display.plane,.planeStackIndex=display.stack,.transform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .globalAlpha=1,.alphaMode=display.alpha,.imageExtent=display.extent};
    return vkCreateDisplayPlaneSurfaceKHR(instance,&info,allocator,surface);
}
