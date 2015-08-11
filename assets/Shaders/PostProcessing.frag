#ifdef GL_ARB_gpu_shader5
#extension GL_ARB_gpu_shader5 : enable
#else
#extension GL_EXT_gpu_shader4 : enable
#define FXAA_FAST_PIXEL_OFFSET 0
#endif

#define FXAA_PC 1
#define FXAA_GLSL_130 1
#define FXAA_QUALITY_PRESET 12
#define FXAA_GREEN_AS_LUMA 1
#define FXAA_GATHER4_ALPHA 0

#define texture2DLod textureLod

precision highp float;

#include "Shaders/FXAA3_11.h"

in vec2             vUV;
out vec4            oColor;

uniform sampler2D   uTexture;
uniform vec2        uInvSize;

vec3 random3(vec3 c) {
    float j = 4096.0*sin(dot(c,vec3(17.0, 59.4, 15.0)));
    vec3 r;
    r.z = fract(512.0*j);
    j *= .125;
    r.x = fract(512.0*j);
    j *= .125;
    r.y = fract(512.0*j);
    return r-0.5;
}

void main(){

    // FXAA
    // Only used on FXAA Quality.
    // Choose the amount of sub-pixel aliasing removal.
    // This can effect sharpness.
    //   1.00 - upper limit (softer)
    //   0.75 - default amount of filtering
    //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
    //   0.25 - almost off
    //   0.00 - completely off
    FxaaFloat QualitySubpix = 0.75;

    // The minimum amount of local contrast required to apply algorithm.
    //   0.333 - too little (faster)
    //   0.250 - low quality
    //   0.166 - default
    //   0.125 - high quality
    //   0.033 - very high quality (slower)
    FxaaFloat QualityEdgeThreshold      = 0.125;//0.166;

    // You dont need to touch theses variables it have no visible effect
    FxaaFloat QualityEdgeThresholdMin   = 0.0625;
    FxaaFloat ConsoleEdgeSharpness      = 8.0;
    FxaaFloat ConsoleEdgeThreshold      = 0.125;
    FxaaFloat ConsoleEdgeThresholdMin   = 0.05;
    FxaaFloat4 Console360ConstDir       = FxaaFloat4(1.0, -1.0, 0.25, -0.25);

    vec4 color      = texture( uTexture, vUV ); /*FxaaPixelShader( vUV, FxaaFloat4(0), uTexture, uTexture, uTexture, uInvSize,
        FxaaFloat4(0), FxaaFloat4(0), FxaaFloat4(0), QualitySubpix, QualityEdgeThreshold, QualityEdgeThresholdMin, 
        ConsoleEdgeSharpness, ConsoleEdgeThreshold, ConsoleEdgeThresholdMin, Console360ConstDir);*/

    // brigthness correction
    const float corr    = 1.0f / 1.2f;
    color.rgb           = pow( color.rgb, vec3( corr ) );

    // luma
    float luma          = sqrt( dot( oColor.rgb, vec3( 0.299, 0.587, 0.114 ) ) );
    
    // grain
   // vec3 grain         = 0.235 * random3( vec3( vUV, 0.0 ) );
   // vec3 grainColor     = texture( uTexture, vUV + ( grain.xy * vec2( 2.0 ) - vec2( 1.0 ) ) * 0.0001 ).rgb;
   // color.rgb           = mix( color.rgb, grainColor, grain.z );


    // vignette
   // float distToCenter  = length( vUV - vec2(0.5) );
  //  float vignette      = smoothstep( 0.75, 0.25, distToCenter );
    //color.rgb           = mix( color.rgb, color.rgb * vignette, 0.45 );

	oColor              = vec4( color.rgb, 1.0 );
}
