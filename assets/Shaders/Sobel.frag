#include "Shaders/Common.glsl"

out vec4            oColor;

uniform sampler2D   uReadTexture;
uniform vec2        uInvSize;

in vec2             vUV;

void main(){
    vec4 horizEdge = vec4( 0.0 );
    horizEdge -= texture( uReadTexture, vec2( vUV.x - uInvSize.x, vUV.y - uInvSize.y ) )    * 1.0;
    horizEdge -= texture( uReadTexture, vec2( vUV.x - uInvSize.x, vUV.y     ) )             * 2.0;
    horizEdge -= texture( uReadTexture, vec2( vUV.x - uInvSize.x, vUV.y + uInvSize.y ) )    * 1.0;
    horizEdge += texture( uReadTexture, vec2( vUV.x + uInvSize.x, vUV.y - uInvSize.y ) )    * 1.0;
    horizEdge += texture( uReadTexture, vec2( vUV.x + uInvSize.x, vUV.y     ) )             * 2.0;
    horizEdge += texture( uReadTexture, vec2( vUV.x + uInvSize.x, vUV.y + uInvSize.y ) )    * 1.0;
    vec4 vertEdge = vec4( 0.0 );
    vertEdge -= texture( uReadTexture, vec2( vUV.x - uInvSize.x, vUV.y - uInvSize.y ) )     * 1.0;
    vertEdge -= texture( uReadTexture, vec2( vUV.x             , vUV.y - uInvSize.y ) )     * 2.0;
    vertEdge -= texture( uReadTexture, vec2( vUV.x + uInvSize.x, vUV.y - uInvSize.y ) )     * 1.0;
    vertEdge += texture( uReadTexture, vec2( vUV.x - uInvSize.x, vUV.y + uInvSize.y ) )     * 1.0;
    vertEdge += texture( uReadTexture, vec2( vUV.x             , vUV.y + uInvSize.y ) )     * 2.0;
    vertEdge += texture( uReadTexture, vec2( vUV.x + uInvSize.x, vUV.y + uInvSize.y ) )     * 1.0;

    vec3 edge = sqrt((horizEdge.rgb * horizEdge.rgb) + (vertEdge.rgb * vertEdge.rgb));
    
	oColor		= vec4( vec3( edge ), 1.0f );
}
