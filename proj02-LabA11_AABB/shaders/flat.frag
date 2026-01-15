// !! This shader is NOT correct
// DO NOT use it
#version 410

// !! don't forget the "flat" modifier
// It disables interpolation across the triangle
flat in vec3 normal;
in vec3 fragPos;

uniform vec3 lightPos;

// not needed for flat shading
//uniform vec3 viewPos;

out vec4 colour_out;

void main()
{

    // manually set the vertex colour
    vec3 colour = vec3(1.0, 0.0, 0.0);

    // 1. ambient
    vec3 ambient = 0.05 * colour;

    // 2. diffuse
    vec3 lightDir = normalize(lightPos);
    vec3 norm = normalize(normal);
    float diff = max(dot(lightDir, norm), 0.0);
    vec3 diffuse = diff * colour;

    // The final combined colour
    colour_out = vec4(ambient + diffuse, 1.0);
}