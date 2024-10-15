#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <math.h>

struct ExtendedVkDescriptorBufferInfo : public VkDescriptorBufferInfo {
    uint32_t binding;
};

// Function to read the SPIR-V shader code from a file
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
   
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file!");
    }
   
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
   
    file.seekg(0);
    file.read(buffer.data(), fileSize);
   
    file.close();
    return buffer;
}

void copyBufferToDevice(VkDevice destDevice, VkDeviceMemory destDeviceMemory, void* srcBuffer, VkDeviceSize offset, VkDeviceSize bufferSize) {
    void* data;
    vkMapMemory(destDevice, destDeviceMemory, offset, bufferSize, 0, &data);
    memcpy(data, srcBuffer, bufferSize);
    vkUnmapMemory(destDevice, destDeviceMemory);
}

// Creates a buffer on a physical device, allocates memory and binds the buffer
void createBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceMemory* deviceMemory, std::vector<ExtendedVkDescriptorBufferInfo>* bufferDescriptors, VkBuffer* buffer, VkDeviceSize size, uint32_t binding) {
    
    // This struct is used to hold the information required for buffer creation
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    // Third param can be used for controlling allocation
    // Could be used for debugging purposes
    vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, buffer);

    // Get device specific memory requirements info like size, alignment and memory type
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    
    // Describes a number of memory heaps and memory types of the physical device that can be accessed
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    // Searches through the availably memory types provided by the physical device
    // to find one that satisfies both the buffer's memory requirements and the desired
    // properties for CPU access.
    uint32_t memoryTypeIndex = 0;
    bool found = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memoryTypeIndex = i;
            found = true;
            break;
        }
    }

    if (!found) throw std::runtime_error("Failed to find suitable memory type!");
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    // Allocate and bind
    vkAllocateMemory(logicalDevice, &allocInfo, nullptr, deviceMemory);
    vkBindBufferMemory(logicalDevice, *buffer, *deviceMemory, 0);

    ExtendedVkDescriptorBufferInfo bufferDescriptorInfo{};
    bufferDescriptorInfo.buffer = *buffer;
    bufferDescriptorInfo.offset = 0;
    bufferDescriptorInfo.range = size;
    bufferDescriptorInfo.binding = binding;
    bufferDescriptors->push_back(bufferDescriptorInfo);
}

int main() {
    // number of elements in array
    const int dataSize = 1024;

    // total size of array
    const size_t bufferSize = sizeof(float) * dataSize;

    // input arrays, filled with data
    std::vector<float> dataA(dataSize, 1.0f); // Initialize with 1.0
    std::vector<float> dataB(dataSize, 2.0f); // Initialize with 2.0

    // Vulkan instance creation
    VkInstance instance;
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Compute Shader Demo";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    // Physical device selection
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;  // global
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        VkPhysicalDeviceProperties deviceProperties;
        std::cout << "Found these devices:" << std::endl;

        for (const VkPhysicalDevice& device : devices) {
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            std::cout << "  Device Name: " << deviceProperties.deviceName << std::endl;
        }

        // here you could look into running kernels device-parallel
        // currently just picking the first device for simplicity
        physicalDevice = devices[0];
        vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
        std::cout << "Selected the following device: " << deviceProperties.deviceName << std::endl;
    }

    // Logical device and queue creation
    VkDevice device;   // global
    VkQueue computeQueue;
    uint32_t queueFamilyIndex;
    {
        // Find a queue family that supports compute operations
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        
        bool found = false;
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                queueFamilyIndex = i;
                found = true;
                break;
            }
        }
        if (!found) throw std::runtime_error("Failed to find a compute queue family!");

        // helps vk decide how to allocate gpu time between multiple queues
        // each queue can assign a value between 0.0 (lowest priority) and 1.0 (highest)
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; // helps identify this struct
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex; // defines which family this queue should be created in / belongs to
        queueCreateInfo.queueCount = 1; // sepcifies the number of queues to create in the family
        queueCreateInfo.pQueuePriorities = &queuePriority; // pointer to an array of priority floats, if queueCount is 1, you can just point to a regular float
        
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; // again, used to identify this struct
        // if you need to create queues in multiple families, you need multiple queueCreateInfo structs.
        // in that case you would set this int to the desired number of infos, and pass in a pointer
        // to an array of create infos in the pQueueCreateInfos field
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

        // use the physical device and the deviceCreateInfo (that also contains the queueCreateInfos) to
        // create a logical device. a reference to device will be stored in `device`
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device!");
        }

        // get a reference to the created queue.
        // (for this we need to specify the index of the family the queue belongs to, aswell as
        // the index of the queue in that family)
        uint32_t queueIndex = 0;
        vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &computeQueue);
    }

    // Create buffers
    VkBuffer bufferA, bufferB, bufferResult;
    VkDeviceMemory bufferMemoryA, bufferMemoryB, bufferMemoryResult;
    std::vector<ExtendedVkDescriptorBufferInfo> bufferDescriptors;
    {
        createBuffer(physicalDevice, device, &bufferMemoryA, &bufferDescriptors, &bufferA, bufferSize, 0);
        createBuffer(physicalDevice, device, &bufferMemoryB, &bufferDescriptors, &bufferB, bufferSize, 1);
        createBuffer(physicalDevice, device, &bufferMemoryResult, &bufferDescriptors, &bufferResult, bufferSize, 2);
    }

    // Map and copy data to buffers
    {
        copyBufferToDevice(device, bufferMemoryA, dataA.data(), 0, bufferSize);
        copyBufferToDevice(device, bufferMemoryB, dataB.data(), 0, bufferSize);
    }

    // Descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout;
    {
        size_t descriptorCount = bufferDescriptors.size();

        VkDescriptorSetLayoutBinding bindings[descriptorCount]{};

        for (int i = 0; i < descriptorCount; i++) {
            bindings[i].binding = bufferDescriptors[i].binding;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = descriptorCount;
        layoutInfo.pBindings = bindings;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    // Pipeline layout
    VkPipelineLayout pipelineLayout;
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }
    }

    // Shader module
    VkShaderModule computeShaderModule;
    {
        auto shaderCode = readFile("shaders/add.spv");

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        if (vkCreateShaderModule(device, &createInfo, nullptr, &computeShaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module!");
        }
    }

    // Compute pipeline
    VkPipeline computePipeline;
    {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = computeShaderModule;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = pipelineLayout;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline!");
        }
    }

    // Descriptor pool and descriptor sets
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    {
        size_t descriptorCount = bufferDescriptors.size();

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = descriptorCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set!");
        }

        VkWriteDescriptorSet descriptorWrites[descriptorCount]{};
        for (int i = 0; i < descriptorCount; i++) {
            descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[i].dstSet = descriptorSet;
            descriptorWrites[i].dstBinding = bufferDescriptors[i].binding;
            descriptorWrites[i].dstArrayElement = 0;
            descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[i].descriptorCount = 1;
            descriptorWrites[i].pBufferInfo = &bufferDescriptors[i];
        }

        vkUpdateDescriptorSets(device, descriptorCount, descriptorWrites, 0, nullptr);
    }

    // Command buffer
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer!");
        }
    }

    // Record command buffer
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdDispatch(commandBuffer, (uint32_t)ceil(dataSize / 256.0), 1, 1);

        vkEndCommandBuffer(commandBuffer);
    }

    // Execute command buffer
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkFence fence;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(device, &fenceInfo, nullptr, &fence);

        vkQueueSubmit(computeQueue, 1, &submitInfo, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

        vkDestroyFence(device, fence, nullptr);
    }

    // Read back the result
    std::vector<float> resultData(dataSize);
    {
        void* data;
        vkMapMemory(device, bufferMemoryResult, 0, bufferSize, 0, &data);
        memcpy(resultData.data(), data, bufferSize);   // segfault happens here.
        vkUnmapMemory(device, bufferMemoryResult);
    }

    // Verify the result
    bool success = true;
    for (int i = 0; i < dataSize; i++) {
        if (resultData[i] != dataA[i] + dataB[i]) {
            success = false;
            break;
        }
    }

    if (success) {
        std::cout << "Success: The computation result is correct." << std::endl;
    } else {
        std::cout << "Error: The computation result is incorrect." << std::endl;
    }

    // Cleanup
    vkDestroyBuffer(device, bufferA, nullptr);
    vkDestroyBuffer(device, bufferB, nullptr);
    vkDestroyBuffer(device, bufferResult, nullptr);
    vkFreeMemory(device, bufferMemoryA, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, computeShaderModule, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return 0;
}
