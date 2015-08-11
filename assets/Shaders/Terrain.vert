#include "Shaders/Common.glsl"

uniform mat4		ciModelViewProjection;
uniform mat4		ciModelView;
uniform mat4		ciProjectionMatrix;
//uniform mat3        ciNormalMatrix;

in vec3				ciPosition;
in vec2				ciTexCoord0;

uniform sampler2D   uHeightMap;
uniform sampler2D   uHeightMapTemp;
uniform sampler2D   uFlora;
uniform float 		uElevation;
uniform float       uHeightMapProgression;

out vec3 			vPosition;
out float			vColor;
out float			vPixelType;

uniform float 	uProgress;

void main(){
	vec2 uv 		= ciTexCoord0.st;
	float height	= mix( texture( uHeightMapTemp, uv ).r, texture( uHeightMap, uv ).r, uHeightMapProgression );

	vec2 flora 		= texture( uFlora, uv ).rg;
	vColor			= flora.g * 0.035;
	vPixelType		= flora.g > 0.1 ? ( 1.0 / 255.0 ) : 0.0;

	vec4 position	= vec4( ciPosition, 1.0 );
	position.y		+= height * uElevation - 1000.0 * ( 1.0 - uProgress );

	vec4 viewPos 	= ciModelView * position;
	vPosition		= viewPos.xyz;
	gl_Position		= ciProjectionMatrix * viewPos;
}