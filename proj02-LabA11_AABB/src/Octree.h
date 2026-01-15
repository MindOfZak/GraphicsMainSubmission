#ifndef __OCTREE_H__
#define __OCTREE_H__

#include "Spatial.h"

// ------------------ Octree ------------------
class Octree : public Spatial
{
public:
    struct Node
    {
        AABB box;
        std::vector<int> tris;
        std::shared_ptr<Node> child[8] = {nullptr};
    };

    std::shared_ptr<Node> root = nullptr;
    int maxDepth = 8;
    int maxPerNode = 16;

    std::shared_ptr<Node> CreateNode(const AABB &box)
    {
        std::shared_ptr<Node> n = std::make_shared<Node>();
        n->box = box;
        return n;
    }

    void Build(const std::vector<Vertex>& vList, const std::vector<unsigned int>& tIdxList, glm::mat4 mat)
    {
        Spatial::Build(vList, tIdxList, mat);

        root = CreateNode(bbox);

        InsertTriangles();
    }

    void Insert(int triIdx) override {
        InsertTri(root, triIdx, 0);
    }

    void Subdivide(std::shared_ptr<Node> n)
    {
        // center
        glm::vec3 c = (n->box.min + n->box.max) * 0.5f;
        glm::vec3 minB = n->box.min;
        glm::vec3 maxB = n->box.max;

        for (int i = 0; i < 8; i++)
        {
            glm::vec3 pMin = {
                (i & 1) ? c.x : minB.x,
                (i & 2) ? c.y : minB.y,
                (i & 4) ? c.z : minB.z};

            glm::vec3 pMax = {
                (i & 1) ? maxB.x : c.x,
                (i & 2) ? maxB.y : c.y,
                (i & 4) ? maxB.z : c.z};
            n->child[i] = CreateNode({pMin, pMax});
        }
    }

    void InsertTri(std::shared_ptr<Node> n, int triIndex, int depth)
    {
        if (depth == maxDepth)
        {
            n->tris.push_back(triIndex);
            return;
        }

        Triangle t = getTriangle(triIndex);
        glm::vec3 triMin = glm::min(t.v0, glm::min(t.v1, t.v2));
        glm::vec3 triMax = glm::max(t.v0, glm::max(t.v1, t.v2));

        if (n->tris.size() < maxPerNode) {
            n->tris.push_back(triIndex);
            return;
        }

        if (n->child[0] == nullptr)
            Subdivide(n);

        for (int i = 0; i < 8; i++)
        {
            const AABB &b = n->child[i]->box;
            if (!(triMax.x < b.min.x || triMin.x > b.max.x ||
                  triMax.y < b.min.y || triMin.y > b.max.y ||
                  triMax.z < b.min.z || triMin.z > b.max.z))
            {
                InsertTri(n->child[i], triIndex, depth + 1);
            }
        }
    }


    bool RaycastNode(std::shared_ptr<Node> n, const Ray &ray, HitInfo &best)
    {
        float t;

        if (!RayAABB(ray.origin, ray.dir, n->box.min, n->box.max, t))
            return false;

        bool hit = false;

        for (int triIdx : n->tris)  {
            float tt;
            Triangle tri = getTriangle(triIdx);
            if (RayTriangle(ray, tri, tt) && tt < best.t)
            {
                best.t = tt;
                best.triIndex = triIdx;
                hit = true;
            }
        }

        for (int i = 0; i < 8; i++)
            if (n->child[i])
                hit |= RaycastNode(n->child[i], ray, best);

        return hit;
    }

    bool Raycast(const Ray &ray, HitInfo &outHit) override
    {
        HitInfo best;
        best.t = FLT_MAX;
        best.triIndex = -1;

        bool result = RaycastNode(root, ray, best);
        if (result)
            outHit = best;
        return result;
    }

    void QueryNode(std::shared_ptr<Node> n, const AABB &box, std::vector<int> &out) const
    {
        if (n == nullptr)
            return;

        // AABB no intersection
        if (n->box.max.x < box.min.x || n->box.min.x > box.max.x ||
            n->box.max.y < box.min.y || n->box.min.y > box.max.y ||
            n->box.max.z < box.min.z || n->box.min.z > box.max.z)
            return;

        out.insert(out.end(), n->tris.begin(), n->tris.end());

        for (int i = 0; i < 8; i++)
            QueryNode(n->child[i], box, out);
    }

    void QueryAABB(const AABB &box, std::vector<int> &out) const override
    {
        QueryNode(root, box, out);
    }
};

#endif