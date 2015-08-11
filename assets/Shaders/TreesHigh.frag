#include "Shaders/Common.glsl"
#include "Shaders/Fog.glsl"

//in vec4             vColor;
in vec3             vPosition;

layout(location = 0) out vec3 oColor;
layout(location = 1) out vec3 oDepthId;

void main(){
    float rayLength = length( vPosition );
    vec3 rayDir     = vPosition / rayLength;
    vec3 color      = applyFog( vec3(0), rayLength, rayDir ).rgb;
    oColor          = color;

    // pack depth to .rg and object data to .b
    float depth     = gl_FragCoord.z * 256.0;
    float depthX    = floor( depth );
    depth           = ( depth - depthX ) * 256.0;
    float depthY    = floor( depth );
    depthX          *= 0.00390625;
    depthY          *= 0.00390625; 
    oDepthId        = vec3( depthX, depthY, 2.0 / 255.0f );
}