#include "./util.h"

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

VkInstance createInstance() {
    VkInstance instance;

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

    // Return by value is safe because VkInstance is implemented as a handle
    return instance;
}

VkPhysicalDevice selectPhysicalDevice(VkInstance &instance) {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

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

    return physicalDevice;
}

// Find a queue family that supports compute operations
uint32_t findComputeQueueFamily(VkPhysicalDevice physicalDevice) {
    uint32_t queueFamilyIndex;
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

    if (!found) {
        throw std::runtime_error("Failed to find a compute queue family!");
    }

    return queueFamilyIndex;
}

VkDevice createDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex) {
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
    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    return device;
}

VkQueue getQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex) {
    // get a reference to the created queue.
    // (for this we need to specify the index of the family the queue belongs to, aswell as
    // the index of the queue in that family)
    VkQueue computeQueue;
    vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, &computeQueue);

    return computeQueue;
}

// Creates a buffer on a physical device, allocates memory, and binds the buffer
Buffer createBuffer( VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size, uint32_t binding) {    
    VkBuffer buffer;
    VkDeviceMemory deviceMemory;

    // This struct is used to hold the information required for buffer creation
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    // Third param can be used for controlling allocation
    // Could be used for debugging purposes
    vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

    // Get device-specific memory requirements info like size, alignment, and memory type
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    // Describes a number of memory heaps and memory types of the physical device that can be accessed
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    // Searches through the available memory types provided by the physical device
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
    vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory);
    vkBindBufferMemory(device, buffer, deviceMemory, 0);

    VkDescriptorBufferInfo bufferDescriptorInfo{};
    bufferDescriptorInfo.buffer = buffer;
    bufferDescriptorInfo.offset = 0;
    bufferDescriptorInfo.range = size;

    Buffer b;
    b.buffer = buffer;
    b.deviceMemory = deviceMemory;
    b.descriptorInfo = bufferDescriptorInfo;
    b.device = device;
    b.size = size;
    b.binding = binding;

    return b;
}

// Copies data from CPU memory to VRAM
void copyToBuffer(VkDevice destDevice, VkDeviceMemory destDeviceMemory, void* srcBuffer, VkDeviceSize offset, VkDeviceSize bufferSize) {
    void* data;
    vkMapMemory(destDevice, destDeviceMemory, offset, bufferSize, 0, &data);
    memcpy(data, srcBuffer, bufferSize);
    vkUnmapMemory(destDevice, destDeviceMemory);
}

// Copies data from VRAM to CPU memory
void copyBufferFromDevice(VkDevice srcDevice, VkDeviceMemory srcDeviceMemory, void* destBuffer, VkDeviceSize offset, VkDeviceSize bufferSize) {
    void* data;
    vkMapMemory(srcDevice, srcDeviceMemory, 0, bufferSize, 0, &data);
    memcpy(destBuffer, data, bufferSize);   // segfault happens here.
    vkUnmapMemory(srcDevice, srcDeviceMemory);
}

VkPipelineLayout createPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout) {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    return pipelineLayout;
}

VkPipeline createPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkShaderModule shaderModule, std::string name) {
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = name.c_str();
    pipelineInfo.layout = pipelineLayout;

    VkPipeline computePipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        std::ostringstream msg;
        msg << "Failed to create compute pipeline \"" << name << "\".";
        throw std::runtime_error(msg.str());
    }

    return computePipeline;
}

VkDescriptorPool createDescriptorPool(VkDevice device, std::vector<Buffer>& buffers) {
    size_t descriptorCount = buffers.size();

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = descriptorCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    return descriptorPool;
}

VkDescriptorSet createDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, std::vector<Buffer>& buffers) {
    size_t descriptorCount = buffers.size();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites(descriptorCount);

    for (int i = 0; i < descriptorCount; i++) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = descriptorSet;
        descriptorWrites[i].dstBinding = buffers[i].binding;
        descriptorWrites[i].dstArrayElement = 0;
        descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].pBufferInfo = &buffers[i].descriptorInfo;
    }

    vkUpdateDescriptorSets(device, descriptorCount, descriptorWrites.data(), 0, nullptr);

    return descriptorSet;
}

VkShaderModule createShaderModule(VkDevice device, std::vector<char>& shaderCode) {  
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule computeShaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &computeShaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return computeShaderModule;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamilyIndex) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool commandPool;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    return commandPool;
}

VkCommandBuffer createCommandBuffer(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer!");
    }

    return commandBuffer;
}

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::vector<Buffer>& buffers) {
    size_t descriptorCount = buffers.size();
    std::vector<VkDescriptorSetLayoutBinding> bindings(descriptorCount);

    for (int i = 0; i < descriptorCount; i++) {
        bindings[i].binding = buffers[i].binding;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(descriptorCount);
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    return descriptorSetLayout;
}

void recordCommandBuffer(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

    vkEndCommandBuffer(commandBuffer);
}

void executeCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue) {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(device, &fenceInfo, nullptr, &fence);

    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(device, fence, nullptr);
}

void destroyBuffers(std::vector<Buffer>& buffers) {
    for (Buffer buffer : buffers) {
        vkDestroyBuffer(buffer.device, buffer.buffer, nullptr);
        vkFreeMemory(buffer.device, buffer.deviceMemory, nullptr);
    }
}
