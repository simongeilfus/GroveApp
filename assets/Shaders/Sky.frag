#include "Shaders/Common.glsl"
#include "Shaders/Fog.glsl"

in vec2             vUv;
in vec4             vColor;
in vec3             vNormal;
in vec3             vPosition;
in vec3             vWsPosition;

uniform sampler2D   uNoiseLookupTable;
uniform mat3        ciNormalMatrix;

uniform float       uTime;

layout(location = 0) out vec3 oColor;
layout(location = 1) out vec3 oDepthId;

float noise( vec2 p )
{
    return texture( uNoiseLookupTable, p ).r;
}

float planeDistance( vec3 p, vec4 n )
{
  return dot(p,n.xyz) + n.w;
}

void main(){
    vec3 pos        = vPosition;
    vec3 dir        = normalize( vPosition );
    vec3 wsDir      = normalize( vWsPosition );

    // horizon and mountains
   /* vec3 n          = normalize( ciNormalMatrix * vec3( 0,-1,0) );
    float hor       = 250+ ( noise( vWsPosition.xxz * 0.001 ) ) * 200.0;
    hor             += ( noise( vWsPosition.xxz * 0.01 ) * 0.5 + 0.5 ) * 100.0;
    hor             += ( noise( vWsPosition.xxz * 0.04 ) * 0.5 + 0.5 ) * 30.0;
    hor             -= ( noise( vWsPosition.xxz * 0.0005 ) ) * 100.0;
    pos.z           += smoothstep( hor - 1.0f, hor - 1.5f, vWsPosition.y ) * planeDistance( vPosition, vec4( n, 0.0 ) ) * 1.5;
    //pos.z         -= smoothstep( 1.0f, - 1.5f, vWsPosition.y ) * planeDistance( vPosition, vec4( n, 0.0 ) ) * 0.1;
    */

    // sun
    vec3 sunPos     = uSunDirection;
    float sunDist   = length( sunPos - dir );
    float sunDotV   = max( dot( sunPos, dir ) / sunDist, 0.0 );

    // sun disk     
    float sunDisk   = sunDotV;
    

    // sun low glow and horizon
    sunDisk         += pow( 1.0 - abs(wsDir.y), sunDist * 20.0 ) * 20.0;
    sunDisk         = smoothstep( -5.0, 4.0, sunDisk );
    sunDisk         *= clamp( 10.0*sunDotV, 0.0, 1.0 );

    // stars
    float stars     = pow( noise( fract( vUv * 10.0 ) ), 25.0 ) * 2500.0;
    stars           += pow( noise( fract( -vUv * 15.0 ) ), 20.0 ) * 500.0;
    stars           *= pow( clamp( noise( vUv * 8.0 + vec2( uTime * 0.0125 ) ), 0.0, 1.0 ), 2.0 );
    stars           = clamp( stars, 0.0, 1.0 );
    stars           = stars * smoothstep( 2.1, 0.0, sunDotV );

    // fog
    float rayLength = length( pos );
    vec3 rayDir     = pos / rayLength;
    vec4 fog        = applyFog( rayLength, rayDir * sunDisk );
    vec3 color      = fog.rgb;
    oColor          = color + vec3( stars );


    // pack depth to .rg and object data to .b
    float depth     = gl_FragCoord.z * 256.0;
    float depthX    = floor( depth );
    depth           = ( depth - depthX ) * 256.0;
    float depthY    = floor( depth );
    depthX          *= 0.00390625;
    depthY          *= 0.00390625; 
    oDepthId        = vec3( depthX, depthY, ( 3.0 + step( 0.1, pow( fog.a, 10.0 ) ) ) / 255.0f );
}