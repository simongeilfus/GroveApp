#include "Shaders/Common.glsl"

uniform mat4    ciModelViewProjection;
in vec4         ciPosition;
in vec2         ciTexCoord0;

out vec2 		vUV;

void main(){
	vUV			= ciTexCoord0;
    gl_Position	= ciModelViewProjection * ciPosition;
}
