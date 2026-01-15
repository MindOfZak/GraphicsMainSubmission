#version 410

out vec4 colour_out;
in vec3 colour_vert;

void main()
{
    // RGBA
    //colour_out = vec4(1.0, 0.0, 0.0, 1.0);
    colour_out = vec4(colour_vert, 1.0);
}