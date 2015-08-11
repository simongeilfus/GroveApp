#include "Shaders/Common.glsl"
#include "Shaders/Wombat.glsl"

out vec4            oColor;

uniform sampler2D   uRoadTexture;
uniform sampler2D   uBlurredRoadTexture;
uniform sampler2D   uTerrainSlope;
uniform sampler2D   uHeightMap;

uniform vec2        uNoiseSeed;
uniform float       uDensity;

in vec2             vUV;

void main(){
	float road 			= texture( uRoadTexture, vUV ).x;
	float blurredRoad 	= texture( uBlurredRoadTexture, vUV ).x;
	float slope 		= texture( uTerrainSlope, vUV ).x;
	float height 		= texture( uHeightMap, vUV ).y;

    float slopeDensity  = smoothstep( 0.0, 0.06, slope );
	float density		= slopeDensity;
	float distToCenter	= smoothstep( 0.35, 0.55, length( vUV - vec2(0.5) ) );
	density 			+= distToCenter;
	density 			+= 15.0 * ( Hermite2D_Deriv( uNoiseSeed + vUV * 28.0 ).x + Hermite2D_Deriv( uNoiseSeed + vUV * 8.0 ).x * 0.5 + 1.0 ) * density * slopeDensity * uDensity;
	density 			+= ( road + blurredRoad * 5.0 );
    density             *= smoothstep( 0.42, 0.44, length( vUV - vec2(0.5) ) ) + smoothstep( 0.42, 0.40, length( vUV - vec2(0.5) ) );
	oColor 				= vec4( clamp( vec3( 0.5 * density, road + blurredRoad, 0.0 ), vec3(0.0f), vec3(1.0f) ), 1.0 );//clamp( density, 0.0f, 1.0f ) ), 1 );
}