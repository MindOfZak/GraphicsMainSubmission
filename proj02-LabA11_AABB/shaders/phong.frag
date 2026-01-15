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

    // manually set the vertex colour
    vec3 color = vec3(1.0, 0.0, 0.0);

    // 1. ambient
    vec3 ambient = 0.05 * color;

    // 2. diffuse
    vec3 lightDir = normalize(lightPos - fragPos);
    vec3 norm = normalize(normal);
    float diff = max(dot(lightDir, norm), 0.0);
    vec3 diffuse = diff * color;
    
    // 3. specular
    vec3 viewDir = normalize(viewPos - fragPos);
    // reflectDir = -lightDir - 2( (-lightDir) dot norm)norm
    // you need to make reflectDir normalised
    vec3 reflectDir = reflect(-lightDir, norm);

    // the shininess coefficient alpha = 8.0 
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);

    // assuming a light source with a bright white colour
    vec3 specular = vec3(0.3) * spec;

    // The final combined colour
    colour_out = vec4(ambient + diffuse + specular, 1.0);
}