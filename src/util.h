#ifndef UTIL_H
#define UTIL_H

#include <vulkan/vulkan.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>

struct ExtendedVkDescriptorBufferInfo : public VkDescriptorBufferInfo {
    uint32_t binding;
};

std::vector<char> readFile(const std::string& filename);
VkInstance createInstance();
VkPhysicalDevice selectPhysicalDevice(VkInstance &instance);
uint32_t findComputeQueueFamily(VkPhysicalDevice physicalDevice);
void copyBufferToDevice(VkDevice destDevice, VkDeviceMemory destDeviceMemory, void* srcBuffer, VkDeviceSize offset, VkDeviceSize bufferSize);
void copyBufferFromDevice(VkDevice srcDevice, VkDeviceMemory srcDeviceMemory, void* destBuffer, VkDeviceSize offset, VkDeviceSize bufferSize);
void createBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceMemory* deviceMemory, std::vector<ExtendedVkDescriptorBufferInfo>* bufferDescriptors, VkBuffer* buffer, VkDeviceSize size, uint32_t binding);

#endif // UTIL_H