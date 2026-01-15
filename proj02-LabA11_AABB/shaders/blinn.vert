// Blinn-Phong and Phong shared vertex shader
#version 410

in layout(location=0) vec3 aPos;
in layout(location=1) vec3 aNormal;

// M V P matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// out vec3 colour_vert;
out vec3 normal;
out vec3 fragPos;

void main()
{
    // homogeneous coordinate
    gl_Position = projection * view * model * vec4(aPos, 1.0); 
    
    fragPos = vec3(model * vec4(aPos, 1.0));

    // convert 4x4 modelling matrix to 3x3
    mat3 normalMatrix = mat3(model);
    
    // output normal to the fragment shader
    // !! only correct for rigid body transforms
    normal = normalMatrix * aNormal;
}