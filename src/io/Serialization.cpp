#include "io/Serialization.h"
#include <QFile>
#include <QSaveFile>
#include <QString>
#include <cmath>
#include <fstream>
#include <stdexcept>
namespace rb {
using json = nlohmann::json;
static json vec(glm::vec3 v) {
    return {v.x, v.y, v.z};
}
static glm::vec3 readVec(const json &a) {
    if (!a.is_array() || a.size() != 3)
        throw std::runtime_error("Expected three-vector");
    return {a.at(0).get<float>(), a.at(1).get<float>(), a.at(2).get<float>()};
}
static json transform(const Transform &t) {
    return {{"position", vec(t.position)},
            {"rotation", {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w}},
            {"scale", vec(t.scale)}};
}
static Transform readTransform(const json &j) {
    auto q = j.at("rotation");
    if (!q.is_array() || q.size() != 4)
        throw std::runtime_error("Expected XYZW quaternion");
    return {readVec(j.at("position")),
            {q.at(3).get<float>(), q.at(0).get<float>(), q.at(1).get<float>(), q.at(2).get<float>()},
            readVec(j.at("scale"))};
}
json projectJson(const Project &p) {
    p.validate();
    json j = {{"format", "robbench"}, {"version", 2},           {"name", p.name},
              {"rigType", p.rigType}, {"parts", json::array()}, {"clips", json::array()}};
    for (const auto &part : p.parts) {
        j["parts"].push_back({{"name", part.name},
                              {"joint", part.joint},
                              {"parent", part.parent},
                              {"size", vec(part.size)},
                              {"c0", transform(part.c0)},
                              {"c1", transform(part.c1)},
                              {"color", vec(part.color)},
                              {"visible", part.visible},
                              {"locked", part.locked}});
        if (part.mesh) {
            auto &vertices = j["parts"].back()["mesh"] = json::array();
            for (const auto &v : part.mesh->vertices)
                vertices.push_back({v.position.x, v.position.y, v.position.z, v.normal.x, v.normal.y,
                                    v.normal.z, v.color.x, v.color.y, v.color.z, v.uv.x, v.uv.y});
        }
    }
    for (const auto &c : p.clips) {
        json clip = {{"name", c.name}, {"duration", c.duration}, {"loop", c.loop}, {"tracks", json::array()}};
        for (const auto &t : c.tracks) {
            json track = {{"joint", t.joint}, {"keys", json::array()}};
            for (const auto &k : t.keys)
                track["keys"].push_back({{"time", k.time},
                                         {"value", transform(k.value)},
                                         {"interpolation", static_cast<int>(k.interpolation)}});
            clip["tracks"].push_back(track);
        }
        j["clips"].push_back(clip);
    }
    return j;
}
Project projectFromJson(const json &j) {
    if (j.at("format") != "robbench" || (j.at("version") != 1 && j.at("version") != 2))
        throw std::runtime_error("Unsupported RobBench format/version");
    if (!j.at("parts").is_array() || !j.at("clips").is_array())
        throw std::runtime_error("Parts and clips must be arrays");
    if (j.at("parts").size() > 10000 || j.at("clips").size() > 1000)
        throw std::runtime_error("Project exceeds safety limits");
    Project p;
    p.name = j.at("name");
    p.rigType = j.at("rigType");
    p.clips.clear();
    for (const auto &a : j.at("parts")) {
        Part part;
        part.name = a.at("name");
        part.joint = a.at("joint");
        part.parent = a.at("parent");
        part.size = readVec(a.at("size"));
        part.c0 = readTransform(a.at("c0"));
        part.c1 = readTransform(a.at("c1"));
        part.color = readVec(a.at("color"));
        part.visible = a.at("visible");
        part.locked = a.at("locked");
        if (a.contains("mesh")) {
            const auto &vertices = a.at("mesh");
            if (!vertices.is_array() || vertices.size() > 1000000)
                throw std::runtime_error("Invalid mesh array");
            auto mesh = std::make_shared<MeshGeometry>();
            mesh->vertices.reserve(vertices.size());
            for (const auto &v : vertices) {
                if (!v.is_array() || v.size() != 11)
                    throw std::runtime_error("Invalid mesh vertex record");
                mesh->vertices.push_back(
                    {{v[0], v[1], v[2]}, {v[3], v[4], v[5]}, {v[6], v[7], v[8]}, {v[9], v[10]}});
            }
            part.mesh = mesh;
        }
        p.parts.push_back(part);
    }
    for (const auto &c : j.at("clips")) {
        Clip clip;
        clip.name = c.at("name");
        clip.duration = c.at("duration");
        clip.loop = c.at("loop");
        if (!c.at("tracks").is_array())
            throw std::runtime_error("Tracks must be an array");
        for (const auto &t : c.at("tracks")) {
            if (!t.at("keys").is_array())
                throw std::runtime_error("Keys must be an array");
            Track track;
            track.joint = t.at("joint");
            if (t.at("keys").size() > 100000)
                throw std::runtime_error("Too many keys");
            for (const auto &k : t.at("keys"))
                track.keys.push_back({k.at("time"), readTransform(k.at("value")),
                                      static_cast<Interpolation>(k.at("interpolation").get<int>())});
            clip.tracks.push_back(track);
        }
        p.clips.push_back(clip);
    }
    p.validate();
    return p;
}
Project projectFromAnimationJson(const json &j) {
    if (j.at("format") != "rbanim" || j.at("version") != 1 || j.at("interpolation") != "baked-linear")
        throw std::runtime_error("Unsupported animation interchange format");
    const auto &frames = j.at("frames");
    const auto &parts = j.at("parts");
    if (!frames.is_array() || frames.size() < 2 || frames.size() > 200000 || parts.empty() ||
        parts.size() > 1000 || frames.size() * parts.size() > 500000)
        throw std::runtime_error("Animation exceeds importer limits");
    json wrapper = {{"format", "robbench"},
                    {"version", 1},
                    {"name", j.at("name")},
                    {"rigType", j.at("rigType")},
                    {"parts", parts},
                    {"clips", json::array({{{"name", j.at("name")},
                                            {"duration", j.at("duration")},
                                            {"loop", j.at("loop")},
                                            {"tracks", json::array()}}})}};
    auto p = projectFromJson(wrapper);
    double previous = -1;
    for (const auto &frame : frames) {
        double time = frame.at("time");
        if (!std::isfinite(time) || time < 0 || time <= previous || time > p.clips[0].duration ||
            frame.at("transforms").size() != p.parts.size())
            throw std::runtime_error("Invalid animation frame time or transform count");
        for (std::size_t i = 0; i < p.parts.size(); ++i)
            setKey(p.clips[0], p.parts[i].joint,
                   {time, readTransform(frame.at("transforms").at(i)), Interpolation::Linear});
        previous = time;
    }
    if (frames.front().at("time") != 0 || std::abs(previous - p.clips[0].duration) > 1e-7)
        throw std::runtime_error("Animation must include both endpoints");
    p.validate();
    return p;
}
static QString pathString(const std::filesystem::path &path) {
    return QString::fromStdU16String(path.u16string());
}
static void atomicWrite(const std::filesystem::path &path, const json &j) {
    QSaveFile file(pathString(path));
    if (!file.open(QIODevice::WriteOnly))
        throw std::runtime_error(file.errorString().toStdString());
    auto text = j.dump(2);
    if (file.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size()) ||
        !file.commit())
        throw std::runtime_error("Atomic save failed: " + file.errorString().toStdString());
}
Project loadProject(const std::filesystem::path &path) {
    QFile file(pathString(path));
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(file.errorString().toStdString());
    if (file.size() > 64 * 1024 * 1024)
        throw std::runtime_error("Project exceeds 64 MiB limit");
    auto data = file.readAll();
    auto parsed = json::parse(data.constData(), data.constData() + data.size());
    return parsed.value("format", std::string()) == "rbanim" ? projectFromAnimationJson(parsed)
                                                             : projectFromJson(parsed);
}
void saveProject(const Project &p, const std::filesystem::path &path) {
    atomicWrite(path, projectJson(p));
}
json animationJson(const Project &p, std::size_t index, int fps) {
    p.validate();
    if (index >= p.clips.size() || fps < 1 || fps > 240)
        throw std::runtime_error("Invalid clip or bake rate");
    const auto &c = p.clips[index];
    if (c.duration * fps > 200000 || (std::ceil(c.duration * fps) + 2) * p.parts.size() > 500000)
        throw std::runtime_error("Animation exceeds export sample limit");
    for (const auto &track : c.tracks) {
        auto part = std::find_if(p.parts.begin(), p.parts.end(),
                                 [&](const Part &a) { return a.joint == track.joint; });
        if (part != p.parts.end() && part->parent == -1)
            for (const auto &key : track.keys)
                if (glm::length(key.value.position) > 1e-5f ||
                    std::abs(glm::dot(key.value.rotation, glm::quat(1, 0, 0, 0))) < .999999f)
                    throw std::runtime_error("World-root animation cannot be encoded by Roblox Pose; animate "
                                             "Root/RootJoint instead");
    }
    for (const auto &t : c.tracks)
        for (const auto &k : t.keys)
            if (glm::length(k.value.scale - glm::vec3(1)) > 1e-5f)
                throw std::runtime_error("Roblox Motor6D animations cannot encode scale");
    json j = {{"format", "rbanim"},
              {"version", 1},
              {"rigType", p.rigType},
              {"name", c.name},
              {"duration", c.duration},
              {"loop", c.loop},
              {"fps", fps},
              {"interpolation", "baked-linear"},
              {"parts", projectJson(p).at("parts")},
              {"frames", json::array()}};
    for (auto &part : j["parts"])
        part.erase("mesh");
    // Retain original key times as well as uniform samples, including both sides of a step.
    std::vector<double> times{0, c.duration};
    for (int i = 1; i < static_cast<int>(std::ceil(c.duration * fps)); ++i)
        times.push_back(static_cast<double>(i) / fps);
    for (const auto &t : c.tracks)
        for (std::size_t i = 0; i < t.keys.size(); ++i) {
            times.push_back(t.keys[i].time);
            if (i > 0 && t.keys[i - 1].interpolation == Interpolation::Step && t.keys[i].time > 0)
                times.push_back(std::max(t.keys[i - 1].time, t.keys[i].time - 1e-5));
        }
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    if (times.size() * p.parts.size() > 500000)
        throw std::runtime_error("Animation exceeds companion importer sample limit");
    for (double time : times) {
        auto pose = evaluatePose(p, index, time);
        json values = json::array();
        for (const auto &t : pose)
            values.push_back(transform(t));
        j["frames"].push_back({{"time", time}, {"transforms", values}});
    }
    return j;
}
void exportAnimation(const Project &p, std::size_t c, const std::filesystem::path &path, int fps) {
    atomicWrite(path, animationJson(p, c, fps));
}
} // namespace rb
