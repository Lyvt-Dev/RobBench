#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#define VMA_IMPLEMENTATION
#include "renderer/Viewport.h"
#include <QFile>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vk_mem_alloc.h>
namespace rb {
namespace {
void check(VkResult r, const char *what) {
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string(what) + ": Vulkan error " + std::to_string(r));
}
struct Vertex {
    glm::vec3 position, normal;
};
struct Push {
    glm::mat4 mvp;
    glm::vec4 rotation, color;
};
class Renderer final : public QVulkanWindowRenderer {
    Viewport &window;
    VmaAllocator allocator{};
    VmaAllocation allocation{};
    VkBuffer buffer{};
    VkPipelineLayout layout{};
    VkPipeline pipeline{};
    VkPipeline overlayPipeline{};
    VkPipelineCache cache{};
    bool ready{false};
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    std::array<VkFence, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT> completion{};
    std::array<bool, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT> pending{};
#endif
    VkShaderModule shader(const char *name) {
        QFile file(QString(":/shaders/") + name);
        if (!file.open(QIODevice::ReadOnly))
            throw std::runtime_error("Cannot open embedded SPIR-V shader");
        auto data = file.readAll();
        std::vector<uint32_t> words(static_cast<std::size_t>(data.size()) / 4);
        std::memcpy(words.data(), data.data(), static_cast<std::size_t>(data.size()));
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = static_cast<std::size_t>(data.size());
        info.pCode = words.data();
        VkShaderModule result{};
        check(vkCreateShaderModule(window.device(), &info, nullptr, &result), "Create shader");
        return result;
    }

  public:
    explicit Renderer(Viewport &w) : window(w) {}
    void initResources() override {
        try {
            auto properties = vk::PhysicalDevice(window.physicalDevice()).getProperties();
            if (properties.apiVersion < VK_API_VERSION_1_3)
                throw std::runtime_error("Vulkan 1.3 capable GPU and driver required");
            spdlog::info("Vulkan device: {}", properties.deviceName.data());
            VmaVulkanFunctions functions{};
            functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
            functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
            VmaAllocatorCreateInfo ai{};
            ai.pVulkanFunctions = &functions;
            ai.instance = window.vulkanInstance()->vkInstance();
            ai.physicalDevice = window.physicalDevice();
            ai.device = window.device();
            ai.vulkanApiVersion = VK_API_VERSION_1_3;
            check(vmaCreateAllocator(&ai, &allocator), "Create GPU allocator");
            std::vector<Vertex> vertices;
            const std::array<glm::vec3, 6> normals{
                {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
            for (auto n : normals) {
                glm::vec3 u = std::abs(n.y) < .5f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                auto v = glm::cross(n, u);
                std::array<glm::vec3, 4> p{
                    {(n - u - v) * .5f, (n + u - v) * .5f, (n + u + v) * .5f, (n - u + v) * .5f}};
                for (int i : {0, 1, 2, 2, 3, 0})
                    vertices.push_back({p[i], n});
            }
            VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bi.size = vertices.size() * sizeof(Vertex);
            bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            VmaAllocationCreateInfo ac{};
            ac.usage = VMA_MEMORY_USAGE_AUTO;
            ac.flags =
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            VmaAllocationInfo info{};
            check(vmaCreateBuffer(allocator, &bi, &ac, &buffer, &allocation, &info), "Create vertex buffer");
            std::memcpy(info.pMappedData, vertices.data(), static_cast<std::size_t>(bi.size));
            check(vmaFlushAllocation(allocator, allocation, 0, VK_WHOLE_SIZE), "Flush vertices");
            VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                      sizeof(Push)};
            VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            li.pushConstantRangeCount = 1;
            li.pPushConstantRanges = &range;
            check(vkCreatePipelineLayout(window.device(), &li, nullptr, &layout), "Create layout");
            VkPipelineCacheCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
            check(vkCreatePipelineCache(window.device(), &ci, nullptr, &cache), "Create pipeline cache");
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
            for (auto &fence : completion) {
                VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                check(vkCreateFence(window.device(), &fi, nullptr, &fence), "Create frame completion fence");
            }
#endif
            ready = true;
        } catch (const std::exception &e) {
            ready = false;
            spdlog::error("{}", e.what());
            if (window.failed)
                window.failed(QString::fromUtf8(e.what()));
        }
    }
    void initSwapChainResources() override {
        if (!ready)
            return;
        VkShaderModule vert{}, frag{};
        try {
            vert = shader("scene.vert.spv");
            frag = shader("scene.frag.spv");
            VkPipelineShaderStageCreateInfo stages[2]{};
            for (auto &s : stages) {
                s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                s.pName = "main";
            }
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vert;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = frag;
            VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
            VkVertexInputAttributeDescription attrs[2]{
                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)}};
            VkPipelineVertexInputStateCreateInfo vi{
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vi.vertexBindingDescriptionCount = 1;
            vi.pVertexBindingDescriptions = &binding;
            vi.vertexAttributeDescriptionCount = 2;
            vi.pVertexAttributeDescriptions = attrs;
            VkPipelineInputAssemblyStateCreateInfo ia{
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo vs{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            vs.viewportCount = 1;
            vs.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo rs{
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            rs.polygonMode = VK_POLYGON_MODE_FILL;
            rs.cullMode = VK_CULL_MODE_NONE;
            rs.lineWidth = 1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            ms.rasterizationSamples = window.sampleCountFlagBits();
            VkPipelineDepthStencilStateCreateInfo ds{
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            ds.depthTestEnable = VK_TRUE;
            ds.depthWriteEnable = VK_TRUE;
            ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            VkPipelineColorBlendAttachmentState attachment{};
            attachment.colorWriteMask = 0xf;
            VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            cb.attachmentCount = 1;
            cb.pAttachments = &attachment;
            VkDynamicState dynamics[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dyn.dynamicStateCount = 2;
            dyn.pDynamicStates = dynamics;
            VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pi.stageCount = 2;
            pi.pStages = stages;
            pi.pVertexInputState = &vi;
            pi.pInputAssemblyState = &ia;
            pi.pViewportState = &vs;
            pi.pRasterizationState = &rs;
            pi.pMultisampleState = &ms;
            pi.pDepthStencilState = &ds;
            pi.pColorBlendState = &cb;
            pi.pDynamicState = &dyn;
            pi.layout = layout;
            pi.renderPass = window.defaultRenderPass();
            check(vkCreateGraphicsPipelines(window.device(), cache, 1, &pi, nullptr, &pipeline),
                  "Create graphics pipeline");
            ds.depthTestEnable = VK_FALSE;
            ds.depthWriteEnable = VK_FALSE;
            check(vkCreateGraphicsPipelines(window.device(), cache, 1, &pi, nullptr, &overlayPipeline),
                  "Create gizmo pipeline");
        } catch (const std::exception &e) {
            if (window.failed)
                window.failed(QString::fromUtf8(e.what()));
        }
        if (vert)
            vkDestroyShaderModule(window.device(), vert, nullptr);
        if (frag)
            vkDestroyShaderModule(window.device(), frag, nullptr);
    }
    void releaseSwapChainResources() override {
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        // Qt waits for device idle before this hook. Reset completion state as
        // swapchain recreation also resets Qt's frame-slot numbering.
        for (std::size_t i = 0; i < completion.size(); ++i)
            if (completion[i] && pending[i]) {
                vkResetFences(window.device(), 1, &completion[i]);
                pending[i] = false;
            }
#endif
        if (pipeline)
            vkDestroyPipeline(window.device(), pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
        if (overlayPipeline)
            vkDestroyPipeline(window.device(), overlayPipeline, nullptr);
        overlayPipeline = VK_NULL_HANDLE;
    }
    void releaseResources() override {
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        for (auto &fence : completion) {
            if (fence)
                vkDestroyFence(window.device(), fence, nullptr);
            fence = VK_NULL_HANDLE;
        }
        pending.fill(false);
#endif
        if (buffer)
            vmaDestroyBuffer(allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        if (layout)
            vkDestroyPipelineLayout(window.device(), layout, nullptr);
        layout = VK_NULL_HANDLE;
        if (cache)
            vkDestroyPipelineCache(window.device(), cache, nullptr);
        cache = VK_NULL_HANDLE;
        if (allocator)
            vmaDestroyAllocator(allocator);
        allocator = nullptr;
        ready = false;
    }
    void logicalDeviceLost() override {
        ready = false;
        if (window.failed)
            window.failed("Vulkan device lost. Save your project and restart RobBench.");
    }
    void startNextFrame() override {
        auto cmd = window.currentCommandBuffer();
        auto size = window.swapChainImageSize();
        std::array<VkClearValue, 3> clear{};
        clear[0].color = {{.016f, .018f, .024f, 1}};
        clear[1].depthStencil = {1, 0};
        clear[2] = clear[0];
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = window.defaultRenderPass();
        begin.framebuffer = window.currentFramebuffer();
        begin.renderArea.extent = {static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height())};
        begin.clearValueCount = window.sampleCountFlagBits() == VK_SAMPLE_COUNT_1_BIT ? 2 : 3;
        begin.pClearValues = clear.data();
        vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
        if (ready && pipeline) {
            VkViewport vp{0, 0, static_cast<float>(size.width()), static_cast<float>(size.height()), 0, 1};
            VkRect2D sc{{0, 0}, begin.renderArea.extent};
            vkCmdSetViewport(cmd, 0, 1, &vp);
            vkCmdSetScissor(cmd, 0, 1, &sc);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
            auto vpMatrix = window.viewProjection();
            auto draw = [&](glm::mat4 model, glm::vec3 color, bool unlit = false) {
                glm::mat3 basis(model);
                for (int i = 0; i < 3; ++i)
                    basis[i] = glm::normalize(basis[i]);
                auto q = glm::normalize(glm::quat_cast(basis));
                Push push{vpMatrix * model, {q.x, q.y, q.z, q.w}, {color, unlit ? 0.f : 1.f}};
                vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(push), &push);
                vkCmdDraw(cmd, 36, 1, 0, 0);
            };
            if (window.showGrid)
                for (int i = -10; i <= 10; ++i) {
                    auto m = glm::translate(glm::mat4(1), glm::vec3(static_cast<float>(i), -.16f, 0));
                    draw(glm::scale(m, glm::vec3(.012f, .01f, 20)),
                         i == 0 ? glm::vec3(.25f, .4f, .75f) : glm::vec3(.22f));
                    m = glm::translate(glm::mat4(1), glm::vec3(0, -.16f, static_cast<float>(i)));
                    draw(glm::scale(m, glm::vec3(20, .01f, .012f)),
                         i == 0 ? glm::vec3(.7f, .25f, .3f) : glm::vec3(.22f));
                }
            if (window.project && window.pose.size() == window.project->parts.size()) {
                auto worlds = worldMatrices(*window.project, window.pose);
                for (std::size_t i = 0; i < worlds.size(); ++i) {
                    auto &part = window.project->parts[i];
                    if (!part.visible)
                        continue;
                    auto color =
                        static_cast<int>(i) == window.selected ? glm::vec3(.74f, .52f, .98f) : part.color;
                    draw(glm::scale(worlds[i], window.partSize(i)), color);
                }
            }
            if (overlayPipeline)
                if (auto frame = window.gizmoFrame()) {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipeline);
                    auto line = [&](glm::vec3 a, glm::vec3 b, float width, glm::vec3 color) {
                        auto z = glm::normalize(b - a);
                        auto reference = std::abs(z.y) < .95f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
                        auto x = glm::normalize(glm::cross(reference, z));
                        auto y = glm::cross(z, x);
                        glm::mat4 matrix(glm::vec4(x, 0), glm::vec4(y, 0), glm::vec4(z, 0),
                                         glm::vec4((a + b) * .5f, 1));
                        draw(glm::scale(matrix, glm::vec3(width, width, glm::length(b - a))), color, true);
                    };
                    for (int axis = 0; axis < 3; ++axis) {
                        const glm::vec3 colors[3]{{.9f, .12f, .12f}, {.12f, .8f, .22f}, {.15f, .38f, 1}};
                        const int active = window.dragging() ? window.dragAxis() : window.hoveredAxis;
                        auto color = active == axis ? glm::vec3(1, .8f, .15f) : colors[axis];
                        const auto unit = frame->axis(axis);
                        const auto radius = frame->radius;
                        if (window.tool() == TransformTool::Rotate) {
                            auto u = frame->axis((axis + 1) % 3), v = frame->axis((axis + 2) % 3);
                            for (int segment = 0; segment < 64; ++segment) {
                                float a = glm::two_pi<float>() * segment / 64,
                                      b = glm::two_pi<float>() * (segment + 1) / 64;
                                line(frame->origin + radius * (std::cos(a) * u + std::sin(a) * v),
                                     frame->origin + radius * (std::cos(b) * u + std::sin(b) * v),
                                     radius * .018f, color);
                            }
                        } else {
                            line(frame->origin + unit * radius * .15f, frame->origin + unit * radius,
                                 radius * .025f, color);
                            float tip = window.tool() == TransformTool::Resize ? .12f : .08f;
                            auto model = glm::translate(glm::mat4(1), frame->origin + unit * radius) *
                                         glm::mat4_cast(frame->orientation);
                            draw(glm::scale(model, glm::vec3(radius * tip)), color, true);
                        }
                    }
                }
        }
        vkCmdEndRenderPass(cmd);
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        const int submittedFrame = window.currentFrame();
        const int nextFrame = (submittedFrame + 1) % window.concurrentFrameCount();
#endif
        window.frameReady();
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        // Older Qt waits on the acquire fence, which does not prove the previous
        // queue wait on the same binary semaphore has completed. Fence each slot
        // after submission and wait only before that slot is reused by Qt.
        if (ready) {
            auto result = vkQueueSubmit(window.graphicsQueue(), 0, nullptr, completion[submittedFrame]);
            if (result == VK_SUCCESS) {
                pending[submittedFrame] = true;
                if (pending[nextFrame]) {
                    result = vkWaitForFences(window.device(), 1, &completion[nextFrame], VK_TRUE, UINT64_MAX);
                    if (result == VK_SUCCESS) {
                        vkResetFences(window.device(), 1, &completion[nextFrame]);
                        pending[nextFrame] = false;
                    }
                }
            }
            if (result != VK_SUCCESS) {
                ready = false;
                if (window.failed)
                    window.failed("Vulkan frame synchronization failed; save and restart.");
            }
        }
#endif
        if (window.frameRendered)
            window.frameRendered();
    }
};
} // namespace
QVulkanWindowRenderer *Viewport::createRenderer() {
    return new Renderer(*this);
}
} // namespace rb
