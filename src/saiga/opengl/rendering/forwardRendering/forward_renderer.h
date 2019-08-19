/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once

#include "saiga/opengl/query/gpuTimer.h"
#include "saiga/opengl/rendering/renderer.h"

namespace Saiga
{
class SAIGA_OPENGL_API ForwardRenderingInterface : public RenderingInterfaceBase
{
   public:
    ForwardRenderingInterface(RendererBase& parent) : RenderingInterfaceBase(parent) {}
    virtual ~ForwardRenderingInterface() {}

    // forward rendering path after lighting, but before post processing
    // this could be used for transparent objects
    virtual void renderOverlay(Camera* cam) {}

    // forward rendering path after lighting and after post processing
    virtual void renderFinal(Camera* cam) {}
    // protected:
    //    RendererBase& parentRenderer;
};

struct SAIGA_OPENGL_API ForwardRenderingParameters : public RenderingParameters
{
};

class SAIGA_OPENGL_API Forward_Renderer : public OpenGLRenderer
{
   public:
    ForwardRenderingParameters params;

    Forward_Renderer(OpenGLWindow& window, const ForwardRenderingParameters& params = ForwardRenderingParameters());
    virtual ~Forward_Renderer() {}

    virtual float getTotalRenderTime() override { return timer.getTimeMS(); }
    virtual void render(const RenderInfo& renderInfo) override;

   private:
    FilteredMultiFrameOpenGLTimer timer;
};

}  // namespace Saiga
