#version 330 core

uniform mat4 uProjMatrix;
uniform mat4 uViewMatrix;

layout (location = 0) in vec3 aPosition;

out vec3 vTexCoord;

void main()
{
    vTexCoord = aPosition;

    mat4 rotView = mat4(mat3(uViewMatrix));
    vec4 pos = uProjMatrix * rotView * vec4(aPosition, 1.0);

    gl_Position = pos.xyww;
}
