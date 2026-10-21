#include "core/Project.h"
#include <stdexcept>
namespace rb {
Project makeRig(const std::string &type) {
    if (type != "R6" && type != "R15")
        throw std::runtime_error("Rig type must be R6 or R15");
    Project p;
    p.rigType = type;
    auto add = [&](std::string name, std::string joint, int parent, glm::vec3 size, glm::vec3 c0,
                   glm::vec3 c1, glm::vec3 color) {
        Part part;
        part.name = std::move(name);
        part.joint = std::move(joint);
        part.parent = parent;
        part.size = size;
        part.c0.position = c0;
        part.c1.position = c1;
        part.color = color;
        p.parts.push_back(part);
    };
    const glm::vec3 skin{.83f, .71f, .52f}, shirt{.42f, .39f, .70f}, pants{.27f, .30f, .40f};
    add("HumanoidRootPart", "World", -1, {2, 2, 1}, {0, 3, 0}, {0, 0, 0}, shirt);
    p.parts[0].visible = false;
    if (type == "R6") {
        add("Torso", "RootJoint", 0, {2, 2, 1}, {0, 0, 0}, {0, 0, 0}, shirt);
        add("Head", "Neck", 1, {2, 1, 1}, {0, 1, 0}, {0, -.5f, 0}, skin);
        add("Left Arm", "Left Shoulder", 1, {1, 2, 1}, {-1, 1, 0}, {.5f, 1, 0}, skin);
        add("Right Arm", "Right Shoulder", 1, {1, 2, 1}, {1, 1, 0}, {-.5f, 1, 0}, skin);
        add("Left Leg", "Left Hip", 1, {1, 2, 1}, {-.5f, -1, 0}, {0, 1, 0}, pants);
        add("Right Leg", "Right Hip", 1, {1, 2, 1}, {.5f, -1, 0}, {0, 1, 0}, pants);
    } else {
        add("LowerTorso", "Root", 0, {2, .4f, 1}, {0, -.8f, 0}, {0, 0, 0}, shirt);
        add("UpperTorso", "Waist", 1, {2, 1.6f, 1}, {0, .2f, 0}, {0, -.8f, 0}, shirt);
        add("Head", "Neck", 2, {2, 1, 1}, {0, .8f, 0}, {0, -.5f, 0}, skin);
        for (int side = 0; side < 2; ++side) {
            float s = side == 0 ? -1.f : 1.f;
            std::string n = side == 0 ? "Left" : "Right";
            int arm = static_cast<int>(p.parts.size());
            add(n + "UpperArm", n + "Shoulder", 2, {1, 1.1f, 1}, {s, .7f, 0}, {-s * .5f, .45f, 0}, skin);
            add(n + "LowerArm", n + "Elbow", arm, {1, .9f, 1}, {0, -.55f, 0}, {0, .45f, 0}, skin);
            add(n + "Hand", n + "Wrist", arm + 1, {1, .3f, 1}, {0, -.45f, 0}, {0, .15f, 0}, skin);
            int leg = static_cast<int>(p.parts.size());
            add(n + "UpperLeg", n + "Hip", 1, {1, 1, 1}, {s * .5f, -.2f, 0}, {0, .5f, 0}, pants);
            add(n + "LowerLeg", n + "Knee", leg, {1, 1, 1}, {0, -.5f, 0}, {0, .5f, 0}, pants);
            add(n + "Foot", n + "Ankle", leg + 1, {1, .3f, 1.4f}, {0, -.5f, 0}, {0, .15f, .2f}, pants);
        }
    }
    p.validate();
    return p;
}
} // namespace rb
