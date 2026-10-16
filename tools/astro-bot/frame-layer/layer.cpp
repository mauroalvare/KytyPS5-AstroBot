#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

#define EXPORT extern "C" __declspec(dllexport)
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char*);
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char*);

namespace {
template<class T> void* Key(T handle) { return *reinterpret_cast<void* const*>(handle); }
struct Instance {
    VkInstance handle;
    PFN_vkGetInstanceProcAddr gipa;
    PFN_GetPhysicalDeviceProcAddr pdpa;
};
struct Target { VkSwapchainKHR swapchain; VkExtent2D extent; };
struct Device {
    PFN_vkGetDeviceProcAddr gdpa;
    PFN_vkDestroyDevice destroy;
    PFN_vkCreateSwapchainKHR createSwapchain;
    PFN_vkDestroySwapchainKHR destroySwapchain;
    PFN_vkGetSwapchainImagesKHR getImages;
    PFN_vkCmdBlitImage blit;
    PFN_vkCmdClearColorImage clear;
    PFN_vkCmdPipelineBarrier barrier;
    PFN_vkQueuePresentKHR present;
    PFN_vkCmdBeginRendering beginRendering;
    PFN_vkCreateShaderModule createShaderModule;
    std::unordered_map<std::string,std::vector<uint32_t>> shaderOverrides;
    std::atomic<uint64_t> shaderReplacements{0};
    bool renderDiagnostics=false;
    bool force16by9=false;
    std::set<std::tuple<uint32_t,uint32_t,uint32_t,bool,bool>> renderSizes;
    std::shared_mutex targetsMutex;
    std::unordered_map<VkImage,Target> targets;
    std::unordered_map<VkSwapchainKHR,VkExtent2D> swapchains;
    std::mutex logMutex;
    FILE* info = nullptr;
    FILE* frames = nullptr;
    uint32_t lastWidth=0, lastHeight=0;
    uint64_t frame=0;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now(), previous=start;
    ~Device() { if(info) fclose(info); if(frames) fclose(frames); }
};
std::shared_mutex mapsMutex;
std::unordered_map<void*,Instance> instances;
std::unordered_map<void*,std::shared_ptr<Device>> devices;
template<class T> std::shared_ptr<Device> GetDevice(T handle) {
    std::shared_lock lock(mapsMutex);
    auto it=devices.find(Key(handle));
    return it==devices.end()?nullptr:it->second;
}
Instance GetInstance(VkInstance instance) {
    std::shared_lock lock(mapsMutex);
    auto it=instances.find(Key(instance));
    return it==instances.end()?Instance{}:it->second;
}
template<class T> T* Link(const void* p, VkStructureType type) {
    for(auto node=static_cast<const VkBaseInStructure*>(p);node;node=node->pNext) {
        if(node->sType==type) {
            auto link=reinterpret_cast<const T*>(node);
            if(link->function==VK_LAYER_LINK_INFO) return const_cast<T*>(link);
        }
    }
    return nullptr;
}
PFN_vkCmdBeginRendering ResolveBeginRendering(PFN_vkGetDeviceProcAddr next,VkDevice device) {
    auto command=reinterpret_cast<PFN_vkCmdBeginRendering>(next(device,"vkCmdBeginRendering"));
    if(!command)command=reinterpret_cast<PFN_vkCmdBeginRendering>(next(device,"vkCmdBeginRenderingKHR"));
    return command;
}
FILE* OpenLog(const char* variable) {
    const char* path=std::getenv(variable);
    return path&&*path?std::fopen(path,"wb"):nullptr;
}
std::string ShaderFingerprint(const void* code,size_t bytes) {
    if(!code||bytes==0||bytes>ULONG_MAX)return {};
    struct Algorithm {
        BCRYPT_ALG_HANDLE handle=nullptr;
        Algorithm(){if(BCryptOpenAlgorithmProvider(&handle,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)handle=nullptr;}
        ~Algorithm(){if(handle)BCryptCloseAlgorithmProvider(handle,0);}
    };
    static const Algorithm algorithm;
    std::array<unsigned char,32> digest{};
    if(!algorithm.handle||BCryptHash(algorithm.handle,nullptr,0,
        const_cast<PUCHAR>(static_cast<const unsigned char*>(code)),ULONG(bytes),digest.data(),ULONG(digest.size()))<0)return {};
    constexpr char hex[]="0123456789abcdef";
    std::string result;result.reserve(64);
    for(auto value:digest){result.push_back(hex[value>>4]);result.push_back(hex[value&15]);}
    return result;
}
bool IsDigest(const std::string& value) {
    return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
void LoadShaderOverrides(const std::shared_ptr<Device>& d) {
    const char* directory=std::getenv("ASTRO_FRAME_SHADER_DIR");
    if(!directory||!*directory)return;
    try {
        for(const auto& entry:std::filesystem::directory_iterator(directory)) {
            if(!entry.is_regular_file()||entry.path().extension()!=L".spv")continue;
            const auto key=entry.path().stem().string();const auto size=entry.file_size();
            if(!IsDigest(key)||size<20||size>16*1024*1024||size%4!=0)continue;
            auto checksum=entry.path();checksum.replace_extension(L".sha256");
            std::ifstream digestFile(checksum);std::string expected;digestFile>>expected;
            if(!IsDigest(expected))continue;
            std::vector<uint32_t> words(size/4);
            std::ifstream file(entry.path(),std::ios::binary);
            if(!file.read(reinterpret_cast<char*>(words.data()),std::streamsize(size))||words[0]!=0x07230203)continue;
            if(ShaderFingerprint(words.data(),size)!=expected)continue;
            d->shaderOverrides.emplace(key,std::move(words));
        }
        if(d->info)std::fprintf(d->info,"optional shader candidates=%zu; exact input/output SHA256; runtime effects unverified\n",d->shaderOverrides.size());
    } catch(const std::exception& error) {
        if(d->info)std::fprintf(d->info,"optional shader candidates unavailable: %s\n",error.what());
    }
}
bool Letterbox(const std::shared_ptr<Device>& d, VkCommandBuffer command, VkImage dst,
               VkImageLayout layout, uint32_t count, const VkImageBlit* regions,
               std::vector<VkImageBlit>& modified) {
    if(layout!=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL||count!=1||!regions) return false;
    Target target{};
    {
        std::shared_lock lock(d->targetsMutex);
        auto it=d->targets.find(dst);
        if(it==d->targets.end()) return false;
        target=it->second;
    }
    const auto& src=regions[0];
    // Only the final full-surface transfer belongs to this presentation layer.
    // Partial transfers (for example, an overlay) must not clear an earlier image.
    if(src.srcSubresource.aspectMask!=VK_IMAGE_ASPECT_COLOR_BIT ||
       src.dstSubresource.aspectMask!=VK_IMAGE_ASPECT_COLOR_BIT ||
       src.srcSubresource.layerCount!=1 || src.dstSubresource.layerCount!=1 ||
       src.dstSubresource.mipLevel!=0 || src.dstSubresource.baseArrayLayer!=0 ||
       src.srcOffsets[0].z!=0 || src.srcOffsets[1].z!=1 ||
       src.dstOffsets[0].x!=0 || src.dstOffsets[0].y!=0 || src.dstOffsets[0].z!=0 ||
       src.dstOffsets[1].x!=int32_t(target.extent.width) ||
       src.dstOffsets[1].y!=int32_t(target.extent.height) || src.dstOffsets[1].z!=1) return false;
    const int width=std::abs(src.srcOffsets[1].x-src.srcOffsets[0].x);
    const int height=std::abs(src.srcOffsets[1].y-src.srcOffsets[0].y);
    if(width<=0||height<=0||target.extent.width==0||target.extent.height==0) return false;
    const double ratio=d->force16by9?16.0/9.0:double(width)/height;
    uint32_t fitWidth=target.extent.width, fitHeight=target.extent.height;
    if(double(fitWidth)/fitHeight>ratio) fitWidth=uint32_t(std::lround(fitHeight*ratio));
    else fitHeight=uint32_t(std::lround(fitWidth/ratio));
    const uint32_t x=(target.extent.width-fitWidth)/2, y=(target.extent.height-fitHeight)/2;
    if(d->info) {
        std::lock_guard lock(d->logMutex);
        if(uint32_t(width)!=d->lastWidth||uint32_t(height)!=d->lastHeight) {
            std::fprintf(d->info,"frame=%llu source=%dx%d swapchain=%ux%u content=%ux%u offset=%u,%u\n",
                static_cast<unsigned long long>(d->frame),width,height,target.extent.width,target.extent.height,fitWidth,fitHeight,x,y);
            std::fflush(d->info);
            d->lastWidth=width; d->lastHeight=height;
        }
    }
    if(fitWidth==target.extent.width&&fitHeight==target.extent.height) return false;
    // The emulator has already transitioned this acquired image to TRANSFER_DST.
    // Clear the whole image and serialize the clear before the centered blit (WAW).
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,src.dstSubresource.mipLevel,1,
        src.dstSubresource.baseArrayLayer,src.dstSubresource.layerCount};
    const VkClearColorValue black{{0,0,0,1}};
    d->clear(command,dst,layout,&black,1,&range);
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout=barrier.newLayout=layout;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=dst; barrier.subresourceRange=range;
    d->barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    modified.assign(regions,regions+count);
    modified[0].dstOffsets[0].x=int32_t(x); modified[0].dstOffsets[0].y=int32_t(y);
    modified[0].dstOffsets[1].x=int32_t(x+fitWidth); modified[0].dstOffsets[1].y=int32_t(y+fitHeight);
    return true;
}
}

EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* alloc,VkInstance* out) {
    auto link=Link<VkLayerInstanceCreateInfo>(info->pNext,VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO);
    if(!link||!link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    const auto gipa=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    const auto pdpa=link->u.pLayerInfo->pfnNextGetPhysicalDeviceProcAddr;
    const auto create=reinterpret_cast<PFN_vkCreateInstance>(gipa(nullptr,"vkCreateInstance"));
    link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    const auto result=create(info,alloc,out);
    if(result==VK_SUCCESS) { std::unique_lock lock(mapsMutex); instances[Key(*out)]={*out,gipa,pdpa}; }
    return result;
}
EXPORT VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance,const VkAllocationCallbacks* alloc) {
    const auto next=GetInstance(instance);
    auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(next.gipa(instance,"vkDestroyInstance"));
    {std::unique_lock lock(mapsMutex);instances.erase(Key(instance));}
    destroy(instance,alloc);
}
EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* alloc,VkDevice* out) {
    auto link=Link<VkLayerDeviceCreateInfo>(info->pNext,VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO);
    if(!link||!link->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    const auto gipa=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    const auto gdpa=link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    Instance instance{};
    {std::shared_lock lock(mapsMutex);auto it=instances.find(Key(physical));if(it!=instances.end())instance=it->second;}
    if(!instance.handle) return VK_ERROR_INITIALIZATION_FAILED;
    const auto create=reinterpret_cast<PFN_vkCreateDevice>(gipa(instance.handle,"vkCreateDevice"));
    link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    const auto result=create(physical,info,alloc,out);
    if(result!=VK_SUCCESS) return result;
    auto d=std::make_shared<Device>(); d->gdpa=gdpa;
#define LOAD(member,name) d->member=reinterpret_cast<PFN_##name>(gdpa(*out,#name))
    LOAD(destroy,vkDestroyDevice); LOAD(createSwapchain,vkCreateSwapchainKHR);
    LOAD(destroySwapchain,vkDestroySwapchainKHR); LOAD(getImages,vkGetSwapchainImagesKHR);
    LOAD(blit,vkCmdBlitImage); LOAD(clear,vkCmdClearColorImage); LOAD(barrier,vkCmdPipelineBarrier);
    LOAD(present,vkQueuePresentKHR);
    d->beginRendering=ResolveBeginRendering(gdpa,*out);
    LOAD(createShaderModule,vkCreateShaderModule);
#undef LOAD
    d->info=OpenLog("ASTRO_FRAME_INFO"); d->frames=OpenLog("ASTRO_FRAME_LOG");
    const char* renderDiagnostics=std::getenv("ASTRO_FRAME_RENDER_DIAG");
    d->renderDiagnostics=renderDiagnostics&&std::strcmp(renderDiagnostics,"1")==0;
    const char* aspect=std::getenv("ASTRO_FRAME_ASPECT");
    d->force16by9=aspect&&std::strcmp(aspect,"16:9")==0;
    LoadShaderOverrides(d);
    if(d->frames) { std::setvbuf(d->frames,nullptr,_IOFBF,128*1024);std::fprintf(d->frames,"frame,time_seconds,interval_ms,present_call_ms,result\n"); }
    if(d->info) {std::fprintf(d->info,"Astro frame layer 1; aspect-preserving final blit; SDR unchanged\n");std::fflush(d->info);}
    {std::unique_lock lock(mapsMutex);devices[Key(*out)]=d;}
    return result;
}
EXPORT VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device,const VkAllocationCallbacks* alloc) {
    auto d=GetDevice(device);
    {std::unique_lock lock(mapsMutex);devices.erase(Key(device));}
    d->destroy(device,alloc);
}
EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,const VkShaderModuleCreateInfo* info,const VkAllocationCallbacks* alloc,VkShaderModule* out) {
    auto d=GetDevice(device);
    if(!d->shaderOverrides.empty()) {
        const auto found=d->shaderOverrides.find(ShaderFingerprint(info->pCode,info->codeSize));
        if(found!=d->shaderOverrides.end()) {
            auto rewritten=*info;rewritten.codeSize=found->second.size()*sizeof(uint32_t);rewritten.pCode=found->second.data();
            VkShaderModule shader{};
            const auto result=d->createShaderModule(device,&rewritten,alloc,&shader);
            if(d->info){std::lock_guard lock(d->logMutex);std::fprintf(d->info,"shader candidate input=%s bytes=%zu->%zu result=%d\n",found->first.c_str(),info->codeSize,rewritten.codeSize,result);std::fflush(d->info);}
            if(result==VK_SUCCESS){*out=shader;d->shaderReplacements.fetch_add(1,std::memory_order_relaxed);return result;}
            if(result==VK_ERROR_DEVICE_LOST)return result;
            // A rejected candidate must not prevent the original shader from loading.
        }
    }
    return d->createShaderModule(device,info,alloc,out);
}
EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks* alloc,VkSwapchainKHR* out) {
    auto d=GetDevice(device); const auto result=d->createSwapchain(device,info,alloc,out);
    if(result==VK_SUCCESS) {
        std::unique_lock lock(d->targetsMutex); d->swapchains[*out]=info->imageExtent;
        if(d->info) {std::lock_guard log(d->logMutex);std::fprintf(d->info,"create swapchain=%ux%u format=%d usage=0x%x presentMode=%d\n",info->imageExtent.width,info->imageExtent.height,info->imageFormat,info->imageUsage,info->presentMode);std::fflush(d->info);}
    }
    return result;
}
EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device,VkSwapchainKHR swapchain,uint32_t* count,VkImage* images) {
    auto d=GetDevice(device);const auto result=d->getImages(device,swapchain,count,images);
    if(images&&(result==VK_SUCCESS||result==VK_INCOMPLETE)) {
        std::unique_lock lock(d->targetsMutex);auto it=d->swapchains.find(swapchain);
        if(it!=d->swapchains.end()) for(uint32_t i=0;i<*count;++i)d->targets[images[i]]={swapchain,it->second};
    }
    return result;
}
EXPORT VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device,VkSwapchainKHR swapchain,const VkAllocationCallbacks* alloc) {
    auto d=GetDevice(device);
    {std::unique_lock lock(d->targetsMutex);std::erase_if(d->targets,[&](const auto& item){return item.second.swapchain==swapchain;});d->swapchains.erase(swapchain);}
    d->destroySwapchain(device,swapchain,alloc);
}
EXPORT VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage(VkCommandBuffer command,VkImage src,VkImageLayout srcLayout,VkImage dst,VkImageLayout dstLayout,uint32_t count,const VkImageBlit* regions,VkFilter filter) {
    auto d=GetDevice(command);std::vector<VkImageBlit> modified;
    if(Letterbox(d,command,dst,dstLayout,count,regions,modified))regions=modified.data();
    d->blit(command,src,srcLayout,dst,dstLayout,count,regions,filter);
}
EXPORT VKAPI_ATTR void VKAPI_CALL vkCmdBeginRendering(VkCommandBuffer command,const VkRenderingInfo* info) {
    auto d=GetDevice(command);
    if(d->renderDiagnostics&&d->info) {
        const auto key=std::make_tuple(info->renderArea.extent.width,info->renderArea.extent.height,
            info->colorAttachmentCount,info->pDepthAttachment!=nullptr,info->pStencilAttachment!=nullptr);
        std::lock_guard lock(d->logMutex);
        if(d->renderSizes.insert(key).second) {
            std::fprintf(d->info,"renderArea frame=%llu size=%ux%u colors=%u depth=%u stencil=%u\n",static_cast<unsigned long long>(d->frame),info->renderArea.extent.width,info->renderArea.extent.height,info->colorAttachmentCount,info->pDepthAttachment!=nullptr,info->pStencilAttachment!=nullptr);
            std::fflush(d->info);
        }
    }
    d->beginRendering(command,info);
}
EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,const VkPresentInfoKHR* info) {
    auto d=GetDevice(queue);
    if(!d->info&&!d->frames) return d->present(queue,info);
    const auto now=std::chrono::steady_clock::now();
    const auto result=d->present(queue,info);const auto end=std::chrono::steady_clock::now();
    std::lock_guard lock(d->logMutex);
    ++d->frame;
    if(d->frames) std::fprintf(d->frames,"%llu,%.9f,%.6f,%.6f,%d\n",static_cast<unsigned long long>(d->frame),
        std::chrono::duration<double>(now-d->start).count(),d->frame==1?0.0:std::chrono::duration<double,std::milli>(now-d->previous).count(),std::chrono::duration<double,std::milli>(end-now).count(),result);
    d->previous=now;
    return result;
}
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device,const char* name) {
    if(std::strcmp(name,"vkCreateShaderModule")==0) {
        auto d=GetDevice(device);
        return d&&!d->shaderOverrides.empty()?reinterpret_cast<PFN_vkVoidFunction>(vkCreateShaderModule):(d?d->gdpa(device,name):nullptr);
    }
    // Render diagnostics are opt-in. Forward this frequent command directly
    // when disabled, avoiding a layer lookup and lock on every render pass.
    if(std::strcmp(name,"vkCmdBeginRendering")==0 || std::strcmp(name,"vkCmdBeginRenderingKHR")==0) {
        auto d=GetDevice(device);
        const auto next=d?d->gdpa(device,name):nullptr;
        return next&&d->renderDiagnostics?reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRendering):next;
    }
#define HOOK(func) if(std::strcmp(name,#func)==0)return reinterpret_cast<PFN_vkVoidFunction>(func)
    HOOK(vkGetDeviceProcAddr);HOOK(vkDestroyDevice);HOOK(vkCreateSwapchainKHR);HOOK(vkDestroySwapchainKHR);
    HOOK(vkGetSwapchainImagesKHR);HOOK(vkCmdBlitImage);HOOK(vkQueuePresentKHR);
    HOOK(vkCmdBeginRendering);
    if(std::strcmp(name,"vkCmdBeginRenderingKHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRendering);
#undef HOOK
    auto d=GetDevice(device);return d?d->gdpa(device,name):nullptr;
}
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,const char* name) {
    if(std::strcmp(name,"vkCmdBeginRendering")==0||std::strcmp(name,"vkCmdBeginRenderingKHR")==0) {
        if(!instance)return nullptr;
        const auto i=GetInstance(instance);
        const auto next=i.gipa?i.gipa(instance,name):nullptr;
        return next?reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRendering):nullptr;
    }
#define HOOK(func) if(std::strcmp(name,#func)==0)return reinterpret_cast<PFN_vkVoidFunction>(func)
    HOOK(vkGetInstanceProcAddr);HOOK(vkCreateInstance);HOOK(vkDestroyInstance);HOOK(vkCreateDevice);
    HOOK(vkCreateShaderModule);
    HOOK(vkGetDeviceProcAddr);HOOK(vkDestroyDevice);HOOK(vkCreateSwapchainKHR);HOOK(vkDestroySwapchainKHR);
    HOOK(vkGetSwapchainImagesKHR);HOOK(vkCmdBlitImage);HOOK(vkQueuePresentKHR);
    HOOK(vkCmdBeginRendering);
    if(std::strcmp(name,"vkCmdBeginRenderingKHR")==0)return reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRendering);
#undef HOOK
    if(!instance)return nullptr;
    auto i=GetInstance(instance);return i.gipa?i.gipa(instance,name):nullptr;
}
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_layerGetPhysicalDeviceProcAddr(VkInstance instance,const char* name) {
    auto i=GetInstance(instance);return i.pdpa?i.pdpa(instance,name):(i.gipa?i.gipa(instance,name):nullptr);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* info) {
    if(!info||info->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;
    info->loaderLayerInterfaceVersion=2;info->pfnGetInstanceProcAddr=vkGetInstanceProcAddr;
    info->pfnGetDeviceProcAddr=vkGetDeviceProcAddr;info->pfnGetPhysicalDeviceProcAddr=vk_layerGetPhysicalDeviceProcAddr;
    return VK_SUCCESS;
}
