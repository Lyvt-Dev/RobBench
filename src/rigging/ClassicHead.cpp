#include "core/Project.h"
#include <cmath>
#include <glm/gtc/constants.hpp>
namespace rb {
std::shared_ptr<const MeshGeometry> makeClassicHead() {
    auto mesh = std::make_shared<MeshGeometry>();
    auto vertex = [](glm::vec3 p, glm::vec3 n, glm::vec3 c = glm::vec3(1)) {
        return MeshVertex{{p.x / 2, p.y, p.z}, n, c, {0, 0}};
    };
    auto triangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 na, glm::vec3 nb, glm::vec3 nc,
                        glm::vec3 color = glm::vec3(1)) {
        mesh->vertices.push_back(vertex(a, na, color));
        mesh->vertices.push_back(vertex(b, nb, color));
        mesh->vertices.push_back(vertex(c, nc, color));
    };
    const float heights[]{-.625f, -.545f, .545f, .625f};
    const float radii[]{.545f, .625f, .625f, .545f};
    for (int i = 0; i < 48; ++i) {
        float a = glm::two_pi<float>() * i / 48, b = glm::two_pi<float>() * (i + 1) / 48;
        glm::vec3 na{std::cos(a), 0, std::sin(a)}, nb{std::cos(b), 0, std::sin(b)};
        for (int band = 0; band < 3; ++band) {
            auto p = na * radii[band];
            p.y = heights[band];
            auto q = nb * radii[band];
            q.y = heights[band];
            auto r = nb * radii[band + 1];
            r.y = heights[band + 1];
            auto s = na * radii[band + 1];
            s.y = heights[band + 1];
            triangle(p, q, r, na, nb, nb);
            triangle(p, r, s, na, nb, na);
        }
        triangle({0, .625f, 0}, {std::cos(a) * .545f, .625f, std::sin(a) * .545f},
                 {std::cos(b) * .545f, .625f, std::sin(b) * .545f}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0});
        triangle({0, -.625f, 0}, {std::cos(b) * .545f, -.625f, std::sin(b) * .545f},
                 {std::cos(a) * .545f, -.625f, std::sin(a) * .545f}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0});
    }
    // Original geometric smile; no copied decal or proprietary avatar mesh.
    auto surface = [](float x, float y) {
        return glm::vec3(x, y, -std::sqrt(.625f * .625f - x * x) - .006f);
    };
    auto quad = [&](float x0, float x1, float y0, float y1) {
        auto a = surface(x0, y0), b = surface(x1, y0), c = surface(x1, y1), d = surface(x0, y1);
        glm::vec3 n{0, 0, -1};
        triangle(a, b, c, n, n, n, {.02f, .02f, .02f});
        triangle(a, c, d, n, n, n, {.02f, .02f, .02f});
    };
    quad(-.255f, -.175f, .06f, .23f);
    quad(.175f, .255f, .06f, .23f);
    for (int i = 0; i < 24; ++i) {
        float x = -.28f + .56f * i / 24, x2 = -.28f + .56f * (i + 1) / 24;
        float y = -.1f - .15f * (1 - (x / .28f) * (x / .28f)),
              y2 = -.1f - .15f * (1 - (x2 / .28f) * (x2 / .28f));
        auto a = surface(x, y), b = surface(x2, y2), c = surface(x2, y2 + .035f), d = surface(x, y + .035f);
        glm::vec3 n{0, 0, -1};
        triangle(a, b, c, n, n, n, {.02f, .02f, .02f});
        triangle(a, c, d, n, n, n, {.02f, .02f, .02f});
    }
    return mesh;
}
} // namespace rb
