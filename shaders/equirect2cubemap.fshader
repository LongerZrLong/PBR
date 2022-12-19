#version 330 core

const vec2 invAtan = vec2(0.1591, 0.3183);

uniform sampler2D uEquirectangularMap;

in vec3 vWorldPos;

out vec4 FragColor;

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(vWorldPos));
    vec3 color = texture(uEquirectangularMap, uv).rgb;

    FragColor = vec4(color, 1.0);
}
