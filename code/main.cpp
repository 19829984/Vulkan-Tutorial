#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

class HelloTriangleApplication {
public:
  void run() {
    initVulkan();
    initWindow();
    mainLoop();
    cleanup();
  }

private:
  void initVulkan() { createInstance(); }

  void createInstance() {
    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName = "Vulkan Tutorial",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14};

    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    std::cout << "available extensions:\n";
    for (const auto &extension : extensionProperties) {
      std::cout << '\t' << extension.extensionName << '\n';
    }
    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
      if (std::ranges::none_of(extensionProperties,
                               [glfwExtension = glfwExtensions[i]](
                                   auto const &extensionProperty) {
                                 return strcmp(extensionProperty.extensionName,
                                               glfwExtension) == 0;
                               })) {
        throw std::runtime_error("Required GLFW extension not supported: " +
                                 std::string(glfwExtensions[i]));
      }
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions};
    instance = vk::raii::Instance(context, createInfo);
  }

  void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window =
        glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    glfwDestroyWindow(window);

    glfwTerminate();
  }

private:
  // Vulkan
  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;

  // GLFW
  GLFWwindow *window;
};

int main() {
  HelloTriangleApplication app;

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
