#version 150

uniform mat4	ciModelView;
uniform mat4	ciProjectionMatrix;

in vec4			ciPosition;
in vec4			ciColor;
in vec4         aInstanceMat0;
in vec4         aInstanceMat1;
in vec4         aInstanceMat2;
in vec4         aInstanceMat3;
out vec3		vPosition;
out vec4		vColor;

void main(){
    mat4 m;
    m[0] 			= aInstanceMat0;
    m[1] 			= aInstanceMat1;
    m[2] 			= aInstanceMat2;
    m[3]			= aInstanceMat3;

	vColor 			= ciColor;
	vec4 viewPos	= ciModelView * m * ciPosition;
	vPosition		= viewPos.xyz;
	gl_Position		= ciProjectionMatrix * viewPos;
}