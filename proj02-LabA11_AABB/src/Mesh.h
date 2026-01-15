#ifndef __MESH_H__
#define __MESH_H__

#include <iostream>
#include <vector>

#include <glad/glad.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

// added in LabA07
// ==============================================
#include <assimp/material.h>

// added in LabA11
#include "Spatial.h"



// added in LabA07
struct Texture {
    GLuint id;
    std::string type;
};

// added in LabA07
struct Material {
    glm::vec3 Diffuse;
    glm::vec3 Specular;
    glm::vec3 Ambient;
    float Shininess;
};

// ==============================================


class Mesh {

protected:
    // changed in LabA07
    // array of vertices and normals
    //std::vector< glm::vec3 > vertices; 
    std::vector<Vertex> vertices;

    // triangle vertex indices
    std::vector< unsigned int > indices;

    // added in LabA07
    std::vector<Texture> textures;

    // Material material;
    
    std::vector<GLuint> buffers;

    // this will be Material in the future
    GLuint shaderId;

    // added in LabA 11
    bool bPicked = false;
    
    void initBuffer();

    // added in LabA07
    std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, std::string dir);
    unsigned int loadTextureAndBind(const char* path, const std::string& directory);
    
    Material Mesh::loadMaterial(aiMaterial* mat);

public:

    std::unique_ptr<Spatial> pSpatial = nullptr;

    Mesh();
    ~Mesh();

    void init(std::string path, GLuint shaderId);
    void loadModel(std::string path);

    void initSpatial(bool useOctree, glm::mat4 mat);

    void setShaderId(GLuint sid);

    // added in LabA 11
    void setPicked(bool b) { bPicked = b; }
    
    void draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj);
};

#endif