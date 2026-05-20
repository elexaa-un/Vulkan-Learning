#include "lve_descriptors.hpp"
#include "lve_utils.hpp"

// std
#include <stdexcept>
namespace lve
{

    // *************** Descriptor Set Layout Builder *********************

    LveDescriptorSetLayout::Builder &LveDescriptorSetLayout::Builder::addBinding(
        uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        uint32_t count)
    {
        LVE_ASSERT(bindings.count(binding) == 0, "Binding already in use");
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;
        layoutBinding.descriptorType = descriptorType;
        layoutBinding.descriptorCount = count;
        layoutBinding.stageFlags = stageFlags;
        bindings[binding] = layoutBinding;
        return *this;
    }

    std::unique_ptr<LveDescriptorSetLayout> LveDescriptorSetLayout::Builder::build() const
    {
        return std::make_unique<LveDescriptorSetLayout>(lveDevice, bindings);
    }

    // *************** Descriptor Set Layout *********************

    LveDescriptorSetLayout::LveDescriptorSetLayout(
        LveDevice &lveDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
        : lveDevice{lveDevice}, bindings{bindings}
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        for (auto kv : bindings)
        {
            setLayoutBindings.push_back(kv.second);
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(
            lveDevice.device(),
            &descriptorSetLayoutInfo,
            nullptr,
            &descriptorSetLayout));
    }

    LveDescriptorSetLayout::~LveDescriptorSetLayout()
    {
        vkDestroyDescriptorSetLayout(lveDevice.device(), descriptorSetLayout, nullptr);
    }

    // *************** Descriptor Pool Builder *********************

    LveDescriptorPool::Builder &LveDescriptorPool::Builder::addPoolSize(
        VkDescriptorType descriptorType, uint32_t count)
    {
        poolSizes.push_back({descriptorType, count});
        return *this;
    }

    LveDescriptorPool::Builder &LveDescriptorPool::Builder::setPoolFlags(
        VkDescriptorPoolCreateFlags flags)
    {
        poolFlags = flags;
        return *this;
    }
    LveDescriptorPool::Builder &LveDescriptorPool::Builder::setMaxSets(uint32_t count)
    {
        maxSets = count;
        return *this;
    }

    std::unique_ptr<LveDescriptorPool> LveDescriptorPool::Builder::build() const
    {
        return std::make_unique<LveDescriptorPool>(lveDevice, maxSets, poolFlags, poolSizes);
    }

    // *************** Descriptor Pool *********************

    LveDescriptorPool::LveDescriptorPool(
        LveDevice &lveDevice,
        uint32_t maxSets,
        VkDescriptorPoolCreateFlags poolFlags,
        const std::vector<VkDescriptorPoolSize> &poolSizes)
        : lveDevice{lveDevice}
    {
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        descriptorPoolInfo.maxSets = maxSets;
        descriptorPoolInfo.flags = poolFlags;

        VK_CHECK(vkCreateDescriptorPool(lveDevice.device(), &descriptorPoolInfo, nullptr, &descriptorPool));
    }

    LveDescriptorPool::~LveDescriptorPool()
    {
        vkDestroyDescriptorPool(lveDevice.device(), descriptorPool, nullptr);
    }

    bool LveDescriptorPool::allocateDescriptor(
        const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        allocInfo.descriptorSetCount = 1;

        // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
        // a new pool whenever an old pool fills up. But this is beyond our current scope
        if (vkAllocateDescriptorSets(lveDevice.device(), &allocInfo, &descriptor) != VK_SUCCESS)
        {
            return false;
        }
        return true;
    }

    void LveDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const
    {
        vkFreeDescriptorSets(
            lveDevice.device(),
            descriptorPool,
            static_cast<uint32_t>(descriptors.size()),
            descriptors.data());
    }

    void LveDescriptorPool::resetPool()
    {
        vkResetDescriptorPool(lveDevice.device(), descriptorPool, 0);
    }

    // *************** Descriptor Writer *********************

    LveDescriptorWriter::LveDescriptorWriter(LveDescriptorSetLayout &setLayout, LveDescriptorPool &pool)
        : setLayout{setLayout}, pool{pool} {}

    LveDescriptorWriter &LveDescriptorWriter::writeBuffer(
        uint32_t binding, VkDescriptorBufferInfo *bufferInfo)
    {
        LVE_ASSERT(setLayout.bindings.count(binding) == 1, "Layout does not contain specified binding");

        auto &bindingDescription = setLayout.bindings[binding];

        LVE_ASSERT(
            bindingDescription.descriptorCount == 1,
            "Binding single descriptor info, but binding expects multiple");

        // Store a copy of the buffer info so it stays valid after the caller's
        // local variable goes out of scope.
        bufferInfos.push_back(*bufferInfo);
        VkDescriptorBufferInfo *storedPtr = &bufferInfos.back();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.pBufferInfo = storedPtr;
        write.descriptorCount = 1;

        writes.push_back(write);
        return *this;
    }

    LveDescriptorWriter &LveDescriptorWriter::writeImage(
        uint32_t binding, VkDescriptorImageInfo *imageInfo)
    {
        LVE_ASSERT(setLayout.bindings.count(binding) == 1, "Layout does not contain specified binding");

        auto &bindingDescription = setLayout.bindings[binding];

        LVE_ASSERT(
            bindingDescription.descriptorCount == 1,
            "Binding single descriptor info, but binding expects multiple");

        // Store a copy of the image info so it stays valid after the caller's
        // local variable goes out of scope.
        imageInfos.push_back(*imageInfo);
        VkDescriptorImageInfo *storedPtr = &imageInfos.back();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType = bindingDescription.descriptorType;
        write.dstBinding = binding;
        write.pImageInfo = storedPtr;
        write.descriptorCount = 1;

        writes.push_back(write);
        return *this;
    }

    bool LveDescriptorWriter::build(VkDescriptorSet &set)
    {
        bool success = pool.allocateDescriptor(setLayout.getDescriptorSetLayout(), set);
        if (!success)
        {
            return false;
        }
        overwrite(set);
        return true;
    }

    void LveDescriptorWriter::overwrite(VkDescriptorSet &set)
    {
        for (auto &write : writes)
        {
            write.dstSet = set;
        }
        vkUpdateDescriptorSets(pool.lveDevice.device(), writes.size(), writes.data(), 0, nullptr);
    }

} // namespace lve
