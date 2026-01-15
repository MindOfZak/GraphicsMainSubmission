// =============================================================
// NOTICE: For simplicity, we are not using SceneGraph in LabA11 

#include "Node.h"

void Node::addChild(std::shared_ptr<Node> child, glm::mat4 trans, glm::mat4 rot )
{
    childNodes.push_back(child);
    childMats.push_back(trans * rot);
}

void Node::addMesh(std::shared_ptr<Mesh> mesh, glm::mat4 trans, glm::mat4 rot, glm::mat4 scale)
{
    meshes.push_back(mesh);
    meshMats.push_back(trans * rot * scale);
}

// all drawings go to Mesh::draw()
void Node::draw(glm::mat4 matModel, glm::mat4 matView, glm::mat4 matProj)
{
    //std::cout << "Node::draw()" << std::endl;

    for (int i = 0; i < meshes.size(); i++) {
        meshes[i]->draw(matModel * meshMats[i], matView, matProj);
    }

    // for (const auto& child : childNodes) {
    for (int i = 0; i < childNodes.size(); i++) {
        childNodes[i]->draw(matModel * childMats[i], matView, matProj);
    }
}