#include "core/Project.h"
#include "io/Serialization.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>
using namespace rb;
static void nearMatrix(glm::mat4 a, glm::mat4 b) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_NEAR(a[c][r], b[c][r], 1e-4);
}
TEST(Math, QuaternionComposition) {
    Transform t;
    t.position = {1, 2, 3};
    t.rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1));
    auto v = t.matrix() * glm::vec4(1, 0, 0, 1);
    EXPECT_NEAR(v.x, 1, 1e-5);
    EXPECT_NEAR(v.y, 3, 1e-5);
    EXPECT_NEAR(v.z, 3, 1e-5);
}
TEST(Rigs, StandardTopology) {
    for (auto type : {"R6", "R15"}) {
        auto p = makeRig(type);
        EXPECT_NO_THROW(p.validate());
        EXPECT_EQ(p.parts.size(), type == std::string("R6") ? 7u : 16u);
        std::size_t joints = 0;
        for (auto &part : p.parts)
            if (part.parent >= 0)
                ++joints;
        EXPECT_EQ(joints, p.parts.size() - 1);
    }
}
TEST(Scene, Motor6DEquation) {
    auto p = makeRig("R15");
    auto pose = evaluatePose(p, 0, 0);
    pose[4].rotation = glm::angleAxis(.7f, glm::vec3(1, 0, 0));
    auto w = worldMatrices(p, pose);
    for (std::size_t i = 1; i < p.parts.size(); ++i) {
        auto &part = p.parts[i];
        nearMatrix(w[i] * part.c1.matrix(), w[part.parent] * part.c0.matrix() * pose[i].matrix());
    }
}
TEST(Scene, CycleAndInvalidParentRejected) {
    auto p = makeRig("R15");
    p.parts[1].parent = 4;
    EXPECT_THROW(p.validate(), std::runtime_error);
    EXPECT_THROW(worldMatrices(p, std::vector<Transform>(p.parts.size())), std::runtime_error);
    p.parts[1].parent = 99999;
    EXPECT_THROW(p.validate(), std::runtime_error);
}
TEST(Animation, ShortestQuaternionPath) {
    Track t{"Shoulder", {}};
    Transform a, b;
    b.rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(0, 0, 1));
    t.keys = {{0, a}, {2, b}};
    auto v = evaluate(t, 1);
    auto x = v.rotation * glm::vec3(1, 0, 0);
    EXPECT_NEAR(x.x, std::sqrt(.5f), 1e-5);
    EXPECT_NEAR(x.y, std::sqrt(.5f), 1e-5);
    t.keys[1].value.rotation = -b.rotation;
    EXPECT_NEAR(std::abs(glm::dot(v.rotation, evaluate(t, 1).rotation)), 1, 1e-5);
}
TEST(Animation, DeterministicBoundariesAndStep) {
    Track t;
    Transform a, b;
    b.position.x = 10;
    t.keys = {{0, a, Interpolation::Step}, {1, b}};
    EXPECT_EQ(evaluate(t, .999).position.x, 0);
    EXPECT_EQ(evaluate(t, 1).position.x, 10);
    EXPECT_EQ(evaluate(t, -1).position.x, 0);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(evaluate(t, .5).position, evaluate(t, .5).position);
}
TEST(Animation, KeyInsertionReplacementDeletion) {
    Clip c;
    setKey(c, "Neck", {1, {}});
    Transform t;
    t.position.x = 5;
    setKey(c, "Neck", {0, t});
    setKey(c, "Neck", {1, t});
    ASSERT_EQ(c.tracks[0].keys.size(), 2);
    EXPECT_EQ(c.tracks[0].keys[0].time, 0);
    EXPECT_EQ(c.tracks[0].keys[1].value.position.x, 5);
    EXPECT_TRUE(deleteKey(c, "Neck", 1));
    EXPECT_FALSE(deleteKey(c, "Neck", 1));
    EXPECT_THROW(setKey(c, "Neck", {3, {}}), std::runtime_error);
}
TEST(Serialization, InvalidDataRejected) {
    auto p = makeRig("R15");
    auto j = projectJson(p);
    j["version"] = 2;
    EXPECT_THROW(projectFromJson(j), std::runtime_error);
    j = projectJson(p);
    j["parts"][0]["c0"]["rotation"] = {0, 0, 0, 0};
    EXPECT_THROW(projectFromJson(j), std::runtime_error);
    j = projectJson(p);
    j["parts"][2]["joint"] = j["parts"][1]["joint"];
    EXPECT_THROW(projectFromJson(j), std::runtime_error);
}
TEST(Integration, R6AndR15SaveReopenExport) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    for (auto type : {"R6", "R15"}) {
        auto p = makeRig(type);
        auto joint = p.parts[3].joint;
        Transform a, b;
        b.rotation = glm::angleAxis(.9f, glm::vec3(1, 0, 0));
        setKey(p.clips[0], joint, {0, a});
        setKey(p.clips[0], joint, {1, b});
        setKey(p.clips[0], joint, {2, a});
        auto path = std::filesystem::path(dir.path().toStdString()) / (std::string(type) + ".robbench");
        saveProject(p, path);
        auto reopened = loadProject(path);
        EXPECT_EQ(projectJson(p), projectJson(reopened));
        auto output = animationJson(reopened, 0, 30);
        EXPECT_EQ(output["frames"].size(), 61);
        EXPECT_EQ(output["frames"][30]["time"], 1);
        auto q = output["frames"][30]["transforms"][3]["rotation"];
        EXPECT_NEAR(q[0].get<float>(), b.rotation.x, 1e-6);
        exportAnimation(reopened, 0, path.string() + ".rbanim");
        EXPECT_TRUE(std::filesystem::exists(path.string() + ".rbanim"));
    }
}
TEST(Export, PreservesNonFrameKeysAndRejectsScale) {
    auto p = makeRig("R15");
    Transform t;
    setKey(p.clips[0], p.parts[4].joint, {.123, t, Interpolation::Step});
    setKey(p.clips[0], p.parts[4].joint, {.789, t});
    auto j = animationJson(p, 0, 24);
    bool found = false, before = false;
    for (auto &f : j["frames"]) {
        if (f["time"] == .123)
            found = true;
        if (std::abs(f["time"].get<double>() - (.789 - 1e-5)) < 1e-8)
            before = true;
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(before);
    p.clips[0].tracks[0].keys[0].value.scale.x = 2;
    EXPECT_THROW(animationJson(p, 0), std::runtime_error);
}
TEST(IO, FailedSaveDoesNotDestroyExistingFile) {
    QTemporaryDir dir;
    auto path = std::filesystem::path(dir.path().toStdString()) / "safe.robbench";
    auto p = makeRig("R15");
    saveProject(p, path);
    auto bad = p;
    bad.parts[0].parent = 1;
    EXPECT_THROW(saveProject(bad, path), std::runtime_error);
    EXPECT_EQ(projectJson(loadProject(path)), projectJson(p));
}

TEST(Export, RootMotionRejected) {
    auto p = makeRig("R15");
    Transform t;
    t.position.x = 1;
    setKey(p.clips[0], p.parts[0].joint, {0, t});
    EXPECT_THROW(animationJson(p, 0), std::runtime_error);
}
TEST(Integration, AnimationImportRoundTrip) {
    auto p = makeRig("R15");
    Transform t;
    t.rotation = glm::angleAxis(.75f, glm::vec3(1, 0, 0));
    setKey(p.clips[0], p.parts[4].joint, {0, {}});
    setKey(p.clips[0], p.parts[4].joint, {1, t});
    auto data = animationJson(p, 0, 30);
    auto imported = projectFromAnimationJson(data);
    auto a = evaluatePose(p, 0, 1);
    auto b = evaluatePose(imported, 0, 1);
    EXPECT_NEAR(std::abs(glm::dot(a[4].rotation, b[4].rotation)), 1, 1e-6);
    data["frames"][1]["time"] = 0;
    EXPECT_THROW(projectFromAnimationJson(data), std::runtime_error);
}
