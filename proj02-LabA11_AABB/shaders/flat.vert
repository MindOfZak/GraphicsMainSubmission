// !! This shader is NOT correct
// DO NOT use it

#version 410

in layout(location=0) vec3 pos;
in layout(location=1) vec3 aNormal;

// Model View Projection
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// !! don't forget the "flat" modifier
// It disables interpolation across the triangle
flat out vec3 normal;
out vec3 fragPos;

void main()
{
    // homogeneous coordinate
    gl_Position = projection * view * model * vec4(pos, 1.0); 
    
    fragPos = vec3(model * vec4(pos, 1.0));
    
    // convert 4x4 modelling matrix to 3x3
    mat3 normalMatrix = mat3(model);

    // output normal to the fragment shader
    // !! only correct for rigid body transforms
    normal = normalMatrix * aNormal;
}