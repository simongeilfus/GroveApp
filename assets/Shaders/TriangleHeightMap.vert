#include "Shaders/Common.glsl"

uniform mat4 ciModelViewProjection;

in vec4 ciPosition;
in vec2 ciTexCoord0;

uniform sampler2D	uHeightMap;

out float vPosY;
void main(){
  vec2 uv 		= ciTexCoord0.st;
  float height	= texture( uHeightMap, uv ).r;
  vec4 position = ciPosition;
  position.y	+= height;
  vPosY			= position.y;
  gl_Position	= ciModelViewProjection * position;
}