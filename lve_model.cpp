#include "lve_model.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#define TINYOBJLOADER_DO_NOT_USE_SIMD 1
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "lve_model.hpp"
#include "lve_texture.hpp" // 如果纹理单独处理，可以不在这里加载纹理，只返回模型
#include "stb_image.h"
#include <cmath>
namespace lve
{
    LveModel::LveModel(LveDevice &device, LveModel::Builder &builder) : lveDevice{device}
    {
        createVertexBuffers(builder.verties);
        createIndexBuffers(builder.indices);
    }
    LveModel::~LveModel()
    {
    }

    std::unique_ptr<LveModel> LveModel::createModelFromFile(LveDevice &device, const std::string &filepath)
    {
        Builder builder;
        builder.loadModel(filepath);
        std::cout << "Vertex Count:" << builder.verties.size() << "\n";
        return std::make_unique<LveModel>(device, builder);
    }

    void LveModel::createVertexBuffers(const std::vector<Vertex> &vertices)
    {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
        uint32_t vertexSize = sizeof(vertices[0]);
        LveBuffer stagingBuffer{
            lveDevice,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)vertices.data());

        vertexBuffer = std::make_unique<LveBuffer>(
            lveDevice,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        lveDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
    }

    void LveModel::createIndexBuffers(const std::vector<uint32_t> &indices)
    {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;
        if (!hasIndexBuffer)
        {
            return;
        }
        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
        uint32_t indexSize = sizeof(indices[0]);
        LveBuffer stagingBuffer{
            lveDevice,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)indices.data());

        indexBuffer = std::make_unique<LveBuffer>(
            lveDevice,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        lveDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
    }

    void LveModel::draw(VkCommandBuffer commandBuffer)
    {
        if (hasIndexBuffer)
        {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void LveModel::bind(VkCommandBuffer commandBuffer)
    {
        VkBuffer buffers[] = {vertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (hasIndexBuffer)
        {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    std::vector<VkVertexInputBindingDescription> LveModel::Vertex::getBindingDescriptions()
    {
        std::vector<VkVertexInputBindingDescription> bindingDescription(1);
        bindingDescription[0].binding = 0;
        bindingDescription[0].stride = sizeof(Vertex);
        bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    std::vector<VkVertexInputAttributeDescription> LveModel::Vertex::getAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescription{};

        attributeDescription.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
        attributeDescription.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
        attributeDescription.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
        attributeDescription.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

        return attributeDescription;
    }

    void LveModel::Builder::loadModel(const std::string &filepath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str()))
        {
            throw std::runtime_error(warn + err);
        }

        verties.clear();
        indices.clear();
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};
        for (const auto &shape : shapes)
        {
            for (auto &index : shape.mesh.indices)
            {
                Vertex vertex{};
                if (index.vertex_index >= 0)
                {
                    vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2],
                    };
                }
                if (attrib.colors.size() > 0)
                {
                    vertex.color = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2],
                    };
                }
                if (index.normal_index >= 0)
                {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2],
                    };
                }

                if (index.texcoord_index >= 0)
                {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1],
                    };
                }

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(verties.size());
                    verties.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    std::unique_ptr<LveModel> createTerrainModel(
        LveDevice &device,
        const std::string &heightmapPath,
        float width, float depth,
        uint32_t segments,
        float minHeight, float maxHeight,
        const std::string &texturePath)
    {
        // 1. 加载高度图
        int imgWidth, imgHeight, channels;
        stbi_uc *heightData = stbi_load(heightmapPath.c_str(), &imgWidth, &imgHeight, &channels, 1); // 灰度图
        if (!heightData)
        {
            throw std::runtime_error("Failed to load heightmap: " + heightmapPath);
        }
        // 简单起见，我们假设 heightmap 尺寸至少为 segments+1，如果不是则缩放采样，这里不做复杂处理，要求高度图尺寸匹配
        // 实际中你可以按比例采样，但为了简单，要求 heightmap 宽度 = segments+1
        if (imgWidth != (int)(segments + 1) || imgHeight != (int)(segments + 1))
        {
            std::cerr << "Warning: heightmap size (" << imgWidth << "x" << imgHeight
                      << ") does not match segments (" << segments << "). Will stretch." << std::endl;
        }

        uint32_t vertexCount = (segments + 1) * (segments + 1);
        uint32_t indexCount = segments * segments * 6; // 每个小方格2个三角形=6个索引

        std::vector<LveModel::Vertex> vertices(vertexCount);
        std::vector<uint32_t> indices(indexCount);

        float halfWidth = width * 0.5f;
        float halfDepth = depth * 0.5f;
        float stepX = width / (float)segments;
        float stepZ = depth / (float)segments;

        // 2. 生成顶点位置和 UV（纹理坐标基于 XZ 平面，重复 tiling）
        for (uint32_t z = 0; z <= segments; ++z)
        {
            for (uint32_t x = 0; x <= segments; ++x)
            {
                uint32_t idx = z * (segments + 1) + x;
                float worldX = -halfWidth + x * stepX;
                float worldZ = -halfDepth + z * stepZ;

                // 从高度图采样高度 (使用双线性插值提高质量，这里简化取最近像素)
                float u = (float)x / segments;
                float v = (float)z / segments;
                int px = (int)(u * (imgWidth - 1));
                int py = (int)(v * (imgHeight - 1));
                float gray = heightData[py * imgWidth + px] / 255.0f;
                float height = minHeight + gray * (maxHeight - minHeight);

                vertices[idx].position = {worldX, height, worldZ};
                // UV: 让纹理在整个地形上重复多次，比如平铺 10 次
                vertices[idx].uv = {u * 10.0f, v * 10.0f};
                // 颜色暂时设为白色，法线后面计算
                vertices[idx].color = {1.0f, 1.0f, 1.0f};
                vertices[idx].normal = {0.0f, 1.0f, 0.0f}; // 临时
            }
        }

        // 3. 计算法线
        for (uint32_t z = 0; z <= segments; ++z)
        {
            for (uint32_t x = 0; x <= segments; ++x)
            {
                uint32_t idx = z * (segments + 1) + x;
                glm::vec3 sum(0.0f);
                // 累积相邻三角形法线
                // 右下三角形 (x, z) - (x+1, z) - (x+1, z+1)
                if (x < segments && z < segments)
                {
                    glm::vec3 p0 = vertices[idx].position;
                    glm::vec3 p1 = vertices[z * (segments + 1) + (x + 1)].position;
                    glm::vec3 p2 = vertices[(z + 1) * (segments + 1) + (x + 1)].position;
                    glm::vec3 normal = glm::normalize(glm::cross(p2 - p0, p1 - p0));
                    sum += normal;
                }
                // 左下三角形 (x, z) - (x+1, z+1) - (x, z+1)
                if (x < segments && z < segments)
                {
                    glm::vec3 p0 = vertices[idx].position;
                    glm::vec3 p1 = vertices[(z + 1) * (segments + 1) + (x + 1)].position;
                    glm::vec3 p2 = vertices[(z + 1) * (segments + 1) + x].position;
                    glm::vec3 normal = glm::normalize(glm::cross(p2 - p0, p1 - p0));
                    sum += normal;
                }
                // 平均化
                if (sum != glm::vec3(0.0f))
                    vertices[idx].normal = glm::normalize(sum);
                else
                    vertices[idx].normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        // 4. 生成索引
        uint32_t index = 0;
        for (uint32_t z = 0; z < segments; ++z)
        {
            for (uint32_t x = 0; x < segments; ++x)
            {
                uint32_t i0 = z * (segments + 1) + x;
                uint32_t i1 = z * (segments + 1) + (x + 1);
                uint32_t i2 = (z + 1) * (segments + 1) + x;
                uint32_t i3 = (z + 1) * (segments + 1) + (x + 1);

                // 三角形1: i0 - i1 - i2
                indices[index++] = i0;
                indices[index++] = i1;
                indices[index++] = i2;
                // 三角形2: i1 - i3 - i2
                indices[index++] = i1;
                indices[index++] = i3;
                indices[index++] = i2;
            }
        }

        stbi_image_free(heightData);

        // 5. 创建模型
        return LveModel::createModelFromVertices(device, vertices, indices);
    }
}