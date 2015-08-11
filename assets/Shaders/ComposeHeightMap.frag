#include "Shaders/Common.glsl"

out vec4            oColor;

uniform sampler2D   uHeightMap;
uniform sampler2D   uBlurredHeightMap;
uniform sampler2D   uRoadTexture;
uniform sampler2D   uBlurredRoadTexture;
uniform sampler2D   uTerrainSlope;

in vec2             vUV;

void main(){
	float height 		= texture( uHeightMap, vUV ).x;
	float blurredHeight	= texture( uBlurredHeightMap, vUV ).x;
	float road 			= texture( uRoadTexture, vUV ).x;
	float blurredRoad 	= texture( uBlurredRoadTexture, vUV ).x;
	float slope 		= texture( uTerrainSlope, vUV ).x;

	float invertSlope 	= ( 1.0f - smoothstep( 0.0005, 0.04, pow( slope, 0.999 ) ) ) * 0.35;
	float roadWeight 	= smoothstep( -0.25, 1.0, road + 1.5 * blurredRoad );

	if( roadWeight > 0.5 ) 
		roadWeight		= 1.0;

	float weight 		= roadWeight;// + invertSlope * 3.0;
	if( weight < 0.5 ) weight += invertSlope * 5.0;

	//weight 			= clamp( weight * 1.3, 0.0, 1.0 );

	float finalHeight 	= mix( height, blurredHeight, weight );
	finalHeight 		*= 1.0 - roadWeight * 0.045 + blurredRoad * 0.005;
	finalHeight 		+= roadWeight * 0.0025;

	oColor 				= vec4( vec3( finalHeight ), 1.0 );
}