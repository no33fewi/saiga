/**
 * Copyright (c) 2017 Darius Rückert 
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "saiga/world/pointCloud.h"

#include "saiga/opengl/shader/shaderLoader.h"

namespace Saiga {


PointCloud::PointCloud()
{
    shader = ShaderLoader::instance()->load<MVPShader>("colored_points.glsl");
    buffer.setDrawMode(GL_POINTS);
}

void PointCloud::render(Camera *cam)
{
    glPointSize(pointSize);
    shader->bind();

    shader->uploadModel(model);

    buffer.bindAndDraw();

    shader->unbind();
}

void PointCloud::updateBuffer()
{
    buffer.set(points,GL_STATIC_DRAW);
}


template<>
void VertexBuffer<PointVertex>::setVertexAttributes(){
    glEnableVertexAttribArray( 0 );
    glEnableVertexAttribArray( 1 );

    glVertexAttribPointer(0,3, GL_FLOAT, GL_FALSE, sizeof(PointVertex), NULL );
    glVertexAttribPointer(1,3, GL_FLOAT, GL_FALSE, sizeof(PointVertex), (void*) (3 * sizeof(GLfloat)) );
}

}
