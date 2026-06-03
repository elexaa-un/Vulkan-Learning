// Vulkan学习项目 — 地形高度采样器
// 从高度图图像中查询指定世界坐标位置的地形高度

#include <vector>
#include <stdexcept>
#include <string>
#include <algorithm>
#include "stb_image.h"

namespace lve
{
    // 地形高度采样器：加载高度图纹理，提供世界坐标到地形高度的查询功能
    class TerrainHeightSampler
    {
    public:
        // 构造函数：加载高度图并设置地形参数
        // 执行步骤：
        //   1. 通过 stb_image 库加载高度图图像（强制转为单通道灰度）
        //   2. 保存图像尺寸，将像素数据复制到内部缓冲区
        //   3. 释放 stb_image 分配的图像内存
        TerrainHeightSampler(const std::string &heightmapPath,
                             float terrainWidth, float terrainDepth,
                             float minH, float maxH)
            : m_width(terrainWidth), m_depth(terrainDepth),
              m_minHeight(minH), m_maxHeight(maxH)
        {
            int w, h, channels;
            unsigned char *data = stbi_load(heightmapPath.c_str(), &w, &h, &channels, 1); // 强制灰度
            if (!data)
                throw std::runtime_error("Failed to load heightmap for sampling");

            m_imgWidth = w;
            m_imgHeight = h;
            m_heightData.assign(data, data + w * h);
            stbi_image_free(data);
        }

        // 查询世界坐标 (x, z) 处的地形高度
        // 执行步骤：
        //   1. 将世界坐标映射到 [0,1] 纹理坐标范围（将世界原点映射到地形中心）
        //   2. 对纹理坐标进行 clamp 边界裁剪
        //   3. 使用最近邻采样将纹理坐标转换为像素坐标
        //   4. 从高度数据中读取灰度值，映射到 [minHeight, maxHeight] 范围并返回
        float getHeight(float worldX, float worldZ) const
        {
            // 世界坐标 → 纹理坐标 [0,1]
            float tx = (worldX + m_width * 0.5f) / m_width;
            float tz = (worldZ + m_depth * 0.5f) / m_depth;

            // 边界裁剪
            tx = std::clamp(tx, 0.0f, 1.0f);
            tz = std::clamp(tz, 0.0f, 1.0f);

            // 纹理坐标 → 像素坐标（最近邻，与 createTerrainModel 中一致）
            int px = static_cast<int>(tx * (m_imgWidth - 1) + 0.5f);
            int pz = static_cast<int>(tz * (m_imgHeight - 1) + 0.5f);
            px = std::clamp(px, 0, m_imgWidth - 1);
            pz = std::clamp(pz, 0, m_imgHeight - 1);

            float gray = m_heightData[pz * m_imgWidth + px] / 255.0f;
            return m_minHeight + gray * (m_maxHeight - m_minHeight);
        }

    private:
        int m_imgWidth, m_imgHeight;                // 高度图图像尺寸（像素）
        std::vector<unsigned char> m_heightData;     // 高度图灰度像素数据
        float m_width, m_depth;                      // 地形在世界空间中的宽度和深度
        float m_minHeight, m_maxHeight;              // 地形高度的最小/最大范围
    };
}
