#include "Shaders/Common.glsl"

out vec4            oColor;

uniform sampler2D   uReadTexture;
uniform vec2        uInvSize;
uniform float		uIteration;

in vec2             vUV;

// Intel: An investigation of fast real-time GPU-based image blur algorithms
// https://software.intel.com/en-us/blogs/2014/07/15/an-investigation-of-fast-real-time-gpu-based-image-blur-algorithms
vec3 kawaseBlur( sampler2D tex, vec2 texCoord, vec2 pixelSize, float iteration )
{
	vec2 texCoordSample;
	vec2 halfPixelSize = pixelSize / 2.0f;
	vec2 dUV = ( pixelSize.xy * vec2( iteration, iteration ) ) + halfPixelSize.xy;
	
	vec3 cOut;
	
	// Sample top left pixel
	texCoordSample.x = texCoord.x - dUV.x;
	texCoordSample.y = texCoord.y + dUV.y;
	
	cOut = texture( tex, texCoordSample ).xyz;
	
	// Sample top right pixel
	texCoordSample.x = texCoord.x + dUV.x;
	texCoordSample.y = texCoord.y + dUV.y;
	
	cOut += texture( tex, texCoordSample ).xyz;
	
	// Sample bottom right pixel
	texCoordSample.x = texCoord.x + dUV.x;
	texCoordSample.y = texCoord.y - dUV.y;
	cOut += texture( tex, texCoordSample ).xyz;
	
	// Sample bottom left pixel
	texCoordSample.x = texCoord.x - dUV.x;
	texCoordSample.y = texCoord.y - dUV.y;
	
	cOut += texture( tex, texCoordSample ).xyz;
	
	// Average
	cOut *= 0.25f;
	
	return cOut;
}

void main(){
	oColor = vec4( kawaseBlur( uReadTexture, vUV, uInvSize, uIteration ), 1 );
	//oColor.xyz += KawaseBlurFilter( uSampler, coord, vec2(1.0) * uInvScreenSize, uIteration*2.0 + 1.0 );
	//oColor.xyz *= 0.5;
}