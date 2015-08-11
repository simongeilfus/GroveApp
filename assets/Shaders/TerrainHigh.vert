#include "Shaders/Common.glsl"

uniform mat4		ciModelViewProjection;
uniform mat4		ciModelView;
uniform mat4		ciProjectionMatrix;
//uniform mat3        ciNormalMatrix;

in vec4				ciPosition;
in vec4				ciColor;
in vec2				ciTexCoord0;
in vec4				ciTexCoord1;

uniform sampler2D   uHeightMap;
uniform sampler2D   uHeightMapTemp;
uniform sampler2D   uFlora;
uniform sampler2D   uNoiseLookupTable;
uniform float 		uElevation;
uniform float       uHeightMapProgression;
//uniform sampler2D   uNormalMap;

out vec3 			vPosition;
//out vec2			vUv;
out float			vColor;
out float			vPixelType;
//out vec3             vNormal;

uniform float 	uTime;
uniform float 	uProgress;
uniform vec3 	uTouch;
uniform float uTouchSize;

void main(){
	vec2 uv 		= ciTexCoord0.st;
	float height	= mix( texture( uHeightMapTemp, uv ).r, texture( uHeightMap, uv ).r, uHeightMapProgression );

	vec2 flora 		= texture( uFlora, uv ).rg;
	vColor			= flora.g * 0.035 + ciColor.r;
	vPixelType		= flora.g > 0.1 ? ( 1.0 / 255.0 ) : 0.0;
	//vUv				= uv;
    //vNormal			= ciNormalMatrix * texture( uNormalMap, uv ).xyz;

	vec4 position	= ciPosition;
	vec3 center 	= ciTexCoord1.xyz;
	vec3 centerOff	= position.xyz - center.xyz;
	vec3 noiseInput	= center.xyz * 0.025 + vec3( ciTexCoord1.w * 0.75 + uTime * 0.01 );
	vec4 offset      = texture( uNoiseLookupTable, noiseInput.xz ) * vec4( 2.0 ) - vec4( 1.0 );
	float delay 	= ciTexCoord1.w;
	float progress 	= uProgress;
	float delayedProgress = smoothstep( delay, delay + 0.1, progress * 1.11111111 );
	center.xyz		+= offset.xyz * ( 1.0f - delayedProgress ) * 50.0;

    // explosion
    vec3 touchDiff  = uTouch - ( center.xyz + vec3( 0.0, height * uElevation, 0.0 ) );
    float dist      = length( touchDiff );
    //centerOff.x     += noise( position.xyz ) * smoothstep( uTouchSize, uTouchSize * 0.8, dist * 1.5 ) * 5.0;
    //centerOff.y     += noise( position.zxy + vec3( 9 ) ) * smoothstep( uTouchSize, uTouchSize * 0.8, dist * 1.5 ) * 5.0;
    //centerOff.z     += noise( position.zyx + vec3( 37 )) * smoothstep( uTouchSize, uTouchSize * 0.8, dist * 1.5 ) * 5.0;
    center.xyz      -= ( touchDiff * offset.xyz ) * vec3( 0.1, 0.5, 0.1 ) * dist * smoothstep( uTouchSize, 0.0, dist );

	position.xyz 	= center + centerOff * delayedProgress;
	position.y		+= height * uElevation;

	vec4 viewPos 	= ciModelView * position;
	vPosition		= viewPos.xyz;
	gl_Position		= ciProjectionMatrix * viewPos;
}