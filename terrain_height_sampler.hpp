#include <vector>
#include <stdexcept>
#include <string>
#include <algorithm>
#include "stb_image.h"

namespace lve
{
    class TerrainHeightSampler
    {
    public:
        // 构造时传入高度图路径、地形世界宽度、深度、最小/最大高度
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
        int m_imgWidth, m_imgHeight;
        std::vector<unsigned char> m_heightData;
        float m_width, m_depth;
        float m_minHeight, m_maxHeight;
    };
}