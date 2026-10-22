#include "io/ModelImporter.h"
#include "io/Serialization.h"
#include <QFile>
#include <QTemporaryDir>
#include <gtest/gtest.h>
using namespace rb;
TEST(Models, ClassicHeadHasFaceAndSerializableGeometry) {
    auto p = makeRig("R6");
    ASSERT_TRUE(p.parts[2].mesh);
    EXPECT_GT(p.parts[2].mesh->vertices.size(), 1000);
    bool dark = false;
    for (const auto &v : p.parts[2].mesh->vertices)
        if (v.color.x < .1f)
            dark = true;
    EXPECT_TRUE(dark);
    auto json = projectJson(p);
    EXPECT_EQ(json["version"], 2);
    auto loaded = projectFromJson(json);
    EXPECT_EQ(projectJson(loaded), json);
    auto animation = animationJson(p, 0);
    EXPECT_FALSE(animation["parts"][2].contains("mesh"));
}
TEST(Models, ImportObjPreservesRealTrianglesAndUV) {
    QTemporaryDir directory;
    QFile file(directory.filePath("triangle.obj"));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("o Triangle\nv 0 0 0\nv 2 0 0\nv 0 2 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n");
    file.close();
    auto p = makeRig("R15");
    auto count = p.parts.size();
    EXPECT_EQ(importModel(p, std::filesystem::path(file.fileName().toStdU16String())), 1);
    ASSERT_EQ(p.parts.size(), count + 1);
    auto &part = p.parts.back();
    ASSERT_TRUE(part.mesh);
    EXPECT_EQ(part.mesh->vertices.size(), 3);
    EXPECT_EQ(p.rigType, "Custom");
    EXPECT_FLOAT_EQ(part.size.x, 2);
    EXPECT_FLOAT_EQ(part.mesh->vertices[1].uv.x, 1);
    EXPECT_EQ(projectJson(projectFromJson(projectJson(p))), projectJson(p));
}
TEST(Models, InvalidImportKeepsDocument) {
    auto p = makeRig("R15");
    auto original = projectJson(p);
    EXPECT_THROW(importModel(p, "/this-model-does-not-exist.obj"), std::runtime_error);
    EXPECT_EQ(projectJson(p), original);
}
