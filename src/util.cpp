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

// Copies data from CPU memory to VRAM
void copyBufferToDevice(VkDevice destDevice, VkDeviceMemory destDeviceMemory, void* srcBuffer, VkDeviceSize offset, VkDeviceSize bufferSize) {
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