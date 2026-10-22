#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>

namespace rb {
struct Transform {
    glm::vec3 position{0};
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 scale{1};
    glm::mat4 matrix() const;
};
struct MeshVertex {
    glm::vec3 position{0}, normal{0, 1, 0}, color{1};
    glm::vec2 uv{0};
};
struct MeshGeometry {
    std::vector<MeshVertex> vertices;
};
std::shared_ptr<const MeshGeometry> makeClassicHead();
struct Part {
    std::string name;
    std::string joint;
    int parent{-1};
    glm::vec3 size{1};
    Transform c0, c1;
    glm::vec3 color{0.6f, 0.65f, 0.75f};
    std::shared_ptr<const MeshGeometry> mesh;
    bool visible{true};
    bool locked{false};
};
enum class Interpolation { Step, Linear, EaseIn, EaseOut, EaseInOut };
struct Keyframe {
    double time{0};
    Transform value;
    Interpolation interpolation{Interpolation::Linear};
};
struct Track {
    std::string joint;
    std::vector<Keyframe> keys;
};
struct Clip {
    std::string name{"Animation"};
    double duration{2};
    bool loop{true};
    std::vector<Track> tracks;
};
struct Project {
    std::string name{"Untitled"};
    std::string rigType{"R15"};
    std::vector<Part> parts;
    std::vector<Clip> clips{Clip{}};
    void validate() const;
};
Project makeRig(const std::string &type);
Transform evaluate(const Track &track, double time);
std::vector<Transform> evaluatePose(const Project &, std::size_t clip, double time);
std::vector<glm::mat4> worldMatrices(const Project &, const std::vector<Transform> &pose);
void setKey(Clip &, const std::string &joint, Keyframe);
bool deleteKey(Clip &, const std::string &joint, double time);
} // namespace rb
