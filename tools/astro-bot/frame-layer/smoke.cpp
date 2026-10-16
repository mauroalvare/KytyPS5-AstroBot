#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

static std::atomic<int> errors{};
static void Check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS) {std::fprintf(stderr,"%s: VkResult %d\n",operation,result);throw std::runtime_error(operation);}
}
static VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void*) {
    if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){++errors;std::fprintf(stderr,"VALIDATION: %s\n",data->pMessage);}
    return VK_FALSE;
}
static uint32_t MemoryType(VkPhysicalDevice physical,uint32_t bits,VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props{};vkGetPhysicalDeviceMemoryProperties(physical,&props);
    for(uint32_t i=0;i<props.memoryTypeCount;++i)if((bits&(1u<<i))&&(props.memoryTypes[i].propertyFlags&flags)==flags)return i;
    throw std::runtime_error("No memory type");
}
struct Case {int width,height,x,y,contentWidth,contentHeight;};
int main(int argc,char** argv) try {
    const bool validation=argc>1&&std::strcmp(argv[1],"--validation")==0;
    if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
    uint32_t extensionCount=0;const char* const* sdlExtensions=SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if(!sdlExtensions)throw std::runtime_error(SDL_GetError());
    std::vector<const char*> extensions(sdlExtensions,sdlExtensions+extensionCount);
    if(validation){extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);}
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="AstroFrame GPU verification";app.apiVersion=VK_API_VERSION_1_3;
    const char* validationLayer="VK_LAYER_KHRONOS_validation";
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;debug.pfnUserCallback=Debug;
    const VkValidationFeatureEnableEXT syncValidation=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};features.pNext=&debug;features.enabledValidationFeatureCount=1;features.pEnabledValidationFeatures=&syncValidation;
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};instanceInfo.pApplicationInfo=&app;
    instanceInfo.enabledExtensionCount=uint32_t(extensions.size());instanceInfo.ppEnabledExtensionNames=extensions.data();
    if(validation){instanceInfo.enabledLayerCount=1;instanceInfo.ppEnabledLayerNames=&validationLayer;instanceInfo.pNext=&features;}
    VkInstance instance{};Check(vkCreateInstance(&instanceInfo,nullptr,&instance),"CreateInstance");
    VkDebugUtilsMessengerEXT messenger{};
    if(validation)Check(reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT"))(instance,&debug,nullptr,&messenger),"CreateDebugMessenger");
    uint32_t physicalCount=0;Check(vkEnumeratePhysicalDevices(instance,&physicalCount,nullptr),"EnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> physicals(physicalCount);Check(vkEnumeratePhysicalDevices(instance,&physicalCount,physicals.data()),"EnumeratePhysicalDevices");
    VkPhysicalDevice physical{};
    for(auto candidate:physicals){VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(candidate,&p);if(!physical||p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)physical=candidate;if(p.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)break;}
    if(!physical)throw std::runtime_error("No Vulkan GPU");
    uint32_t queueCount=0;vkGetPhysicalDeviceQueueFamilyProperties(physical,&queueCount,nullptr);std::vector<VkQueueFamilyProperties> queues(queueCount);vkGetPhysicalDeviceQueueFamilyProperties(physical,&queueCount,queues.data());
    uint32_t family=UINT32_MAX;for(uint32_t i=0;i<queueCount;++i)if(queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){family=i;break;}
    if(family==UINT32_MAX)throw std::runtime_error("No graphics queue");
    const float priority=1;VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queueInfo.queueFamilyIndex=family;queueInfo.queueCount=1;queueInfo.pQueuePriorities=&priority;
    const char* swapchainExtension=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};deviceInfo.queueCreateInfoCount=1;deviceInfo.pQueueCreateInfos=&queueInfo;deviceInfo.enabledExtensionCount=1;deviceInfo.ppEnabledExtensionNames=&swapchainExtension;
    VkDevice device{};Check(vkCreateDevice(physical,&deviceInfo,nullptr,&device),"CreateDevice");
    VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};poolInfo.queueFamilyIndex=family;poolInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool{};Check(vkCreateCommandPool(device,&poolInfo,nullptr,&pool),"CreateCommandPool");
    const Case cases[]={{300,100,61,0,178,100},{240,300,0,82,240,135},{320,180,0,0,320,180},{301,101,60,0,180,101}};
    for(const auto& test:cases){
        auto* window=SDL_CreateWindow("AstroFrame verification",test.width,test.height,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);
        if(!window)throw std::runtime_error(SDL_GetError());
        VkSurfaceKHR surface{};if(!SDL_Vulkan_CreateSurface(window,instance,nullptr,&surface))throw std::runtime_error(SDL_GetError());
        VkBool32 supported{};Check(vkGetPhysicalDeviceSurfaceSupportKHR(physical,family,surface,&supported),"SurfaceSupport");if(!supported)throw std::runtime_error("No presentation support");
        VkSurfaceCapabilitiesKHR caps{};Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps),"SurfaceCapabilities");
        if(caps.currentExtent.width!=uint32_t(test.width)||caps.currentExtent.height!=uint32_t(test.height))throw std::runtime_error("Unexpected drawable extent");
        if((caps.supportedUsageFlags&(VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT))!=(VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT))throw std::runtime_error("Swapchain transfer unsupported");
        uint32_t formatCount=0;Check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&formatCount,nullptr),"SurfaceFormats");std::vector<VkSurfaceFormatKHR> formats(formatCount);Check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&formatCount,formats.data()),"SurfaceFormats");
        VkSurfaceFormatKHR format{};for(auto f:formats)if(f.format==VK_FORMAT_B8G8R8A8_UNORM||f.format==VK_FORMAT_R8G8B8A8_UNORM){format=f;break;}
        if(format.format==VK_FORMAT_UNDEFINED)throw std::runtime_error("No supported UNORM format");
        VkSwapchainCreateInfoKHR swapInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};swapInfo.surface=surface;swapInfo.minImageCount=caps.minImageCount;swapInfo.imageFormat=format.format;swapInfo.imageColorSpace=format.colorSpace;swapInfo.imageExtent=caps.currentExtent;swapInfo.imageArrayLayers=1;swapInfo.imageUsage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;swapInfo.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;swapInfo.preTransform=caps.currentTransform;swapInfo.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;swapInfo.presentMode=VK_PRESENT_MODE_FIFO_KHR;swapInfo.clipped=VK_TRUE;
        VkSwapchainKHR swapchain{};Check(vkCreateSwapchainKHR(device,&swapInfo,nullptr,&swapchain),"CreateSwapchain");
        uint32_t imageCount=0;Check(vkGetSwapchainImagesKHR(device,swapchain,&imageCount,nullptr),"SwapchainImages");std::vector<VkImage> images(imageCount);Check(vkGetSwapchainImagesKHR(device,swapchain,&imageCount,images.data()),"SwapchainImages");
        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VkSemaphore acquired{};Check(vkCreateSemaphore(device,&semaphoreInfo,nullptr,&acquired),"CreateSemaphore");uint32_t index=0;Check(vkAcquireNextImageKHR(device,swapchain,5'000'000'000,acquired,VK_NULL_HANDLE,&index),"AcquireImage");
        VkImageCreateInfo sourceInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};sourceInfo.imageType=VK_IMAGE_TYPE_2D;sourceInfo.format=VK_FORMAT_R8G8B8A8_UNORM;sourceInfo.extent={16,9,1};sourceInfo.mipLevels=sourceInfo.arrayLayers=1;sourceInfo.samples=VK_SAMPLE_COUNT_1_BIT;sourceInfo.tiling=VK_IMAGE_TILING_OPTIMAL;sourceInfo.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;sourceInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        VkImage source{};Check(vkCreateImage(device,&sourceInfo,nullptr,&source),"CreateImage");VkMemoryRequirements requirements{};vkGetImageMemoryRequirements(device,source,&requirements);VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=requirements.size;allocate.memoryTypeIndex=MemoryType(physical,requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);VkDeviceMemory sourceMemory{};Check(vkAllocateMemory(device,&allocate,nullptr,&sourceMemory),"AllocateImageMemory");Check(vkBindImageMemory(device,source,sourceMemory,0),"BindImageMemory");
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bufferInfo.size=VkDeviceSize(test.width)*test.height*4;bufferInfo.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;bufferInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;VkBuffer buffer{};Check(vkCreateBuffer(device,&bufferInfo,nullptr,&buffer),"CreateBuffer");vkGetBufferMemoryRequirements(device,buffer,&requirements);allocate.allocationSize=requirements.size;allocate.memoryTypeIndex=MemoryType(physical,requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);VkDeviceMemory bufferMemory{};Check(vkAllocateMemory(device,&allocate,nullptr,&bufferMemory),"AllocateBufferMemory");Check(vkBindBufferMemory(device,buffer,bufferMemory,0),"BindBufferMemory");
        VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};commandInfo.commandPool=pool;commandInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;commandInfo.commandBufferCount=1;VkCommandBuffer command{};Check(vkAllocateCommandBuffers(device,&commandInfo,&command),"AllocateCommandBuffer");VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(command,&begin),"BeginCommandBuffer");
        auto transition=[&](VkImage image,VkImageLayout oldLayout,VkImageLayout newLayout,VkAccessFlags before,VkAccessFlags after){VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=before;b.dstAccessMask=after;b.oldLayout=oldLayout;b.newLayout=newLayout;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);};
        transition(source,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearColorValue white{{1,1,1,1}};const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdClearColorImage(command,source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&white,1,&range);
        transition(source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        transition(images[index],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.srcOffsets[1]={16,9,1};region.dstOffsets[1]={test.width,test.height,1};vkCmdBlitImage(command,source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,VK_FILTER_LINEAR);
        transition(images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(test.width),uint32_t(test.height),1};vkCmdCopyImageToBuffer(command,images[index],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&copy);
        transition(images[index],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_TRANSFER_READ_BIT,0);Check(vkEndCommandBuffer(command),"EndCommandBuffer");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence{};Check(vkCreateFence(device,&fenceInfo,nullptr,&fence),"CreateFence");const VkPipelineStageFlags waitStage=VK_PIPELINE_STAGE_TRANSFER_BIT;VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired;submit.pWaitDstStageMask=&waitStage;submit.commandBufferCount=1;submit.pCommandBuffers=&command;Check(vkQueueSubmit(queue,1,&submit,fence),"QueueSubmit");Check(vkWaitForFences(device,1,&fence,VK_TRUE,10'000'000'000),"WaitFence");
        void* mapped{};Check(vkMapMemory(device,bufferMemory,0,VK_WHOLE_SIZE,0,&mapped),"MapReadback");auto* pixels=static_cast<unsigned char*>(mapped);int mismatches=0;
        for(int y=0;y<test.height;++y)for(int x=0;x<test.width;++x){const bool content=x>=test.x&&x<test.x+test.contentWidth&&y>=test.y&&y<test.y+test.contentHeight;const int expected=content?255:0;const auto pos=(y*test.width+x)*4;for(int c=0;c<3;++c)if(pixels[pos+c]!=expected)++mismatches;}
        vkUnmapMemory(device,bufferMemory);
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.swapchainCount=1;present.pSwapchains=&swapchain;present.pImageIndices=&index;Check(vkQueuePresentKHR(queue,&present),"QueuePresent");Check(vkDeviceWaitIdle(device),"DeviceWaitIdle");
        std::printf("%dx%d: verified all RGB pixels, mismatches=%d\n",test.width,test.height,mismatches);if(mismatches)throw std::runtime_error("Pixel comparison failed");
        vkDestroyFence(device,fence,nullptr);vkFreeCommandBuffers(device,pool,1,&command);vkDestroySemaphore(device,acquired,nullptr);vkDestroyBuffer(device,buffer,nullptr);vkFreeMemory(device,bufferMemory,nullptr);vkDestroyImage(device,source,nullptr);vkFreeMemory(device,sourceMemory,nullptr);vkDestroySwapchainKHR(device,swapchain,nullptr);vkDestroySurfaceKHR(instance,surface,nullptr);SDL_DestroyWindow(window);
    }
    vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);
    if(messenger)reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT"))(instance,messenger,nullptr);
    vkDestroyInstance(instance,nullptr);SDL_Quit();
    std::printf("Validation errors: %d\n",errors.load());return errors.load()?1:0;
} catch(const std::exception& e){std::fprintf(stderr,"FAILED: %s\n",e.what());return 1;}
