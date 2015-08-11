#include "Shaders/Common.glsl"

uniform mat4	ciModelView;
uniform mat4	ciProjectionMatrix;

in vec4			ciPosition;
in vec4			ciColor;
in vec2			ciTexCoord0;
in vec3         ciNormal;

out vec2		vUv;
out vec3		vPosition;
out vec3		vWsPosition;
out vec3        vNormal;
out vec4		vColor;

void main(){
    vUv				= ciTexCoord0;
    vColor			= ciColor;
	vNormal			= ciNormal;
	vec4 viewPos	= ciModelView * ciPosition;
	vWsPosition		= ciPosition.xyz;
	vPosition		= viewPos.xyz;
	gl_Position		= ciProjectionMatrix * viewPos;
}