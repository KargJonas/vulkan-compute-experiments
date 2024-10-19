#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstdlib>


#include "./util.h"


// these are the steps we need to take in order to execute a compute shader
// 1. cpu buffer creation
// 2. vk instance creation
// 3. physical device selection
// 4. logical device/queue creation
// 5. buffer creation
// 6. buffer transfer
// 7. descriptor set layout definitions
// 8. pipeline layout definition
// 9. shader code loading / shader module creation
// 10. compute pipeline creation
// 11. descriptor pool creation
// 12. command buffer creation
// 13. command buffer filling ("recording")
// 14. command buffer execution
// 15. reverse buffer transfer
// 16. cleanup

/**
 * Todo: introduce a Kernel struct and std::vector<Kernel> kernels;
 * the struct will hold all objects/configuration relevant to one kernel.
 * at teardown we can iterate over all kernels to free memory.
 */


int main() {
    // number of elements in array
    // const int dataSize = 800000000;
    const int nelem = 8;

    // total size of array
    const size_t bufferSize = sizeof(float) * nelem;

    // input arrays, filled with data
    std::vector<float> dataA(nelem, 1.0f); // Initialize with 1.0
    std::vector<float> dataB(nelem, 2.0f); // Initialize with 2.0

    VkInstance instance = createInstance(); // create vk instance
    VkPhysicalDevice physicalDevice = selectPhysicalDevice(instance); // select physical device
    uint32_t queueFamilyIndex = findComputeQueueFamily(physicalDevice); // get index of the compute queue family
    uint32_t queueIndex = 0;
    VkDevice device = createDevice(physicalDevice, queueFamilyIndex);

    // Allocate memory on the GPU
    VkBuffer bufferA, bufferB, bufferResult;
    VkDeviceMemory bufferMemoryA, bufferMemoryB, bufferMemoryResult;
    std::vector<ExtendedVkDescriptorBufferInfo> bufferDescriptors;
    createBuffer(physicalDevice, device, &bufferMemoryA, &bufferDescriptors, &bufferA, bufferSize, 0);
    createBuffer(physicalDevice, device, &bufferMemoryB, &bufferDescriptors, &bufferB, bufferSize, 1);
    createBuffer(physicalDevice, device, &bufferMemoryResult, &bufferDescriptors, &bufferResult, bufferSize, 2);

    // Transfer data from CPU to GPU
    copyBufferToDevice(device, bufferMemoryA, dataA.data(), 0, bufferSize);
    copyBufferToDevice(device, bufferMemoryB, dataB.data(), 0, bufferSize);

    // Descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout(device, bufferDescriptors);

    // Pipeline creation (each operation will have it's own pipeline)
    std::vector<char> shaderCode = readFile("shaders/add.spv");
    VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorSetLayout);
    VkShaderModule computeShaderModule = createShaderModule(device, shaderCode);

    // Compute pipeline
    VkPipeline computePipeline = createPipeline(device, pipelineLayout, computeShaderModule, "main");

    // Descriptor pool and descriptor sets
    VkDescriptorPool descriptorPool = createDescriptorPool(device, bufferDescriptors);
    VkDescriptorSet descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout, bufferDescriptors);

    // Command buffer
    VkCommandPool commandPool = createCommandPool(device, queueFamilyIndex);
    VkCommandBuffer commandBuffer = createCommandBuffer(device, commandPool);

    VkQueue queue = getQueue(device, queueFamilyIndex, queueIndex);

    uint32_t groupSizeX = (uint32_t)ceil(nelem / 256.0);
    uint32_t groupSizeY = 1;
    uint32_t groupSizeZ = 1;
    recordCommandBuffer(commandBuffer, computePipeline, pipelineLayout, descriptorSet, groupSizeX, groupSizeY, groupSizeZ);
    executeCommandBuffer(device, commandBuffer, queue);

    // Read back the result
    std::vector<float> resultData(nelem);
    copyBufferFromDevice(device, bufferMemoryResult, resultData.data(), 0, bufferSize);

    // Verify the result
    bool success = true;
    for (int i = 0; i < nelem; i++) {
        std::cout << resultData[i] << "  ";

        if (resultData[i] != dataA[i] + dataB[i]) {
            success = false;
            break;
        }
    }

    std::cout << std::endl;

    if (success) std::cout << "Success: The computation result is correct." << std::endl;
    else std::cout << "Error: The computation result is incorrect." << std::endl;

    // TODO: introduce central descriptor registry that can later be used for tear down
    //   also, this is likely better than extending the VkDescriptorBufferInfo struct with the binding info

    // Buffer Cleanup
    vkDestroyBuffer(device, bufferA, nullptr);
    vkDestroyBuffer(device, bufferB, nullptr);
    vkDestroyBuffer(device, bufferResult, nullptr);
    vkFreeMemory(device, bufferMemoryA, nullptr);
    vkFreeMemory(device, bufferMemoryB, nullptr);
    vkFreeMemory(device, bufferMemoryResult, nullptr);

    // Descriptor Cleanup
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    // MISC
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, computeShaderModule, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
