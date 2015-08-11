#include "Shaders/Common.glsl"

in vec4				ciPosition;
in vec3				ciTexCoord0;

in vec4				ciColor;
out vec3			vPosition;


uniform mat4		ciModelView;
uniform mat4		ciProjectionMatrix;

uniform sampler2D	uHeightMap;
uniform sampler2D   uHeightMapTemp;
uniform vec2		uHeightMapSize;
uniform float       uHeightMapProgression;
uniform float 		uElevation;

uniform float 		uProgress;

void main(){
	vec4 position 	= ciPosition;
	vec2 uv 		= vec2( ciTexCoord0.x, ciTexCoord0.y );
	float height	= mix( texture( uHeightMapTemp, uv ).r, texture( uHeightMap, uv ).r, uHeightMapProgression ) * uElevation - 0.5;

	float delay 	= ciTexCoord0.z;
	float progress 	= uProgress;
	float delayedProgress = ( smoothstep( delay, delay + 0.4, progress * 1.4 ) );

	position.xyz 	= vec3( ciTexCoord0.x * uHeightMapSize.x, height, ( ciTexCoord0.y ) * uHeightMapSize.y ) + position.xyz * ( delayedProgress );

	vec4 viewPos	= ciModelView * position;
	vPosition		= viewPos.xyz;
	gl_Position		= ciProjectionMatrix * viewPos;
}