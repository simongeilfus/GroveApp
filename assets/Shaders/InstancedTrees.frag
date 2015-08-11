#version 150

in vec4             vColor;
in vec3             vPosition;
out vec4            oColor;

uniform vec3        uFogColor;
uniform float       uFogDensity;

uniform vec3        uSunColor;
uniform vec3        uSunDirection;
uniform float       uSunDispertion;
uniform float       uSunIntensity;
uniform vec3        uInscatteringCoeffs;

// Fog adapted from from Iñigo Quilez article on fog
// http://www.iquilezles.org/www/articles/fog/fog.htm
vec3 applyFog( vec3 rgb, float dist, vec3 rayDir )
{
    float minSc         = 0.02;
    float density       = -(dist+10.0) * uFogDensity * 0.15 - dist * 0.0025;
    float sunAmount     = pow( max( dot( rayDir, uSunDirection ), 0.0 ), 1.0/(0.008+uSunDispertion*10.0) );
    vec3 sunFogColor    = mix( uFogColor, uSunColor, uSunIntensity * 10.0 * pow(sunAmount,10.0) );
    vec3 insColor       = vec3(1.0) - clamp( vec3( 
        exp(density*(uInscatteringCoeffs.x+minSc)), 
        exp(density*(uInscatteringCoeffs.y+minSc)), 
        exp(density*(uInscatteringCoeffs.z+minSc)) ), 
    vec3(0), vec3(1) );
    
    return mix( rgb, sunFogColor, insColor );
}

void main(){
    float rayLength = length( vPosition );
    vec3 rayDir     = vPosition / rayLength;
    vec3 color      = applyFog( vColor.rgb, rayLength, rayDir );
    oColor          = vec4( color, vColor.a );
}