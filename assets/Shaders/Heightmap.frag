#include "Shaders/Common.glsl"
#include "Shaders/Wombat.glsl"

out vec4 		oColor;
in vec2			vUV;

uniform int     uOctaves;
uniform float   uNoiseScale;
uniform float   uNoiseSeed;

void main(){
	vec2 pos				= vUV * uNoiseScale + vec2( uNoiseSeed );
	//const mat2 mat 			= mat2( 0.5, -0.5, 0.5, 0.5 );
    const mat2 mat          = mat2( 1.8, -1.2, 1.2, 1.8 );
	float gain 				= 0.5f;
	float amp				= 1.0;
	vec2 noiseAccum			= vec2( 0.0 );

	float sum 				= 0.0;

	for( int i = 0; i < uOctaves; i++ ){
		vec3 n				= Hermite2D_Deriv( pos );
		noiseAccum			+= vec2( n.y, n.z );
		sum					+= amp * n.x / ( 1.0 + dot( noiseAccum, noiseAccum ) );
		amp					*= gain;
		pos					= mat * pos;
	}

	sum 					= smoothstep( -0.3, 1.0, sum ) * 0.45;


	float distToCenter 		= smoothstep( 0.45, 0.5, length( vUV - vec2(0.5) ) ) - smoothstep( 0.5, 0.55, length( vUV - vec2(0.5) ) );
	float borders			= distToCenter;	
	pos						= 0.2 * ( vUV + vec2(0.01234) ) * uNoiseScale + vec2( uNoiseSeed );
	amp						= 1.0;
	gain 					= 0.5;
	noiseAccum				= vec2( 0.0 );
	float bordersSum		= borders * abs( Hermite2D_Deriv( pos + vec2(0.1) ).x ) * 0.1;
	for( int i = 0; i < uOctaves; i++ ){
		vec3 n				= borders * abs( Hermite2D_Deriv( pos ) );
		noiseAccum			+= vec2( n.y, n.z );
		bordersSum			+= amp * n.x / ( 1.0 + dot( noiseAccum, noiseAccum ) );
		amp					*= gain;
		pos					= mat * pos;
	}
	sum 					+= bordersSum * 2.5;

	oColor					= vec4( vec3( sum * 0.75 ), 1.0f );
}