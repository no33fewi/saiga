
##GL_VERTEX_SHADER

#version 330
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;
layout(location=2) in vec2 in_tex;

uniform mat4 model;
uniform mat4 proj;


out vec2 texCoord;

void main() {
    texCoord = in_tex;
    gl_Position = proj * model * vec4(in_position,1);
}





##GL_FRAGMENT_SHADER

#version 330
uniform mat4 model;
uniform mat4 proj;

uniform vec3 color;
uniform sampler2D text;


in vec2 texCoord;

out vec4 out_color;

void main() {
    vec4 diffColor = vec4(texture(text,texCoord).rgba);
    out_color =  diffColor;
//    out_color =  vec4(1);
}


