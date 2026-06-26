#version 450
layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Constants {
    vec4 camPos;
    vec4 camUp;
    vec4 camRight;
    vec4 camForward;
    vec4 sphereColor;
    float time;
    float winWidth;
    float winHeight;
    float sphereRadius;
};


const int MAXRAYSTEPS = 150;
const float EPSILON = .01;
const float MAXRAYDIST = 1000.;

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
    float d1 = getSphereDist(pos-vec3(sin(time*2.)+.75, 0., 3.5), sphereRadius);
    float d2 = getSphereDist(pos-vec3(1.05, 0., 3.8), .75);
    float d3 = smin(d1, d2, .05);

    float p1 = -pos.y+2.;

    float c1 = min(d3, p1);

    return c1;
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

vec3 sunDir = vec3(1., 1.5, .5);


void main() {
    float aspectRatio = winWidth/winHeight;
    vec2 uv = vec2((inUV.x - .5)*aspectRatio, inUV.y - .5);

    vec3 rayDir = normalize(uv.x*camRight.xyz + uv.y*camUp.xyz + camForward.xyz);

    vec3 rayOrigin = camPos.xyz;

    float dist = rayMarch(rayOrigin, rayDir);

    vec3 pos = rayOrigin + rayDir*dist;

    vec3 norm = getNorm(pos);

    vec3 col = sphereColor.xyz;
    if (fract(pos.x) < .1 || fract(pos.z) < .1)
        col = vec3(0, 0, 0);

    col *= (1.+dot(norm, -sunDir))/2.;

    if (dist >= MAXRAYDIST) {
        col = vec3(.5, .6, .9);
    }


    outColor = vec4(col, 1.);
}