﻿#include "Texture.h"
#include "vkImageFormat.h"

#include "saiga/vulkan/buffer/StagingBuffer.h"

namespace Saiga{
namespace Vulkan{


Texture::~Texture()
{
    cout << "destroy texture" << endl;
    destroy();
}

void Texture::destroy()
{
    if(image)
    {
        device.destroyImage(image);
        device.destroyImageView(imageView);
        device.destroySampler(sampler);
        DeviceMemory::destroy();
        image = nullptr;
    }
}

void Texture::transitionImageLayout(vk::CommandBuffer cmd, vk::ImageLayout newLayout)
{


    vk::ImageMemoryBarrier barrier = {};
    barrier.oldLayout = imageLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    //        barrier.srcAccessMask = 0; // TODO
    //        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite; // TODO


    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (imageLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eHost ;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (imageLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eAllCommands;
    } else {
        //            throw std::invalid_argument("unsupported layout transition!");
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits::eAllCommands;
        destinationStage = vk::PipelineStageFlagBits::eAllCommands;
    }



    cmd.pipelineBarrier(sourceStage,destinationStage,vk::DependencyFlags(),0,nullptr,0,nullptr,1,&barrier);


    imageLayout = newLayout;
}

vk::DescriptorImageInfo Texture::getDescriptorInfo()
{
    SAIGA_ASSERT(image);
    vk::DescriptorImageInfo descriptorInfo;
    descriptorInfo.imageLayout = imageLayout;
    descriptorInfo.imageView = imageView;
    descriptorInfo.sampler = sampler;
    SAIGA_ASSERT(imageView && sampler);
    return descriptorInfo;

}

void Texture2D::fromImage(VulkanBase& base, Image &img, vk::ImageUsageFlags usage)
{
    device = base.device;
    //    SAIGA_ASSERT(_img.type == UC3 || _img.type == UC4);


    //    TemplatedImage<ucvec4> img(_img.height,_img.width);


    //    if(_img.type == UC3)
    //    {
    //        auto vimg = _img.getImageView<ucvec3>();
    //        for(int y = 0; y < _img.height; ++y){
    //            for(int x = 0; x < _img.width; ++x){
    //                img(y,x) = ucvec4(vimg(y,x),0);
    //            }
    //        }
    //    }else if(_img.type == UC4)
    //    {
    //        _img.getImageView<ucvec4>().copyTo(img.getImageView());
    //    }


    mipLevels = 1;
    width = img.width;
    height = img.height;

    //    cout << img.type << endl;
    vk::Format format = getvkFormat(img.type);


    //    vk::FormatProperties formatProperties = base.physicalDevice.getFormatProperties(format);






    imageLayout = vk::ImageLayout::eUndefined;
    // Create optimal tiled target image
    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.initialLayout = imageLayout;
    imageCreateInfo.extent = vk::Extent3D{ width, height, 1U };
    imageCreateInfo.usage = usage | vk::ImageUsageFlagBits::eTransferDst;
    image = base.device.createImage(imageCreateInfo);
    SAIGA_ASSERT(image);


    auto memReqs = device.getImageMemoryRequirements(image);
    DeviceMemory::allocateMemory(base,memReqs,vk::MemoryPropertyFlagBits::eDeviceLocal);
    device.bindImageMemory(image,memory,0);






    vk::CommandBuffer cmd = base.createAndBeginTransferCommand();

    transitionImageLayout(cmd,vk::ImageLayout::eTransferDstOptimal);


    vk::BufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = width;
    bufferCopyRegion.imageExtent.height = height;
    bufferCopyRegion.imageExtent.depth = 1;
    bufferCopyRegion.bufferOffset = 0;

    StagingBuffer staging;

    staging.init(base,img.data(),img.size());

    cmd.copyBufferToImage(staging.buffer,image,vk::ImageLayout::eTransferDstOptimal,bufferCopyRegion);

    transitionImageLayout(cmd,vk::ImageLayout::eShaderReadOnlyOptimal);

    base.endTransferWait(cmd);



    staging.destroy();


    vk::ImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.viewType = vk::ImageViewType::e2D;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    viewCreateInfo.image = image;
    imageView = device.createImageView(viewCreateInfo);
    SAIGA_ASSERT(imageView);

    // Create a defaultsampler
    vk::SamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.magFilter =  vk::Filter::eLinear;
    samplerCreateInfo.minFilter = vk::Filter::eLinear;
    samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.compareOp = vk::CompareOp::eNever;
    samplerCreateInfo.minLod = 0.0f;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = 0.0f;
    // Only enable anisotropic filtering if enabled on the devicec
    samplerCreateInfo.maxAnisotropy = 16;
    samplerCreateInfo.anisotropyEnable = false;
    samplerCreateInfo.borderColor = vk::BorderColor::eIntOpaqueWhite;
    //    VK_CHECK_RESULT(vkCreateSampler(device->device, &samplerCreateInfo, nullptr, &sampler));
    sampler = device.createSampler(samplerCreateInfo);
    SAIGA_ASSERT(sampler);

    cout << "texture created." << endl;
}


}
}
