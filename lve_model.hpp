#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "lve_device.hpp"
#include "lve_utils.hpp"
#include "lve_buffer.hpp"
#include "lve_frustum_culling.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include "stb_image.h"
namespace lve
{
    class LveModel
    {
    public:
        struct Vertex
        {
            glm::vec3 position{};
            glm::vec3 color{};
            glm::vec3 normal{};
            glm::vec2 uv{};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
            bool operator==(const Vertex &other) const
            {
                return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
            }
        };
        struct Builder
        {
            std::vector<Vertex> verties{};
            std::vector<uint32_t> indices{};

            void loadModel(const std::string &filepath);
        };

        LveModel(LveDevice &lveDevice, LveModel::Builder &builder);
        ~LveModel();

        static std::unique_ptr<LveModel> createModelFromFile(LveDevice &device, const std::string &filepath);

        LveModel(const LveModel &) = delete;
        LveModel &operator=(const LveModel &) = delete;

        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);
        static std::unique_ptr<LveModel> createModelFromVertices(
            LveDevice &device,
            const std::vector<Vertex> &vertices,
            const std::vector<uint32_t> &indices)
        {
            Builder builder;
            builder.verties = vertices;
            builder.indices = indices;
            return std::make_unique<LveModel>(device, builder);
        }
        uint32_t getIndexCount() const { return indexCount; }

        /// 获取模型在**局部空间**的轴对齐包围盒（用于视锥体剔除）
        const AABB& getAABB() const { return aabb; }

    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffers(const std::vector<uint32_t> &indices);
        LveDevice &lveDevice;
        std::unique_ptr<LveBuffer> vertexBuffer;
        uint32_t vertexCount;

        bool hasIndexBuffer = false;
        std::unique_ptr<LveBuffer> indexBuffer;
        uint32_t indexCount;

        // 模型在局部空间的轴对齐包围盒，顶点数据加载后计算
        AABB aabb{};
    };
    std::unique_ptr<LveModel> createTerrainModel(
        LveDevice &device,
        const std::string &heightmapPath, // 高度图文件路径
        float width,                      // 地形宽度（X轴长度）
        float depth,                      // 地形深度（Z轴长度）
        uint32_t segments,                // 每个维度上的分段数（顶点数 = segments+1）
        float minHeight,                  // 最低高度（对应高度图最暗处）
        float maxHeight,                  // 最高高度（对应高度图最亮处）
        const std::string &texturePath);  // 地形纹理（可选）

    float bilinearSample(const stbi_uc *data, int width, int height, float u, float v);
    static std::unique_ptr<LveModel> createSkyboxModel(LveDevice &device)
    {
        // 立方体顶点坐标（范围 -1 到 1）
        std::vector<glm::vec3> positions = {
            {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}, {-1, -1, 1}, // 底面
            {-1, 1, -1},
            {1, 1, -1},
            {1, 1, 1},
            {-1, 1, 1} // 顶面
        };
        // 索引（每个面两个三角形，共12个三角形）
        std::vector<uint32_t> indices = {
            // 底面
            0, 1, 2,
            2, 3, 0,
            // 顶面
            4, 6, 5,
            4, 7, 6,
            // 左面
            0, 4, 1,
            1, 4, 5,
            // 右面
            2, 6, 3,
            3, 6, 7,
            // 前面
            1, 5, 2,
            2, 5, 6,
            // 后面
            0, 3, 4,
            4, 3, 7};

        std::vector<LveModel::Vertex> vertices(positions.size());
        for (size_t i = 0; i < positions.size(); ++i)
        {
            vertices[i].position = positions[i];
            vertices[i].normal = glm::vec3(0.0f); // 不重要，因为我们会禁用背面剔除或反转法线
            vertices[i].color = glm::vec3(1.0f);
            vertices[i].uv = glm::vec2(0.0f);
        }

        return LveModel::createModelFromVertices(device, vertices, indices);
    };
}
namespace std
{
    template <>
    struct hash<lve::LveModel::Vertex>
    {
        size_t operator()(lve::LveModel::Vertex const &vertex) const
        {
            size_t seed = 0;
            lve::hash_combine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}
