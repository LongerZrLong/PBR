#version 330 core

uniform samplerCube uSkyBox;

in vec3 vTexCoord;

out vec4 FragColor;

void main()
{
    vec3 envColor = texture(uSkyBox, vTexCoord).rgb;

    // HDR tonemap and gamma correct
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2));

    FragColor = vec4(envColor, 1.0);
}
