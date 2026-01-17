#include <iostream>
#include <unordered_map>

#include <glad/glad.h>

#include "Mesh.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Grid.h"
#include "Octree.h"

// ------------------------------------------------------------
// 1x1 white fallback texture (prevents black when no diffuse map)
// ------------------------------------------------------------
static GLuint gWhiteTex = 0;

static GLuint GetWhiteTexture()
{
    if (gWhiteTex != 0) return gWhiteTex;

    unsigned char white[3] = { 255, 255, 255 };

    glGenTextures(1, &gWhiteTex);
    glBindTexture(GL_TEXTURE_2D, gWhiteTex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    return gWhiteTex;
}

// ------------------------------------------------------------
// Texture cache so the same png/jpg isn't loaded multiple times
// ------------------------------------------------------------
static std::unordered_map<std::string, GLuint> gTextureCache;

Mesh::Mesh() {}

Mesh::~Mesh()
{
    // Cleanup GL buffers per submesh
    for (auto& sm : subMeshes)
    {
        if (sm.ebo) glDeleteBuffers(1, &sm.ebo);
        if (sm.vbo) glDeleteBuffers(1, &sm.vbo);
        if (sm.vao) glDeleteVertexArrays(1, &sm.vao);
        sm.vao = sm.vbo = sm.ebo = 0;
    }
}

void Mesh::init(std::string path, GLuint id)
{
    shaderId = id;
    loadModel(path);
    initBuffer();
}

void Mesh::initSpatial(bool useOctree, glm::mat4 mat)
{
    if (useOctree)
        pSpatial = std::make_unique<Octree>();
    else
        pSpatial = std::make_unique<Grid>(glm::ivec3(32));

    // Spatial uses combined vertices/indices (covers all submeshes)
    pSpatial->Build(vertices, indices, mat);
}

static std::string GetDirectoryFromPath(const std::string& path)
{
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return "";
    return path.substr(0, slash);
}

void Mesh::loadModel(std::string path)
{
    vertices.clear();
    indices.clear();
    textures.clear();
    subMeshes.clear();

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals
    );

    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0)
    {
        std::cout << "load model failed: " << path << "\n";
        std::cout << "Assimp error: " << importer.GetErrorString() << "\n";
        return;
    }

    std::cout << "load model successful: " << path << "\n";

    const std::string dir = GetDirectoryFromPath(path);

    // Build per-submesh CPU data + combined data for spatial
    for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++)
    {
        aiMesh* mesh = scene->mMeshes[mi];
        if (!mesh) continue;

        SubMesh sm;

        // Defaults
        sm.diffuseTex = 0;
        sm.kd = glm::vec3(1.0f);
        sm.hasTexture = false;

        // Material -> Kd + diffuse texture for this submesh
        if ((int)mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            if (material)
            {
                // Read diffuse colour (Kd)
                aiColor3D kd(1.f, 1.f, 1.f);
                material->Get(AI_MATKEY_COLOR_DIFFUSE, kd);
                sm.kd = glm::vec3(kd.r, kd.g, kd.b);

                // Load diffuse texture (map_Kd)
                std::vector<Texture> diffuseMaps =
                    loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", dir);

                if (!diffuseMaps.empty())
                {
                    sm.diffuseTex = diffuseMaps[0].id;
                    sm.hasTexture = (sm.diffuseTex != 0);

                    // Keep for mipmap toggle support
                    textures.push_back(diffuseMaps[0]);
                }
            }
        }

        // Submesh vertices
        sm.v.reserve(mesh->mNumVertices);
        for (unsigned int v = 0; v < mesh->mNumVertices; v++)
        {
            Vertex out;
            out.pos = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);

            if (mesh->HasNormals())
                out.normal = glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
            else
                out.normal = glm::vec3(0, 1, 0);

            if (mesh->mTextureCoords[0])
                out.texCoord = glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
            else
                out.texCoord = glm::vec2(0.0f);

            sm.v.push_back(out);
        }

        // Submesh indices (local)
        for (unsigned int f = 0; f < mesh->mNumFaces; f++)
        {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned int k = 0; k < face.mNumIndices; k++)
                sm.i.push_back(face.mIndices[k]);
        }

        sm.indexCount = (GLsizei)sm.i.size();
        subMeshes.push_back(std::move(sm));

        // Append into combined arrays for Spatial (with offsets)
        unsigned int vOffset = (unsigned int)vertices.size();
        vertices.insert(vertices.end(), subMeshes.back().v.begin(), subMeshes.back().v.end());

        for (unsigned int idx : subMeshes.back().i)
            indices.push_back(idx + vOffset);
    }

    std::cout << "combined vertices: " << vertices.size() << "\n";
    std::cout << "combined indices:  " << indices.size() << "\n";
    std::cout << "submeshes:         " << subMeshes.size() << "\n";
}

void Mesh::initBuffer()
{
    // Build VAO/VBO/EBO for each submesh
    for (auto& sm : subMeshes)
    {
        glGenVertexArrays(1, &sm.vao);
        glGenBuffers(1, &sm.vbo);
        glGenBuffers(1, &sm.ebo);

        glBindVertexArray(sm.vao);

        glBindBuffer(GL_ARRAY_BUFFER, sm.vbo);
        glBufferData(GL_ARRAY_BUFFER, sm.v.size() * sizeof(Vertex), sm.v.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sm.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sm.i.size() * sizeof(unsigned int), sm.i.data(), GL_STATIC_DRAW);

        // layout:
        // 0 = position (vec3)
        // 1 = normal   (vec3)
        // 2 = uv       (vec2)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(float) * 3));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

        glBindVertexArray(0);
    }
}

void Mesh::setShaderId(GLuint sid)
{
    shaderId = sid;
}

std::vector<Texture> Mesh::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, std::string dir)
{
    std::vector<Texture> out;

    const unsigned int nTex = mat->GetTextureCount(type);
    for (unsigned int i = 0; i < nTex; i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);

        Texture t;
        t.type = typeName;

        std::string rel = str.C_Str();
        std::string full = dir.empty() ? rel : (dir + "/" + rel);

        auto it = gTextureCache.find(full);
        if (it != gTextureCache.end())
        {
            t.id = it->second;
        }
        else
        {
            t.id = loadTextureAndBind(rel.c_str(), dir);
            if (t.id != 0)
                gTextureCache[full] = t.id;
        }

        if (t.id != 0)
            out.push_back(t);
    }

    return out;
}

void Mesh::setUseMipmaps(bool enabled)
{
    for (auto& t : textures)
    {
        if (t.id == 0) continue;

        glBindTexture(GL_TEXTURE_2D, t.id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (enabled)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        else
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

unsigned int Mesh::loadTextureAndBind(const char* path, const std::string& directory)
{
    std::string filename = std::string(path);
    if (!directory.empty())
        filename = directory + '/' + filename;

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (!data)
    {
        std::cout << "Texture failed to load at path: " << filename << "\n";
        return 0;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);

    GLenum format;
    if (nrComponents == 1) format = GL_RED;
    else if (nrComponents == 3) format = GL_RGB;
    else format = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

Material Mesh::loadMaterial(aiMaterial* mat)
{
    Material material;
    aiColor3D color(0.f, 0.f, 0.f);
    float shininess = 0.0f;

    mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
    material.Diffuse = glm::vec3(color.r, color.g, color.b);

    mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
    material.Ambient = glm::vec3(color.r, color.g, color.b);

    mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
    material.Specular = glm::vec3(color.r, color.g, color.b);

    mat->Get(AI_MATKEY_SHININESS, shininess);
    material.Shininess = shininess;

    return material;
}

void Mesh::draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj)
{
    glUseProgram(shaderId);

    GLuint model_loc = glGetUniformLocation(shaderId, "model");
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, &matModel[0][0]);

    GLuint view_loc = glGetUniformLocation(shaderId, "view");
    glUniformMatrix4fv(view_loc, 1, GL_FALSE, &matView[0][0]);

    GLuint projection_loc = glGetUniformLocation(shaderId, "projection");
    glUniformMatrix4fv(projection_loc, 1, GL_FALSE, &matProj[0][0]);

    glUniform1i(glGetUniformLocation(shaderId, "bPicked"), bPicked);

    // texture unit 0
    glUniform1i(glGetUniformLocation(shaderId, "textureMap"), 0);
    glActiveTexture(GL_TEXTURE0);

    // Cache uniform locations (tiny perf + avoids repeated lookups)
    GLint kdLoc = glGetUniformLocation(shaderId, "materialDiffuse");
    GLint hasTexLoc = glGetUniformLocation(shaderId, "hasTexture");

    // Draw each submesh with its own diffuse texture + Kd tint
    for (const auto& sm : subMeshes)
    {
        if (kdLoc >= 0) glUniform3fv(kdLoc, 1, &sm.kd[0]);
        if (hasTexLoc >= 0) glUniform1i(hasTexLoc, sm.hasTexture ? 1 : 0);

        if (sm.diffuseTex != 0)
            glBindTexture(GL_TEXTURE_2D, sm.diffuseTex);
        else
            glBindTexture(GL_TEXTURE_2D, GetWhiteTexture());

        glBindVertexArray(sm.vao);
        glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}



