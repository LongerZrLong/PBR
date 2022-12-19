#version 330 core

uniform mat4 uProjMatrix;
uniform mat4 uViewMatrix;

layout (location = 0) in vec3 aPosition;

out vec3 vWorldPos;

void main()
{
    vWorldPos = aPosition;
    gl_Position =  uProjMatrix * uViewMatrix * vec4(vWorldPos, 1.0);
}
