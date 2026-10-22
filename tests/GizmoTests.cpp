#include "modeling/TransformGizmo.h"
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>
using namespace rb;
namespace {
Ray through(glm::vec3 point) {
    return {point + glm::vec3(0, 0, 5), {0, 0, -1}};
}
void nearVector(glm::vec3 a, glm::vec3 b) {
    for (int i = 0; i < 3; ++i)
        EXPECT_NEAR(a[i], b[i], 1e-4f);
}
} // namespace
TEST(Gizmo, PickingAndParallelRays) {
    GizmoFrame frame{{0, 0, 0}, {1, 0, 0, 0}, 1};
    auto ring = hitGizmo(through({1, 0, 0}), frame, TransformTool::Rotate, .05f);
    ASSERT_TRUE(ring);
    EXPECT_EQ(ring->axis, 2);
    auto move = hitGizmo(through({.7f, .02f, 0}), frame, TransformTool::Move, .05f);
    ASSERT_TRUE(move);
    EXPECT_EQ(move->axis, 0);
    EXPECT_FALSE(hitGizmo(through({.7f, .2f, 0}), frame, TransformTool::Move, .05f));
    EXPECT_FALSE(axisParameter({{0, 0, 5}, {0, 0, -1}}, {0, 0, 0}, {0, 0, 1}));
    EXPECT_FALSE(intersectPlane({{0, 0, -5}, {0, 0, -1}}, {0, 0, 0}, {0, 0, 1}));
    EXPECT_FALSE(intersectPlane({{0, 0, 5}, {1, 0, 0}}, {0, 0, 0}, {0, 0, 1}));
}
TEST(Gizmo, JointPivotIncludesC1AndAnimatedOffset) {
    auto p = makeRig("R15");
    auto pose = evaluatePose(p, 0, 0);
    p.parts[4].c1.rotation = glm::angleAxis(.5f, glm::vec3(0, 0, 1));
    pose[4].position = {.2f, .3f, 0};
    pose[4].rotation = glm::angleAxis(.4f, glm::vec3(1, 0, 0));
    auto world = worldMatrices(p, pose);
    auto frame = makeGizmoFrame(p, pose, 4, TransformTool::Rotate, TransformSpace::Local, 1);
    auto joint = world[4] * p.parts[4].c1.matrix();
    nearVector(frame.origin, glm::vec3(joint[3]));
    nearVector(frame.axis(0), glm::vec3(joint * glm::vec4(1, 0, 0, 0)));
    auto resize = makeGizmoFrame(p, pose, 4, TransformTool::Resize, TransformSpace::World, 1);
    nearVector(resize.origin, glm::vec3(world[4][3]));
    nearVector(resize.axis(0), glm::vec3(world[4] * glm::vec4(1, 0, 0, 0)));
}
TEST(Gizmo, WorldTranslationConvertsMotorBasis) {
    auto p = makeRig("R15");
    auto pose = evaluatePose(p, 0, 0);
    pose[2].rotation = glm::angleAxis(.6f, glm::vec3(0, 0, 1));
    p.parts[10].c0.rotation = glm::angleAxis(.4f, glm::vec3(0, 1, 0));
    pose[10].position = {.3f, .2f, .1f};
    auto before = worldMatrices(p, pose);
    auto frame = makeGizmoFrame(p, pose, 10, TransformTool::Move, TransformSpace::World, 1);
    auto drag = GizmoDrag::begin(p, pose, 10, TransformTool::Move, frame, 0,
                                 through(frame.origin + glm::vec3(.5f, 0, 0)));
    ASSERT_TRUE(drag);
    auto value = drag->update(through(frame.origin + glm::vec3(1.9f, 0, 0)), false);
    ASSERT_TRUE(value);
    pose[10] = value->transform;
    auto after = worldMatrices(p, pose);
    nearVector(glm::vec3(after[10][3] - before[10][3]), {1.4f, 0, 0});
    nearVector(glm::vec3(after[11][3] - before[11][3]), {1.4f, 0, 0});
    auto snapped = drag->update(through(frame.origin + glm::vec3(.76f, 0, 0)), true);
    ASSERT_TRUE(snapped);
    pose[10] = snapped->transform;
    nearVector(glm::vec3(worldMatrices(p, pose)[10][3] - before[10][3]), {.3f, 0, 0});
    EXPECT_TRUE(p.clips[0].tracks.empty());
}
TEST(Gizmo, WorldRotationPreservesPivotAndConnectedChildren) {
    auto p = makeRig("R15");
    auto pose = evaluatePose(p, 0, 0);
    p.parts[4].c0.rotation = glm::angleAxis(.5f, glm::vec3(0, 1, 0));
    p.parts[4].c1.rotation = glm::angleAxis(.3f, glm::vec3(1, 0, 0));
    pose[2].rotation = glm::angleAxis(.4f, glm::vec3(1, 0, 0));
    auto before = worldMatrices(p, pose);
    auto frame = makeGizmoFrame(p, pose, 4, TransformTool::Rotate, TransformSpace::World, 1);
    auto drag = GizmoDrag::begin(p, pose, 4, TransformTool::Rotate, frame, 2,
                                 through(frame.origin + glm::vec3(1, 0, 0)));
    ASSERT_TRUE(drag);
    auto value = drag->update(through(frame.origin + glm::vec3(0, 1, 0)), false);
    ASSERT_TRUE(value);
    pose[4] = value->transform;
    auto after = worldMatrices(p, pose);
    nearVector(glm::vec3((after[4] * p.parts[4].c1.matrix())[3]), frame.origin);
    const auto rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1));
    nearVector(glm::vec3(after[5][3]) - frame.origin, rotation * (glm::vec3(before[5][3]) - frame.origin));
    nearVector(glm::vec3(after[6][3]) - frame.origin, rotation * (glm::vec3(before[6][3]) - frame.origin));
    auto almost = drag->update(through(frame.origin + glm::vec3(std::cos(.3f), std::sin(.3f), 0)), true);
    ASSERT_TRUE(almost);
    pose[4] = almost->transform;
    auto snapped = worldMatrices(p, pose);
    auto delta = glm::quat_cast(glm::mat3(snapped[4])) * glm::inverse(glm::quat_cast(glm::mat3(before[4])));
    EXPECT_NEAR(std::abs(glm::dot(delta, glm::angleAxis(glm::radians(15.f), glm::vec3(0, 0, 1)))), 1, 1e-5);
}
TEST(Gizmo, ResizeChangesGeometryAndClampsWithoutScalingJoints) {
    auto p = makeRig("R15");
    auto pose = evaluatePose(p, 0, 0);
    auto frame = makeGizmoFrame(p, pose, 4, TransformTool::Resize, TransformSpace::Local, 1);
    auto drag = GizmoDrag::begin(p, pose, 4, TransformTool::Resize, frame, 0,
                                 through(frame.origin + glm::vec3(.5f, 0, 0)));
    ASSERT_TRUE(drag);
    auto value = drag->update(through(frame.origin + glm::vec3(.76f, 0, 0)), true);
    ASSERT_TRUE(value);
    EXPECT_NEAR(value->size.x, 1.5f, 1e-5f);
    nearVector(value->transform.scale, {1, 1, 1});
    nearVector(value->transform.position, {0, 0, 0});
    auto small = drag->update(through(frame.origin + glm::vec3(-10, 0, 0)), false);
    ASSERT_TRUE(small);
    EXPECT_FLOAT_EQ(small->size.x, .01f);
    EXPECT_FLOAT_EQ(p.parts[4].size.x, 1);
}
TEST(Gizmo, LockedAndScaledRigsDoNotOfferUnsafeTransforms) {
    auto p = makeRig("R15");
    auto pose = evaluatePose(p, 0, 0);
    auto frame = makeGizmoFrame(p, pose, 4, TransformTool::Move, TransformSpace::World, 1);
    p.parts[4].locked = true;
    EXPECT_FALSE(GizmoDrag::begin(p, pose, 4, TransformTool::Move, frame, 0, through(frame.origin)));
    pose[2].scale = {2, 1, 1};
    EXPECT_THROW(makeGizmoFrame(p, pose, 4, TransformTool::Rotate, TransformSpace::World, 1),
                 std::runtime_error);
}
