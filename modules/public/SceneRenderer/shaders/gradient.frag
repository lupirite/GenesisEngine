#version 450
layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Constants {
    float time;
    float winWidth;
    float winHeight;
};


const int MAXRAYSTEPS = 500;
const float EPSILON = .005;
const float MAXRAYDIST = 10.;

float smin( float a, float b, float k )
{
    k *= 1.0;
    float r = exp2(-a/k) + exp2(-b/k);
    return -k*log2(r);
}

float getSphereDist(vec3 pos, float radius) {
    return length(pos) - radius;
}

float getDist(vec3 pos) {
    float d1 = getSphereDist(pos-vec3(0., 0., 4.), .5);
    float d2 = getSphereDist(pos-vec3(1.05, 0., 3.8), .75);
    float d3 = smin(d1, d2, .05);

    return d3;
}

vec3 getNorm(vec3 pos) {
    float d = getDist(pos);
    float dx = getDist(pos+vec3(EPSILON, 0, 0));
    float dy = getDist(pos+vec3(0, EPSILON, 0));
    float dz = getDist(pos+vec3(0, 0, EPSILON));

    return normalize((vec3(dx, dy, dz) - vec3(d)));
}

float rayMarch(vec3 pos, vec3 dir) {
    float totalDist = 0.;
    for (int i = 0; i < MAXRAYSTEPS; i++) {
        float dist = getDist(pos);

        if (dist < EPSILON) {
            return totalDist;
        }

        pos = pos + dir*dist;
        totalDist += dist;
    }
    return MAXRAYDIST;
}

vec3 sunDir = vec3(1., -1.5, .5);


void main() {
    float aspectRatio = winWidth/winHeight;
    vec2 uv = vec2((inUV.x - .5)*aspectRatio, inUV.y - .5);

    vec3 rayDir = normalize(vec3(uv.xy, 1.));

    vec3 rayOrigin = vec3(0., 0., 0.);

    float dist = rayMarch(rayOrigin, rayDir);

    vec3 pos = rayOrigin + rayDir*dist;

    vec3 norm = getNorm(pos);

    vec3 col = (1.+dot(norm, -sunDir))/2.*vec3(.25, .1, .9);

    if (dist >= MAXRAYDIST) {
        col = vec3(.5, .6, .9);
    }


    outColor = vec4(col, 1.);
}