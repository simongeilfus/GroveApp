#include "Shaders/Common.glsl"
#include "Shaders/Fog.glsl"

in float            vColor;
in vec3 			vPosition;
in float            vPixelType;
//in vec2            vUv;

//uniform mat3        ciNormalMatrix;
//uniform vec2        uUVOffset;
//uniform sampler2D	uHeightMap;
//uniform sampler2D   uFlora;


layout(location = 0) out vec3 oColor;
layout(location = 1) out vec3 oDepthId;

void main(){
    float rayLength = length( vPosition );
    vec3 rayDir     = vPosition / rayLength;
    //float k = texture( uFlora, vUv ).r;
    //vec3 kk          = vec3( 0 );// k < 1.0 ? 1.0 - k : 1.0 );
    vec3 color      = applyFog( vec3( vColor * uFogColor * 4.5 ), rayLength, rayDir ).rgb;
    oColor          = color;

    // pack depth to .rg and object data to .b
    float depth     = gl_FragCoord.z * 256.0;
    float depthX    = floor( depth );
    depth           = ( depth - depthX ) * 256.0;
    float depthY    = floor( depth );
    depthX          *= 0.00390625;
    depthY          *= 0.00390625; 
    oDepthId        = vec3( depthX, depthY, vPixelType );
}