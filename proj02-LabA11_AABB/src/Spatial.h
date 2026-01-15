#ifndef __SPATIAL_H__
#define __SPATIAL_H__

#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Ray
{
    glm::vec3 origin;
    glm::vec3 dir;
};

struct AABB
{
    glm::vec3 min, max;
};

// added in LabA07
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct Triangle
{
    glm::vec3 v0, v1, v2;
};

struct HitInfo
{
    float t;
    int triIndex;
};

class Spatial
{

public:
    AABB bbox;

    // this is not the most efficient way, should use some pointers
    // I tried references,but coming with bugs
    std::vector<Vertex> vertexList = std::vector<Vertex>();
    std::vector<unsigned int> triIdxList = std::vector<unsigned int>();
    glm::mat4 matModel;

    Spatial()  { }
    virtual ~Spatial() {}

    virtual void Build(const std::vector<Vertex> & vList, const std::vector<unsigned int> & tIdxList, glm::mat4 mat);    
    void ComputeBounds(AABB &out);
    void InsertTriangles();
    Triangle & getTriangle(int triIdx);

    virtual void Insert(int triIdx) = 0;
    virtual bool Raycast(const Ray &ray, HitInfo &outHit)  = 0;
    virtual void QueryAABB(const AABB &box, std::vector<int> &results) const = 0;
};

bool RayAABB(const glm::vec3 &orig, const glm::vec3 &dir, 
    const glm::vec3 &minB, const glm::vec3 &maxB, float &tmin);

bool RayTriangle(const Ray &ray, const Triangle &tri, float &t);

inline bool AABBIntersects(const AABB& a, const AABB& b)
{
    // If one box is on left side of the other
    if (a.max.x < b.min.x || a.min.x > b.max.x) return false;

    // If one box is in front / behind the other
    if (a.max.y < b.min.y || a.min.y > b.max.y) return false;

    // If one box is above / below the other
    if (a.max.z < b.min.z || a.min.z > b.max.z) return false;

    return true;
}
#endif