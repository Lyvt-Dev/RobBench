#include "animation/KeyEditing.h"
#include "io/Serialization.h"
#include <gtest/gtest.h>
using namespace rb;
TEST(KeyEditing, GroupMovePreservesSpacingAndChannels) {
    auto p = makeRig("R15");
    auto &c = p.clips[0];
    c.duration = 10;
    Transform value;
    value.position.x = 7;
    setKey(c, "Neck", {1, value});
    setKey(c, "Neck", {3, {}});
    setKey(c, "LeftShoulder", {2, value});
    auto selection = transformKeyTimes(c, {{"Neck", 1}, {"Neck", 3}, {"LeftShoulder", 2}}, 2);
    EXPECT_TRUE(selection.contains({"Neck", 3}));
    EXPECT_TRUE(selection.contains({"Neck", 5}));
    EXPECT_TRUE(selection.contains({"LeftShoulder", 4}));
    EXPECT_FLOAT_EQ(evaluate(c.tracks[0], 3).position.x, 7);
    EXPECT_DOUBLE_EQ(c.tracks[0].keys[1].time - c.tracks[0].keys[0].time, 2);
}
TEST(KeyEditing, CollisionsAndBoundsAreTransactional) {
    auto p = makeRig("R6");
    auto &c = p.clips[0];
    setKey(c, "Neck", {0, {}});
    setKey(c, "Neck", {1, {}});
    auto original = projectJson(p);
    EXPECT_THROW(transformKeyTimes(c, {{"Neck", 0}}, 1), std::runtime_error);
    EXPECT_EQ(projectJson(p), original);
    EXPECT_THROW(transformKeyTimes(c, {{"Neck", 0}, {"Neck", 1}}, -1), std::runtime_error);
    EXPECT_EQ(projectJson(p), original);
}
TEST(KeyEditing, DurationScalesAllTracksUniformly) {
    auto p = makeRig("R15");
    auto &c = p.clips[0];
    setKey(c, "Neck", {.5, {}});
    setKey(c, "LeftShoulder", {1.5, {}});
    setKey(c, "Neck", {2, {}});
    resizeClip(c, 8, true);
    EXPECT_DOUBLE_EQ(c.tracks[0].keys[0].time, 2);
    EXPECT_DOUBLE_EQ(c.tracks[1].keys[0].time, 6);
    EXPECT_DOUBLE_EQ(c.tracks[0].keys[1].time, 8);
    EXPECT_THROW(resizeClip(c, 1, false), std::runtime_error);
    EXPECT_DOUBLE_EQ(c.duration, 8);
    resizeClip(c, 2, true);
    EXPECT_DOUBLE_EQ(c.tracks[1].keys[0].time, 1.5);
}
TEST(KeyEditing, SelectionScaleCopyPasteDelete) {
    Clip c;
    c.duration = 10;
    setKey(c, "JointA", {1, {}});
    setKey(c, "JointB", {2, {}});
    auto selected = transformKeyTimes(c, {{"JointA", 1}, {"JointB", 2}}, 0, 2, 1);
    auto copied = copyKeys(c, selected);
    auto pasted = pasteKeys(c, copied, 5);
    EXPECT_TRUE(pasted.contains({"JointA", 5}));
    EXPECT_TRUE(pasted.contains({"JointB", 7}));
    deleteKeys(c, pasted);
    EXPECT_EQ(c.tracks[0].keys.size(), 1);
    EXPECT_EQ(c.tracks[1].keys.size(), 1);
}
