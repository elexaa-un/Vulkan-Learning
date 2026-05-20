#include "lve_texture_manager.hpp"
#include "lve_utils.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace lve
{
    // ------------------------------------------------------------------
    // Static member definitions (ODR requirement)
    // ------------------------------------------------------------------
    VkSampler LveTextureManager::s_fallbackSampler = VK_NULL_HANDLE;
    VkImageView LveTextureManager::s_fallbackImageView = VK_NULL_HANDLE;
    VkImage LveTextureManager::s_fallbackImage = VK_NULL_HANDLE;
    VkDeviceMemory LveTextureManager::s_fallbackImageMemory = VK_NULL_HANDLE;

    // ------------------------------------------------------------------
    // Helper: find a suitable memory type index on the physical device.
    // ------------------------------------------------------------------
    static uint32_t findMemoryType(
        VkPhysicalDevice physicalDevice,
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("LveTextureManager: failed to find suitable memory type!");
    }

    // ------------------------------------------------------------------
    // Helper: allocate and begin a single-use command buffer.
    // ------------------------------------------------------------------
    static VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        return commandBuffer;
    }

    // ------------------------------------------------------------------
    // Helper: end and submit a single-use command buffer, then free it.
    // ------------------------------------------------------------------
    static void endSingleTimeCommands(
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue graphicsQueue,
        VkCommandBuffer commandBuffer)
    {
        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(graphicsQueue));

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    // ------------------------------------------------------------------
    // createDefaultSampler
    // ------------------------------------------------------------------
    VkSampler LveTextureManager::createDefaultSampler(
        VkDevice device,
        VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE; // fallback sampler, no anisotropy
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f; // no mipmaps

        VkSampler sampler;
        VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));

        std::cout << "LveTextureManager: fallback sampler created." << std::endl;
        return sampler;
    }

    // ------------------------------------------------------------------
    // createWhiteImageView
    // ------------------------------------------------------------------
    VkImageView LveTextureManager::createWhiteImageView(
        VkDevice device,
        VkPhysicalDevice physicalDevice)
    {
        // --- Determine a graphics queue for submission ---
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t graphicsQueueFamily = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsQueueFamily = i;
                break;
            }
        }
        if (graphicsQueueFamily == UINT32_MAX)
        {
            throw std::runtime_error("LveTextureManager: no graphics queue family found!");
        }

        VkQueue graphicsQueue;
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

        // --- Create a temporary command pool ---
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;

        VkCommandPool commandPool;
        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

        // --- 1x1 white pixel data ---
        const uint8_t whitePixel[4] = {255, 255, 255, 255};
        VkDeviceSize imageSize = 4; // 1x1 RGBA

        // --- Staging buffer ---
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer));

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memReq);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            physicalDevice,
            memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory));
        VK_CHECK(vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0));

        // Map and write white pixel
        void *data;
        VK_CHECK(vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data));
        std::memcpy(data, whitePixel, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        // --- Create the 1x1 image ---
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {1, 1, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &s_fallbackImage));

        vkGetImageMemoryRequirements(device, s_fallbackImage, &memReq);

        VkMemoryAllocateInfo imageAllocInfo{};
        imageAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        imageAllocInfo.allocationSize = memReq.size;
        imageAllocInfo.memoryTypeIndex = findMemoryType(
            physicalDevice,
            memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(device, &imageAllocInfo, nullptr, &s_fallbackImageMemory));
        VK_CHECK(vkBindImageMemory(device, s_fallbackImage, s_fallbackImageMemory, 0));

        // --- Transition layout and copy ---
        VkCommandBuffer cmd = beginSingleTimeCommands(device, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = s_fallbackImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {1, 1, 1};

        vkCmdCopyBufferToImage(
            cmd,
            stagingBuffer,
            s_fallbackImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        endSingleTimeCommands(device, commandPool, graphicsQueue, cmd);

        // --- Cleanup staging resources ---
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        // --- Create image view ---
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = s_fallbackImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageView));

        // Destroy the temporary command pool
        vkDestroyCommandPool(device, commandPool, nullptr);

        std::cout << "LveTextureManager: fallback 1x1 white texture created." << std::endl;
        return imageView;
    }

    // ------------------------------------------------------------------
    // cleanupFallbackResources
    // ------------------------------------------------------------------
    void LveTextureManager::cleanupFallbackResources(VkDevice device)
    {
        if (s_fallbackSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device, s_fallbackSampler, nullptr);
            s_fallbackSampler = VK_NULL_HANDLE;
        }
        if (s_fallbackImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, s_fallbackImageView, nullptr);
            s_fallbackImageView = VK_NULL_HANDLE;
        }
        if (s_fallbackImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, s_fallbackImage, nullptr);
            s_fallbackImage = VK_NULL_HANDLE;
        }
        if (s_fallbackImageMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, s_fallbackImageMemory, nullptr);
            s_fallbackImageMemory = VK_NULL_HANDLE;
        }

        std::cout << "LveTextureManager: fallback resources cleaned up." << std::endl;
    }

} // namespace lve
