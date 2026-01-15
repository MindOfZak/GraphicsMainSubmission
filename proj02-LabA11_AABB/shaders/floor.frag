#version 330 core
out vec4 FragColor;

uniform vec3 floorColor;

void main()
{
    FragColor = vec4(floorColor, 1.0);
}

