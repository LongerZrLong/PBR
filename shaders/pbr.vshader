#version 330 core

uniform mat4 uProjMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uModelMatrix;

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec2 vTexCoord;
out vec3 vWorldPos;
out vec3 vNormal;

void main()
{
    vTexCoord = aTexCoord;
    vWorldPos = vec3(uModelMatrix * vec4(aPosition, 1.0));
    vNormal = mat3(uModelMatrix) * aNormal;

    gl_Position =  uProjMatrix * uViewMatrix * vec4(vWorldPos, 1.0);
}