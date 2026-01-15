#ifndef __GRID_H__
#define __GRID_H__

#include "Spatial.h"

// ------------------ Uniform Grid ------------------
class Grid : public Spatial
{
public:
    glm::ivec3 dims;

    glm::vec3 cellSize;
    std::vector<std::vector<int>> cells;

    Grid(glm::ivec3 dims = {16, 16, 16}) : dims(dims) {}

    void Build(const std::vector<Vertex> & vList, const std::vector<unsigned int> & tIdxList, glm::mat4 mat) 
    {
        Spatial::Build(vList, tIdxList, mat);

        cellSize = (bbox.max - bbox.min) / glm::vec3(dims);

        // init cell grid
        int size = dims.x * dims.y * dims.z;
        cells = std::vector<std::vector<int>>(size);
        for (int i = 0; i < size; i++)
            cells[i] = std::vector<int>();

        InsertTriangles();   
    }

    glm::ivec3 PosToCell(const glm::vec3 &p) const
    {
        glm::vec3 local = (p - bbox.min) / cellSize;
        return glm::clamp(glm::ivec3(local), glm::ivec3(0), dims - glm::ivec3(1));
    }

    void Insert(int triIdx) override
    {
        Triangle t = getTriangle(triIdx);

        glm::vec3 triMin = glm::min(t.v0, glm::min(t.v1, t.v2));
        glm::vec3 triMax = glm::max(t.v0, glm::max(t.v1, t.v2));

        glm::ivec3 minCell = PosToCell(triMin);
        glm::ivec3 maxCell = PosToCell(triMax);

        for (int z = minCell.z; z <= maxCell.z; z++)
            for (int y = minCell.y; y <= maxCell.y; y++)
                for (int x = minCell.x; x <= maxCell.x; x++)
                {
                    int idx = x + dims.x * (y + dims.y * z);
                    cells[idx].push_back(triIdx);
                }
    }

    
    bool Raycast(const Ray &ray, HitInfo &outHit) override
    {
        float tHit;
        if (!RayAABB(ray.origin, ray.dir, bbox.min, bbox.max, tHit))
            return false;

        // 3D DDA
        glm::vec3 p = ray.origin + ray.dir * std::max(0.0f, tHit);
        glm::ivec3 cell = PosToCell(p);

        glm::vec3 step = glm::sign(ray.dir);
        glm::vec3 tDelta = glm::abs(cellSize / ray.dir);
        glm::vec3 next;
        next.x = ((cell.x + (step.x > 0)) * cellSize.x + bbox.min.x - p.x) / ray.dir.x;
        next.y = ((cell.y + (step.y > 0)) * cellSize.y + bbox.min.y - p.y) / ray.dir.y;
        next.z = ((cell.z + (step.z > 0)) * cellSize.z + bbox.min.z - p.z) / ray.dir.z;

        float bestT = FLT_MAX;
        int bestIdx = -1;

        while (cell.x >= 0 && cell.y >= 0 && cell.z >= 0 &&
               cell.x < dims.x && cell.y < dims.y && cell.z < dims.z)
        {
            int idx = cell.x + dims.x * (cell.y + dims.y * cell.z);
            for (int triIdx : cells[idx]) {
                float t;
                Triangle tri = Spatial::getTriangle(triIdx);

                if (RayTriangle(ray, tri, t)) {
                    if (t < bestT) {
                        bestT = t;
                        bestIdx = triIdx;
                    }
                }
            }

            if (next.x < next.y) {
                if (next.x < next.z) {
                    cell.x += (int)step.x;
                    next.x += tDelta.x;
                } else {
                    cell.z += (int)step.z;
                    next.z += tDelta.z;
                }
            } else {
                if (next.y < next.z) {
                    cell.y += (int)step.y;
                    next.y += tDelta.y;
                } else {
                    cell.z += (int)step.z;
                    next.z += tDelta.z;
                }
            }
        }

        if (bestIdx >= 0) {
            outHit = {bestT, bestIdx};
            return true;
        }
        return false;
    }

    void QueryAABB(const AABB &box, std::vector<int> &out) const override
    {
        if (!AABBIntersects(box, bbox))
            return;

        glm::ivec3 minC = PosToCell(box.min);
        glm::ivec3 maxC = PosToCell(box.max);

        for (int z = minC.z; z <= maxC.z; z++)
            for (int y = minC.y; y <= maxC.y; y++)
                for (int x = minC.x; x <= maxC.x; x++)
                {
                    int idx = x + dims.x * (y + dims.y * z);
                    out.insert(out.end(), cells[idx].begin(), cells[idx].end());
                }
    }
};

#endif __GRID_H__