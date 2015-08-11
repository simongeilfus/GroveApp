#include "Shaders/Common.glsl"

in vec4				ciPosition;
in vec2				ciTexCoord0;
in vec4				ciTexCoord1;
in vec4				ciColor;
out vec3			vPosition;
out vec4			vColor;

uniform mat4		ciModelView;
uniform mat4		ciProjectionMatrix;

uniform sampler2D	uHeightMap;
uniform sampler2D   uHeightMapTemp;
uniform sampler2D	uNoiseLookupTable;
uniform float       uHeightMapProgression;
uniform float 		uElevation;

uniform float 		uTime;
uniform float 		uProgress;
uniform vec3 		uTouch;
uniform float 		uTouchSize;

void main(){

	vColor 			= ciColor;
	vec4 position 	= ciPosition;
	vec3 center 	= ciTexCoord1.xyz;
	float height	= mix( texture( uHeightMapTemp, ciTexCoord0 ).r, texture( uHeightMap, ciTexCoord0 ).r, uHeightMapProgression ) * uElevation - 0.5;

	// elevation
	position.y 		+= height;
	center.y 		+= height;

	vec3 centerOff	= position.xyz - center.xyz;
	vec3 noiseInput	= center.xyz * 0.0375 + vec3( ciTexCoord1.w * 50.0 + uTime * 0.02 );
	vec4 noise 		= texture( uNoiseLookupTable, 0.1* noiseInput.xz ) * vec4( 2.0 ) - vec4( 1.0 );// vec4(SimplexPerlin3D_Deriv( noiseInput ));

	// create a small delay between objects and triangles
	float delay 	= saturate(ciTexCoord1.w);
	float progress 	= uProgress;
	float delayedProgress = ( smoothstep( delay, delay + 0.4, progress * 1.4 ) );

	// random triangle size
	float scale 	= mix( noise.w * 0.5 + 0.5, 1.0, delayedProgress );

	// random triangle animation
	vec3 offset 	= noise.xyz;
	//offset.y 		= offset.y * 0.25 + 0.5;
	//offset.x 		= offset.x * 0.5 + 0.5;
	center.xyz		+= offset * ( 1.0f - delayedProgress ) * 30.0;


	// explosion
	vec3 touchDiff 	= uTouch - center.xyz;
	float dist 		= length( touchDiff );
	center.xyz 		-= ( touchDiff * offset ) * dist * smoothstep( uTouchSize, 0.0, dist );

	// size
	float size 		= delayedProgress;

	// calculate position
	position.xyz 	= center + centerOff * size;

	vec4 viewPos	= ciModelView * position;
	vPosition		= viewPos.xyz;
	gl_Position		= ciProjectionMatrix * viewPos;
}