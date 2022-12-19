#version 330 core

uniform mat4 uProjMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uModelMatrix;

layout (location = 0) in vec3 aPosition;

void main() {
    // send position (eye coordinates) to fragment shader
    vec4 tPosition = uViewMatrix * uModelMatrix * vec4(aPosition, 1.0);
    gl_Position = uProjMatrix * tPosition;
}
