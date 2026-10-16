// CPU command-contract checks. No window, graphics driver or game is started.
#include "layer.cpp"
#include <stdexcept>

namespace {
int clears=0,barriers=0,presents=0;
bool rejectCandidate=false,deviceLost=false;
std::vector<uint32_t> shaderCalls;
void Require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
VKAPI_ATTR void VKAPI_CALL Clear(VkCommandBuffer,VkImage,VkImageLayout layout,const VkClearColorValue* color,uint32_t count,const VkImageSubresourceRange* range) {
    ++clears;
    Require(layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,"clear layout");
    Require(count==1&&range->levelCount==1&&range->layerCount==1,"clear range");
    Require(color->float32[0]==0&&color->float32[1]==0&&color->float32[2]==0&&color->float32[3]==1,"opaque black");
}
VKAPI_ATTR void VKAPI_CALL Barrier(VkCommandBuffer,VkPipelineStageFlags before,VkPipelineStageFlags after,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t count,const VkImageMemoryBarrier* barriers_) {
    ++barriers;
    Require(before==VK_PIPELINE_STAGE_TRANSFER_BIT&&after==VK_PIPELINE_STAGE_TRANSFER_BIT,"transfer ordering");
    Require(count==1&&barriers_->srcAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT&&barriers_->dstAccessMask==VK_ACCESS_TRANSFER_WRITE_BIT,"clear/blit WAW ordering");
    Require(barriers_->oldLayout==barriers_->newLayout,"no unexpected layout transition");
}
VKAPI_ATTR VkResult VKAPI_CALL Present(VkQueue,const VkPresentInfoKHR*) {++presents;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL Begin(VkCommandBuffer,const VkRenderingInfo*) {}
VKAPI_ATTR VkResult VKAPI_CALL CreateShader(VkDevice,const VkShaderModuleCreateInfo* info,const VkAllocationCallbacks* alloc,VkShaderModule* out) {
    Require(info->pNext==reinterpret_cast<const void*>(uintptr_t(0x123)),"shader pNext preserved");
    Require(alloc==reinterpret_cast<const VkAllocationCallbacks*>(uintptr_t(0x456)),"shader allocator preserved");
    const auto tag=info->pCode[4];shaderCalls.push_back(tag);
    Require(info->codeSize==(tag==99?6u:5u)*sizeof(uint32_t),"shader byte count forwarded");
    if(tag==99&&deviceLost)return VK_ERROR_DEVICE_LOST;
    if(tag==99&&rejectCandidate)return VK_ERROR_UNKNOWN;
    *out=reinterpret_cast<VkShaderModule>(uintptr_t(tag+100));return VK_SUCCESS;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Next(VkDevice,const char* name) {
    return std::strcmp(name,"vkCreateShaderModule")==0?reinterpret_cast<PFN_vkVoidFunction>(CreateShader):reinterpret_cast<PFN_vkVoidFunction>(Begin);
}

VkImageBlit Region(int width,int height) {
    VkImageBlit region{};
    region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    region.srcOffsets[1]={3840,2160,1};region.dstOffsets[1]={width,height,1};
    return region;
}
}

int main() try {
    auto d=std::make_shared<Device>();d->clear=Clear;d->barrier=Barrier;d->present=Present;d->gdpa=Next;
    const auto image=reinterpret_cast<VkImage>(uintptr_t(1));
    struct Case {int w,h,x,y,cw,ch;bool sixteen;};
    const Case cases[]={{3440,1440,440,0,2560,1440,true},{2560,1440,0,0,2560,1440,true},
        {300,100,61,0,178,100,true},{240,300,0,82,240,135,true},{301,101,60,0,180,101,true},
        {3440,1440,0,0,3440,1440,false}};
    int checks=0;
    for(const auto& test:cases) {
        d->targets[image]={VK_NULL_HANDLE,{uint32_t(test.w),uint32_t(test.h)}};d->force16by9=test.sixteen;
        auto region=Region(test.w,test.h);
        if(!test.sixteen)region.srcOffsets[1]={3440,1440,1};
        clears=barriers=0;std::vector<VkImageBlit> modified;
        const bool changed=Letterbox(d,VK_NULL_HANDLE,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region,modified);
        const bool expected=test.x!=0||test.y!=0;
        Require(changed==expected,"unexpected letterbox decision");
        Require(clears==int(expected)&&barriers==int(expected),"clear/barrier command counts");
        if(changed) {
            Require(modified.size()==1,"modified region count");
            const auto& output=modified[0];
            Require(output.dstOffsets[0].x==test.x&&output.dstOffsets[0].y==test.y,"content origin");
            Require(output.dstOffsets[1].x==test.x+test.cw&&output.dstOffsets[1].y==test.y+test.ch,"content size");
            Require(std::memcmp(output.srcOffsets,region.srcOffsets,sizeof(region.srcOffsets))==0,"source changed");
        }
        ++checks;
    }
    d->targets[image]={VK_NULL_HANDLE,{3440,1440}};d->force16by9=true;
    // A partial overlay, alternate mip/layer, or 3D transfer must remain untouched.
    for(int variant=0;variant<11;++variant) {
        auto region=Region(3440,1440);VkImageLayout layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;uint32_t count=1;
        switch(variant) {
        case 0:region.dstOffsets[0].x=20;break;
        case 1:region.dstOffsets[1].y=720;break;
        case 2:region.dstSubresource.mipLevel=1;break;
        case 3:region.dstSubresource.baseArrayLayer=1;break;
        case 4:region.dstSubresource.layerCount=2;break;
        case 5:region.srcOffsets[1].z=2;break;
        case 6:region.srcSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;break;
        case 7:region.srcOffsets[1].x=0;break;
        case 8:layout=VK_IMAGE_LAYOUT_GENERAL;break;
        case 9:count=0;break;
        case 10:count=2;break;
        }
        clears=barriers=0;std::vector<VkImageBlit> modified;
        Require(!Letterbox(d,VK_NULL_HANDLE,image,layout,count,&region,modified),"nonfinal transfer intercepted");
        Require(clears==0&&barriers==0&&modified.empty(),"nonfinal transfer emitted commands");++checks;
    }
    void* dispatch=reinterpret_cast<void*>(uintptr_t(5));
    auto device=reinterpret_cast<VkDevice>(&dispatch);auto queue=reinterpret_cast<VkQueue>(&dispatch);
    devices[dispatch]=d;
    Require(vkGetDeviceProcAddr(device,"vkCmdBeginRendering")==reinterpret_cast<PFN_vkVoidFunction>(Begin),"production diagnostics bypass");++checks;
    d->renderDiagnostics=true;
    Require(vkGetDeviceProcAddr(device,"vkCmdBeginRendering")==reinterpret_cast<PFN_vkVoidFunction>(vkCmdBeginRendering),"diagnostics hook");++checks;
    Require(vkQueuePresentKHR(queue,nullptr)==VK_SUCCESS&&presents==1&&d->frame==0,"production present bypass");++checks;
    // Color-only and color/depth passes at the same size carry different
    // resolution evidence; neither may suppress the other's diagnostic record.
    d->info=std::tmpfile();Require(d->info!=nullptr,"temporary diagnostic log");d->beginRendering=Begin;
    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};rendering.renderArea.extent={2560,1440};rendering.colorAttachmentCount=1;
    const auto command=reinterpret_cast<VkCommandBuffer>(&dispatch);
    vkCmdBeginRendering(command,&rendering);
    VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};rendering.pDepthAttachment=&depth;
    vkCmdBeginRendering(command,&rendering);
    Require(d->renderSizes.size()==2,"color/depth render area diagnostics");++checks;
    Require(ShaderFingerprint("abc",3)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 known vector");++checks;
    // Synthetic module data and a stub creation function test the wrapper's
    // forwarding contract only. Actual candidate SPIR-V is validated separately.
    d->createShaderModule=CreateShader;
    uint32_t code[]={0x07230203,0x10600,0,1,0};
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.pNext=reinterpret_cast<const void*>(uintptr_t(0x123));info.codeSize=sizeof(code);info.pCode=code;
    const auto allocator=reinterpret_cast<const VkAllocationCallbacks*>(uintptr_t(0x456));VkShaderModule shader{};
    Require(vkGetDeviceProcAddr(device,"vkCreateShaderModule")==reinterpret_cast<PFN_vkVoidFunction>(CreateShader),"shader hook bypass by default");++checks;
    d->shaderOverrides[ShaderFingerprint(code,sizeof(code))]={0x07230203,0x10600,0,1,99,0x1234};
    Require(vkGetDeviceProcAddr(device,"vkCreateShaderModule")==reinterpret_cast<PFN_vkVoidFunction>(vkCreateShaderModule),"optional shader hook");++checks;
    Require(vkCreateShaderModule(device,&info,allocator,&shader)==VK_SUCCESS&&shaderCalls==std::vector<uint32_t>{99}&&code[4]==0,"candidate substitution and original preservation");++checks;
    shaderCalls.clear();rejectCandidate=true;
    Require(vkCreateShaderModule(device,&info,allocator,&shader)==VK_SUCCESS&&shaderCalls==std::vector<uint32_t>({99,0}),"rejected candidate falls back to original");++checks;
    shaderCalls.clear();deviceLost=true;
    Require(vkCreateShaderModule(device,&info,allocator,&shader)==VK_ERROR_DEVICE_LOST&&shaderCalls==std::vector<uint32_t>{99},"device loss does not retry");++checks;
    deviceLost=rejectCandidate=false;shaderCalls.clear();code[4]=7;
    Require(vkCreateShaderModule(device,&info,allocator,&shader)==VK_SUCCESS&&shaderCalls==std::vector<uint32_t>{7},"unmatched shader unchanged");++checks;
    devices.clear();
    std::printf("%d CPU checks passed; no window or GPU work was used.\n",checks);return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"FAILED: %s\n",error.what());return 1;}
