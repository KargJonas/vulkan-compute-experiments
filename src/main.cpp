#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstdlib>

#include "./util.h"


// These are the steps we need to take in order to execute a compute shader
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


// Todo:
//   in order to progress toward a point where we can automatically schedule kernels on the gpu,
//   we will need an interface that lets us define the requirements of a kernel.
//   this program will take these requirements and create the necessary buffers for it.
//   when running multiple kernels, we will need to create multiple pipelines - one for each kernel.
//   buffer reuse optimization should be handled one layer of abstraction above this one -
//   in the frontend where we have access to the entire compute graph.


int main() {
    // Number of elements in each buffer
    const int nelem = 8;

    // Buffer types will be passed in from the frontend using string based type names for simplicity
    const size_t bufferSize = getTypeSize("float") * nelem;

    // Input data for demo purposes
    std::vector<float> dataA = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<float> dataB = {3, 1, 4, 1, 5, 9, 2, 6};

    VkInstance instance = createInstance(); // Create vk instance
    VkPhysicalDevice physicalDevice = selectPhysicalDevice(instance); // Select physical device
    uint32_t queueFamilyIndex = findComputeQueueFamily(physicalDevice); // Get index of the compute queue family
    uint32_t queueIndex = 0;
    VkDevice device = createDevice(physicalDevice, queueFamilyIndex);

    // Allocate memory on the GPU
    Buffer a = createBuffer(physicalDevice, device, bufferSize, 0);
    Buffer b = createBuffer(physicalDevice, device, bufferSize, 1);
    Buffer result = createBuffer(physicalDevice, device, bufferSize, 2);
    
    // TODO: We could make this an array
    std::vector<Buffer> bufferList = {a, b, result};

    // Transfer data from CPU to GPU
    copyToBuffer(device, a.deviceMemory, dataA.data(), 0, bufferSize);
    copyToBuffer(device, b.deviceMemory, dataB.data(), 0, bufferSize);

    // Descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout(device, bufferList);

    // Pipeline creation (each operation will have it's own pipeline)
    std::vector<char> shaderCode = readFile("shaders/add.spv");
    VkPipelineLayout pipelineLayout = createPipelineLayout(device, descriptorSetLayout);
    VkShaderModule computeShaderModule = createShaderModule(device, shaderCode);

    // Compute pipeline
    // TODO: Pipeline names should reflect the type of kernel it is running.
    VkPipeline computePipeline = createPipeline(device, pipelineLayout, computeShaderModule, "main");

    // Descriptor pool and descriptor sets
    VkDescriptorPool descriptorPool = createDescriptorPool(device, bufferList);
    VkDescriptorSet descriptorSet = createDescriptorSet(device, descriptorPool, descriptorSetLayout, bufferList);

    // Command buffer
    VkCommandPool commandPool = createCommandPool(device, queueFamilyIndex);
    VkCommandBuffer commandBuffer = createCommandBuffer(device, commandPool);

    VkQueue queue = getQueue(device, queueFamilyIndex, queueIndex);

    recordCommandBuffer(commandBuffer, computePipeline, pipelineLayout, descriptorSet, (uint32_t)ceil(nelem / 256.0), 1, 1);
    executeCommandBuffer(device, commandBuffer, queue);

    // Read back the result
    std::vector<float> resultData(nelem);
    copyBufferFromDevice(device, result.deviceMemory, resultData.data(), 0, bufferSize);

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

    // Buffer Cleanup
    destroyBuffers(bufferList);

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
