#include "Shaders/Common.glsl"

in float vPosY;
out vec4 oHeight;

void main(){
    oHeight = vec4( vec3( vPosY ), 1.0 );
}