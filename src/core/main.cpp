#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

int main() {
    VkInstance instance;
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Vulkan failed to initialize. Check your drivers!" << std::endl;
        return -1;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::cout << "Vulkan initialized. Found " << deviceCount << " GPU(s)." << std::endl;

    vkDestroyInstance(instance, nullptr);
    return 0;
}