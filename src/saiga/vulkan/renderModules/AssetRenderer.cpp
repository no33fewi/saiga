﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "AssetRenderer.h"
#include "saiga/model/objModelLoader.h"
#include "saiga/vulkan/Shader/all.h"
#include "saiga/vulkan/Vertex.h"

#if defined(SAIGA_OPENGL_INCLUDED)
#    error OpenGL was included somewhere.
#endif

namespace Saiga
{
namespace Vulkan
{
void AssetRenderer::destroy()
{
    Pipeline::destroy();
    uniformBufferVS.destroy();
}
bool AssetRenderer::bind(vk::CommandBuffer cmd)
{
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSet, nullptr);
    //    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,pipeline);
    return Pipeline::bind(cmd);
}

void AssetRenderer::pushModel(VkCommandBuffer cmd, mat4 model)
{
    pushConstant(cmd, vk::ShaderStageFlagBits::eVertex, sizeof(mat4), &model[0][0]);
}


void AssetRenderer::updateUniformBuffers(vk::CommandBuffer cmd, glm::mat4 view, glm::mat4 proj)
{
    uboVS.projection = proj;
    uboVS.modelview = view;
    uboVS.lightPos = vec4(5,5,5,0);
    uniformBufferVS.update(cmd, sizeof(uboVS), &uboVS);
}

void AssetRenderer::init(VulkanBase& vulkanDevice, VkRenderPass renderPass)
{
    PipelineBase::init(vulkanDevice, 1);
    addDescriptorSetLayout({{7, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
    addPushConstantRange({vk::ShaderStageFlagBits::eVertex, 0, sizeof(mat4)});
    shaderPipeline.load(device, {"vulkan/coloredAsset.vert", "vulkan/coloredAsset.frag"});
    PipelineInfo info;
    info.addVertexInfo<VertexNC>();
    create(renderPass, info);



    descriptorSet = createDescriptorSet();
    uniformBufferVS.init(vulkanDevice, &uboVS, sizeof(UBOVS));
    vk::DescriptorBufferInfo descriptorInfo = uniformBufferVS.getDescriptorInfo();
    device.updateDescriptorSets(
        {
            vk::WriteDescriptorSet(descriptorSet, 7, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &descriptorInfo,
                                   nullptr),
        },
        nullptr);
}



}  // namespace Vulkan
}  // namespace Saiga
