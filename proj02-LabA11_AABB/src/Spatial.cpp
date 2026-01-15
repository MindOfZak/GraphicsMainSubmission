
#include "Spatial.h"

void Spatial::ComputeBounds(AABB &out)
{
    glm::vec3 minB(FLT_MAX), maxB(-FLT_MAX);

    for (int i = 0; i < triIdxList.size()/3; i++)
    {
        Triangle t = getTriangle(i);

        minB = glm::min(minB, glm::min(t.v0, glm::min(t.v1, t.v2)));
        maxB = glm::max(maxB, glm::max(t.v0, glm::min(t.v1, t.v2)));
    }
    out = {minB, maxB};
}

void Spatial::Build(const std::vector<Vertex> & vList, const std::vector<unsigned int> & tIdxList, glm::mat4 mat)
{
    vertexList = vList;
    triIdxList = tIdxList;
    matModel = mat;
    ComputeBounds(bbox);
}

Triangle & Spatial::getTriangle(int triIdx)
{
    int idx = triIdx * 3;
    glm::vec3 v0 = vertexList[triIdxList[idx]].pos;
    glm::vec3 v1 = vertexList[triIdxList[idx+1]].pos;
    glm::vec3 v2 = vertexList[triIdxList[idx+2]].pos;

    v0 = glm::vec3( matModel * glm::vec4(v0, 1.0));
    v1 = glm::vec3( matModel * glm::vec4(v1, 1.0));
    v2 = glm::vec3( matModel * glm::vec4(v2, 1.0));

    return Triangle{v0, v1, v2};
}

void Spatial::InsertTriangles()
{
    for (int i = 0; i < triIdxList.size() / 3; i++)
        Insert(i);
}

bool RayAABB(const glm::vec3 &orig, const glm::vec3 &dir, const glm::vec3 &minB, const glm::vec3 &maxB, float &tmin)
{
    float t1 = (minB.x - orig.x) / dir.x;
    float t2 = (maxB.x - orig.x) / dir.x;
    float t3 = (minB.y - orig.y) / dir.y;
    float t4 = (maxB.y - orig.y) / dir.y;
    float t5 = (minB.z - orig.z) / dir.z;
    float t6 = (maxB.z - orig.z) / dir.z;

    float tminCandidate = std::max({std::min(t1, t2), std::min(t3, t4), std::min(t5, t6)});
    float tmaxCandidate = std::min({std::max(t1, t2), std::max(t3, t4), std::max(t5, t6)});

    if (tmaxCandidate < 0 || tminCandidate > tmaxCandidate)
        return false;
    tmin = tminCandidate;
    return true;
}

bool RayTriangle(const Ray &ray, const Triangle &tri, float &t)
{
    const float EPS = 1e-6f;
    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 pvec = glm::cross(ray.dir, edge2);
    float det = glm::dot(edge1, pvec);
    if (fabs(det) < EPS)
        return false;
    float invDet = 1.0f / det;

    glm::vec3 tvec = ray.origin - tri.v0;
    float u = glm::dot(tvec, pvec) * invDet;
    if (u < 0 || u > 1)
        return false;

    glm::vec3 qvec = glm::cross(tvec, edge1);
    float v = glm::dot(ray.dir, qvec) * invDet;
    if (v < 0 || u + v > 1)
        return false;

    t = glm::dot(edge2, qvec) * invDet;
    return t > EPS;
}