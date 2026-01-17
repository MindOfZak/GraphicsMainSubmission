#ifndef __MESH_H__
#define __MESH_H__

#include <iostream>
#include <vector>
#include <string>
#include <memory>

#include <glad/glad.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include <assimp/material.h>

// LabA11
#include "Spatial.h"

// Texture + Material structs you already had
struct Texture {
    GLuint id = 0;
    std::string type;
};

struct Material {
    glm::vec3 Diffuse;
    glm::vec3 Specular;
    glm::vec3 Ambient;
    

    float Shininess = 0.0f;
};

class Mesh
{
protected:
    // For spatial/picking/collision (one combined set)
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Keep a list of all loaded textures so mipmap toggle can apply to them
    std::vector<Texture> textures;

    // OpenGL buffers for drawing per-submesh
    struct SubMesh
    {
        glm::vec3 kd = glm::vec3(1.0f);
        bool hasTexture = false;
        std::vector<Vertex> v;
        std::vector<unsigned int> i;

        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;

        GLuint diffuseTex = 0;   // texture id for this submesh (diffuse)
        GLsizei indexCount = 0;
    };

    std::vector<SubMesh> subMeshes;

    GLuint shaderId = 0;
    bool bPicked = false;

    void initBuffer();

    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, std::string dir);
    unsigned int loadTextureAndBind(const char* path, const std::string& directory);
    Material loadMaterial(aiMaterial* mat);

public:
    std::unique_ptr<Spatial> pSpatial = nullptr;

    Mesh();
    ~Mesh();

    void init(std::string path, GLuint shaderId);
    void loadModel(std::string path);

    void initSpatial(bool useOctree, glm::mat4 mat);

    void setShaderId(GLuint sid);

    void setPicked(bool b) { bPicked = b; }

    void draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj);
    void setUseMipmaps(bool enabled);
};

#endif
