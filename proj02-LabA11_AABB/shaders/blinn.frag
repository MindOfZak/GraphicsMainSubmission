#version 410

// for lighting
in vec3 fragPos;
in vec3 normal;

uniform vec3 lightPos;
uniform vec3 viewPos;

out vec4 colour_out;

void main()
{
    // RGBA
    //colour_out = vec4(1.0, 0.0, 0.0, 1.0);
    //colour_out = vec4(colour_vert, 1.0);

    // manually set R G B of the surface colour, here is RED
    vec3 colour = vec3(1.0, 0.0, 0.0);

    // 1. ambient
    vec3 ambient = 0.05 * colour;

    // 2. diffuse
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 norm = normalize(normal);
    float diff = max(dot(lightDir, norm), 0.0);
    vec3 diffuse = diff * colour;
    
    // 3. specular
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    
    // the shininess coefficient beta = 32.0 
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);

    // assuming a light source with a bright white colour
    vec3 specular = vec3(0.3) * spec;

    // The final output fragment colour 
    // combination of ambient, diffuse and specular
    colour_out = vec4(ambient + diffuse + specular, 1.0);
}