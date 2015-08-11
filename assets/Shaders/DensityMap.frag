#include "Shaders/Common.glsl"

out vec4            oColor;

uniform sampler2D   uRoadTexture;
uniform sampler2D   uBlurredRoadTexture;
uniform sampler2D   uTerrainSlope;

in vec2             vUV;

void main(){
	float road 			= texture( uRoadTexture, vUV ).x;
	float blurredRoad 	= texture( uBlurredRoadTexture, vUV ).x;
	float slope 		= texture( uTerrainSlope, vUV ).x;

	float distToCenter	= smoothstep( 0.35, 0.55, length( vUV - vec2(0.5) ) );
	float density 		= road * 2.0 + 
						blurredRoad * 8.0 + 
						smoothstep( 0.0005, 0.04, pow( slope, 0.999 ) ) * 1.65
						- distToCenter * 0.65;

	density 			= smoothstep( 0.0, 2.0, density );
	oColor 				= vec4( vec3( clamp( density, 0.0, 1.0 ) ), 1.0 );
}
