#include "io/ModelImporter.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
namespace rb {
std::size_t importModel(Project &project, const std::filesystem::path &path, float scale) {
    if (!std::isfinite(scale) || scale <= 0 || scale > 10000)
        throw std::runtime_error("Invalid model import scale");
    Assimp::Importer importer;
    const auto utf8 = path.u8string();
    const auto *scene = importer.ReadFile(
        std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size()),
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_ValidateDataStructure);
    if (!scene || !scene->mRootNode)
        throw std::runtime_error(std::string("Model import failed: ") + importer.GetErrorString());
    struct Item {
        std::string name;
        std::shared_ptr<MeshGeometry> mesh;
        glm::vec3 low, high, color;
    };
    std::vector<Item> items;
    std::size_t total = 0;
    std::function<void(const aiNode *, aiMatrix4x4)> visit = [&](const aiNode *node, aiMatrix4x4 parent) {
        auto world = parent * node->mTransformation;
        aiMatrix3x3 normal(world);
        normal.Inverse().Transpose();
        for (unsigned m = 0; m < node->mNumMeshes; ++m) {
            const auto *source = scene->mMeshes[node->mMeshes[m]];
            if (source->HasBones())
                throw std::runtime_error("This model has skinning. Import a static export, or use the Studio "
                                         "exact-rig workflow; skinned mesh binding is not implemented.");
            Item item;
            item.name = source->mName.length ? source->mName.C_Str() : node->mName.C_Str();
            item.mesh = std::make_shared<MeshGeometry>();
            item.low = glm::vec3(std::numeric_limits<float>::max());
            item.high = -item.low;
            item.color = {.7f, .7f, .75f};
            aiColor4D color;
            if (source->mMaterialIndex < scene->mNumMaterials &&
                aiGetMaterialColor(scene->mMaterials[source->mMaterialIndex], AI_MATKEY_COLOR_DIFFUSE,
                                   &color) == AI_SUCCESS)
                item.color = {color.r, color.g, color.b};
            for (unsigned f = 0; f < source->mNumFaces; ++f) {
                const auto &face = source->mFaces[f];
                if (face.mNumIndices != 3)
                    continue;
                for (unsigned k = 0; k < 3; ++k) {
                    unsigned i = face.mIndices[k];
                    auto p = world * source->mVertices[i];
                    auto n = normal * source->mNormals[i];
                    n.Normalize();
                    MeshVertex v;
                    v.position = {p.x * scale, p.y * scale, p.z * scale};
                    v.normal = {n.x, n.y, n.z};
                    if (source->HasVertexColors(0)) {
                        auto c = source->mColors[0][i];
                        v.color = {c.r, c.g, c.b};
                    }
                    if (source->HasTextureCoords(0)) {
                        auto uv = source->mTextureCoords[0][i];
                        v.uv = {uv.x, uv.y};
                    }
                    item.low = glm::min(item.low, v.position);
                    item.high = glm::max(item.high, v.position);
                    item.mesh->vertices.push_back(v);
                    if (++total > 1000000)
                        throw std::runtime_error("Model exceeds one million triangle vertices");
                }
            }
            if (!item.mesh->vertices.empty())
                items.push_back(std::move(item));
        }
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            visit(node->mChildren[i], world);
    };
    visit(scene->mRootNode, aiMatrix4x4());
    if (items.empty())
        throw std::runtime_error("Model contains no supported triangles");
    Project next = project;
    int root = -1;
    for (std::size_t i = 0; i < next.parts.size(); ++i)
        if (next.parts[i].parent == -1)
            root = static_cast<int>(i);
    auto unique = [&](std::string base) {
        if (base.empty())
            base = "Mesh";
        std::string name = base;
        int n = 1;
        while (std::any_of(next.parts.begin(), next.parts.end(),
                           [&](const Part &p) { return p.name == name || p.joint == name; }))
            name = base + "_" + std::to_string(n++);
        return name;
    };
    const auto before = next.parts.size();
    for (auto &item : items) {
        Part part;
        part.name = unique(item.name);
        part.joint = part.name;
        part.parent = root;
        part.size = glm::max(item.high - item.low, glm::vec3(.001f));
        const auto center = (item.low + item.high) * .5f;
        part.c0.position = center;
        part.color = item.color;
        for (auto &vertex : item.mesh->vertices) {
            vertex.position = (vertex.position - center) / part.size;
            vertex.normal = glm::normalize(vertex.normal * part.size);
        }
        part.mesh = item.mesh;
        next.parts.push_back(std::move(part));
    }
    next.rigType = "Custom";
    next.validate();
    project = std::move(next);
    return project.parts.size() - before;
}
} // namespace rb
