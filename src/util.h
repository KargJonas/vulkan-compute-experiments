#ifndef UTIL_H
#define UTIL_H

#include <vulkan/vulkan.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <sstream>
#include <math.h>

struct ExtendedVkDescriptorBufferInfo : public VkDescriptorBufferInfo {
    uint32_t binding;
};

struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory deviceMemory;
    VkDescriptorBufferInfo descriptorInfo;
    VkDevice device;
    uint32_t size;
    uint32_t binding;
};

std::vector<char> readFile(const std::string& filename);
VkInstance createInstance();
VkPhysicalDevice selectPhysicalDevice(VkInstance &instance);
uint32_t findComputeQueueFamily(VkPhysicalDevice physicalDevice);
VkDevice createDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);
VkQueue getQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex);
Buffer createBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize size, uint32_t binding);
void copyToBuffer(VkDevice destDevice, VkDeviceMemory destDeviceMemory, void* srcBuffer, VkDeviceSize offset, VkDeviceSize bufferSize);
void copyBufferFromDevice(VkDevice srcDevice, VkDeviceMemory srcDeviceMemory, void* destBuffer, VkDeviceSize offset, VkDeviceSize bufferSize);
VkCommandPool createCommandPool(VkDevice device, uint32_t computeQueueFamily);
VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool);
VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
VkPipeline createPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkShaderModule shaderModule, std::string name);
VkDescriptorPool createDescriptorPool(VkDevice device, std::vector<Buffer>& bufferDescriptors);
VkShaderModule createShaderModule(VkDevice device, std::vector<char>& shaderCode);
VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::vector<Buffer>& bufferDescriptors);
VkDescriptorSet createDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, std::vector<Buffer>& bufferDescriptors);
void recordCommandBuffer(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
void executeCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue);
void destroyBuffers(std::vector<Buffer>& buffers);

#endif // UTIL_H
