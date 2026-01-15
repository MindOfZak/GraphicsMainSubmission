// =============================================================
// NOTICE: For simplicity, we are not using SceneGraph in LabA11 

#ifndef __NODE_H__
#define __NODE_H__


#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "Mesh.h"


class Node {
private:
    // actually we don't need a parent at present
    //std::vector< std::shared_ptr <Node> > parent;

    // Note: separating child nodes and transforms is not a good design
    // the code is only for concept demonstration
    std::vector< std::shared_ptr <Node> > childNodes;
    std::vector< glm::mat4 > childMats;

    std::vector< std::shared_ptr <Mesh> > meshes;
    std::vector< glm::mat4 > meshMats;

public:
    void addChild(std::shared_ptr<Node> child, glm::mat4 trans = glm::mat4(1.0), glm::mat4 rot = glm::mat4(1.0));
    void addMesh(std::shared_ptr<Mesh> mesh, glm::mat4 trans = glm::mat4(1.0), glm::mat4 rot = glm::mat4(1.0), glm::mat4 scale = glm::mat4(1.0));

    void draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj);

};

#endif