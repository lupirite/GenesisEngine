#version 450
layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Constants {
    float time;
    float screenWidth;
    float screenHeight;
} push;

void main() {

    
    float aspectRatio = push.screenWidth/push.screenHeight;
    vec2 uv = vec2((inUV.x - .5)*aspectRatio, inUV.y - .5)*2.;
    if (length(uv) < .5)
        outColor = vec4(sin(uv.x+push.time), uv.y, 0.0, 1.0); //(sin(uv.x+push.time)+1)/2
    else
        outColor = vec4(0, 0, 0, 1);
}