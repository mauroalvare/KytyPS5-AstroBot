// Independent CPU-only regressions. No Vulkan loader/driver/window is used.
#include "layer.cpp"
#include <stdexcept>

namespace {
int clears=0,barriers=0,coreCalls=0,khrCalls=0;
bool hasCore=false,hasKhr=false;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
VKAPI_ATTR void VKAPI_CALL Clear(VkCommandBuffer,VkImage,VkImageLayout,const VkClearColorValue*,uint32_t,const VkImageSubresourceRange*){++clears;}
VKAPI_ATTR void VKAPI_CALL Barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*){++barriers;}
VKAPI_ATTR void VKAPI_CALL Core(VkCommandBuffer,const VkRenderingInfo*){++coreCalls;}
VKAPI_ATTR void VKAPI_CALL Khr(VkCommandBuffer,const VkRenderingInfo*){++khrCalls;}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL NextDevice(VkDevice,const char* name){
    if(std::strcmp(name,"vkCmdBeginRendering")==0&&hasCore)return reinterpret_cast<PFN_vkVoidFunction>(Core);
    if(std::strcmp(name,"vkCmdBeginRenderingKHR")==0&&hasKhr)return reinterpret_cast<PFN_vkVoidFunction>(Khr);
    return nullptr;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL NextInstance(VkInstance,const char* name){return NextDevice(VK_NULL_HANDLE,name);}
}

int main() try {
    auto d=std::make_shared<Device>();d->force16by9=true;d->clear=Clear;d->barrier=Barrier;d->gdpa=NextDevice;
    void* dispatch=reinterpret_cast<void*>(uintptr_t(5));
    const auto device=reinterpret_cast<VkDevice>(&dispatch);
    const auto command=reinterpret_cast<VkCommandBuffer>(&dispatch);
    const auto instance=reinterpret_cast<VkInstance>(&dispatch);
    const auto image=reinterpret_cast<VkImage>(uintptr_t(1));
    int checks=0;
    for(const VkExtent2D extent: {VkExtent2D{301,170},VkExtent2D{303,170}}) {
        d->targets[image]={VK_NULL_HANDLE,extent};
        VkImageBlit region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        region.srcOffsets[1]={3840,2160,1};region.dstOffsets[1]={int32_t(extent.width),int32_t(extent.height),1};
        std::vector<VkImageBlit> modified;clears=barriers=0;
        Require(Letterbox(d,command,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,modified),"single trailing-pixel bar skipped");
        Require(clears==1&&barriers==1,"single-pixel bar requires clear and WAW ordering");
        Require(modified[0].dstOffsets[0].x==0&&modified[0].dstOffsets[0].y==0,"single-pixel origin");
        Require(extent.width==301?modified[0].dstOffsets[1].y==169:modified[0].dstOffsets[1].x==302,"single-pixel content extent");
        ++checks;
    }
    for(int state=0;state<4;++state) {
        hasCore=(state&1)!=0;hasKhr=(state&2)!=0;
        const auto expected=hasCore?Core:(hasKhr?Khr:nullptr);
        Require(ResolveBeginRendering(NextDevice,device)==expected,"core/KHR resolution");++checks;
    }
    devices[dispatch]=d;instances[dispatch]={instance,NextInstance,nullptr};
    hasCore=false;hasKhr=true;d->beginRendering=ResolveBeginRendering(NextDevice,device);d->renderDiagnostics=true;
    const auto hook=reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRendering);
    Require(vkGetDeviceProcAddr(device,"vkCmdBeginRendering")==nullptr,"unsupported core command advertised");++checks;
    Require(vkGetDeviceProcAddr(device,"vkCmdBeginRenderingKHR")==hook,"KHR diagnostic hook unavailable");++checks;
    vkCmdBeginRendering(command,nullptr);
    Require(khrCalls==1&&coreCalls==0,"KHR wrapper called wrong/null downstream");++checks;
    Require(vkGetInstanceProcAddr(instance,"vkCmdBeginRendering")==nullptr,"instance advertised absent core command");++checks;
    Require(vkGetInstanceProcAddr(instance,"vkCmdBeginRenderingKHR")==hook,"instance KHR hook unavailable");++checks;
    d->renderDiagnostics=false;
    Require(vkGetDeviceProcAddr(device,"vkCmdBeginRenderingKHR")==reinterpret_cast<PFN_vkVoidFunction>(Khr),"disabled diagnostics did not bypass hook");++checks;
    hasCore=hasKhr=false;d->beginRendering=nullptr;d->renderDiagnostics=true;
    Require(vkGetDeviceProcAddr(device,"vkCmdBeginRenderingKHR")==nullptr,"absent KHR command advertised");++checks;
    Require(vkGetInstanceProcAddr(instance,"vkCmdBeginRenderingKHR")==nullptr,"instance advertised absent KHR command");++checks;
    Require(vkGetInstanceProcAddr(VK_NULL_HANDLE,"vkCmdBeginRenderingKHR")==nullptr,"global device command advertised");++checks;
    devices.clear();instances.clear();
    std::printf("%d CPU regression checks passed; no window or GPU work was used.\n",checks);return 0;
} catch(const std::exception& error){std::fprintf(stderr,"FAILED: %s\n",error.what());return 1;}
