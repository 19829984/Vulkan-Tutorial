#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan_core.h>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};
#ifdef NDEBUG
cosntexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

std::vector<const char *> deviceExtensions = {
    vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName,
    vk::KHRCreateRenderpass2ExtensionName};

std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                               vk::DynamicState::eScissor};

class HelloTriangleApplication {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
  }

  void createInstance() {
    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName = "Vulkan Tutorial",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14};
    std::vector<char const *> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    auto layerProperties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(
            requiredLayers, [&layerProperties](auto const &requiredLayer) {
              return std::ranges::none_of(
                  layerProperties, [requiredLayer](const auto &layerProperty) {
                    return strcmp(layerProperty.layerName, requiredLayer) == 0;
                  });
            })) {
      throw std::runtime_error(
          "One or more required layers are not supported!");
    }

    auto requiredExtensions = getRequiredExtensions();

    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    std::cout << "available extensions:\n";
    for (const auto &extension : extensionProperties) {
      std::cout << '\t' << extension.extensionName << '\n';
    }
    for (uint32_t i = 0; i < requiredExtensions.size(); ++i) {
      if (std::ranges::none_of(extensionProperties,
                               [glfwExtension = requiredExtensions[i]](
                                   auto const &extensionProperty) {
                                 return strcmp(extensionProperty.extensionName,
                                               glfwExtension) == 0;
                               })) {
        throw std::runtime_error("Required GLFW extension not supported: " +
                                 std::string(requiredExtensions[i]));
      }
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()};
    instance = vk::raii::Instance(context, createInfo);
  }

  void createSurface() {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("Failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  void pickPhysicalDevice() {
    std::vector<vk::raii::PhysicalDevice> devices =
        instance.enumeratePhysicalDevices();
    std::cout << "Number of devices: " << devices.size() << std::endl;
    const auto devIter = std::ranges::find_if(devices, [&](auto const &device) {
      std::vector<vk::QueueFamilyProperties> queueFamilies =
          device.getQueueFamilyProperties();
      bool isSuitable = device.getProperties().apiVersion >= VK_API_VERSION_1_3;
      // Supports graphics
      const auto qfpIter = std::ranges::find_if(
          queueFamilies, [](vk::QueueFamilyProperties const &qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                   static_cast<vk::QueueFlags>(0);
          });
      isSuitable = isSuitable && (qfpIter != queueFamilies.end());

      auto extensions = device.enumerateDeviceExtensionProperties();
      bool found = true;

      // Check all required device extensions are present
      for (auto const &deviceExtension : deviceExtensions) {
        auto extensionIter = std::ranges::find_if(
            extensions, [deviceExtension](auto const &ext) {
              return strcmp(ext.extensionName, deviceExtension) == 0;
            });
        found = found && extensionIter != extensions.end();
      }
      isSuitable = isSuitable && found;
      if (isSuitable) {
        physicalDevice = device;
      }
      return isSuitable;
    });

    if (devIter == devices.end()) {
      throw std::runtime_error("Failed to fidn a suitable GPU!");
    }
  }

  void createLogicalDevice() {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
        physicalDevice.getQueueFamilyProperties();
    size_t numFamilyProperties = queueFamilyProperties.size();
    auto graphicsQueueFamilyProperty =
        std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {
          return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                 static_cast<vk::QueueFlags>(0);
        });
    graphicsFamilyIndex = static_cast<uint32_t>(std::distance(
        queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

    // Check if graphicsIndex also supports presentation
    presentFamilyIndex =
        physicalDevice.getSurfaceSupportKHR(graphicsFamilyIndex, *surface)
            ? graphicsFamilyIndex
            : static_cast<uint32_t>(numFamilyProperties);
    if (presentFamilyIndex == numFamilyProperties) {
      // Otherwise, find a new family index that does support both
      for (uint32_t qFamilyIndex = 0; qFamilyIndex < numFamilyProperties;
           ++qFamilyIndex) {
        auto qfp = queueFamilyProperties[qFamilyIndex];
        if (qfp.queueFlags & vk::QueueFlagBits::eGraphics &&
            physicalDevice.getSurfaceSupportKHR(qFamilyIndex, *surface)) {
          graphicsFamilyIndex = qFamilyIndex;
          presentFamilyIndex = graphicsFamilyIndex;
          break;
        }
      }
      if (presentFamilyIndex == numFamilyProperties) {
        // No single family that supports both
        for (uint32_t qFamilyIndex = 0; qFamilyIndex < numFamilyProperties;
             ++qFamilyIndex) {

          auto qfp = queueFamilyProperties[qFamilyIndex];
          if (physicalDevice.getSurfaceSupportKHR(qFamilyIndex, *surface)) {
            presentFamilyIndex = qFamilyIndex;
            break;
          }
        }
      }
    }
    if ((graphicsFamilyIndex == numFamilyProperties) ||
        (presentFamilyIndex == numFamilyProperties)) {
      throw std::runtime_error(
          "Could not find a queue for graphics or present");
    }

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex = graphicsFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority};

    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                       vk::PhysicalDeviceShaderDrawParametersFeatures>
        featureChain{{},
                     {.dynamicRendering = true},
                     {.extendedDynamicState = true},
                     {.shaderDrawParameters = true}};

    vk::DeviceCreateInfo deviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()};

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);

    graphicsQueue = vk::raii::Queue(device, graphicsFamilyIndex, 0);
    presentQueue = vk::raii::Queue(device, presentFamilyIndex, 0);
  }

  uint32_t findQueueFamilies(vk::raii::PhysicalDevice physicalDevice) {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
        physicalDevice.getQueueFamilyProperties();
    uint32_t queueIndex = ~0;
    for (uint32_t queueFamilyIndex = 0;
         queueFamilyIndex < queueFamilyProperties.size(); ++queueFamilyIndex) {
      auto qfp = queueFamilyProperties[queueFamilyIndex];
      if (qfp.queueFlags & vk::QueueFlagBits::eGraphics &&
          physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, *surface)) {
        queueIndex = queueFamilyIndex;
        break;
      }
    }
    if (queueIndex == ~0) {
      throw std::runtime_error(
          "Could not find a queue for graphis and surface");
    }
    return queueIndex;
  }

  std::vector<const char *> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
      extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
  }

  void createSwapChain() {
    auto surfaceCapabilities =
        physicalDevice.getSurfaceCapabilitiesKHR(surface);
    swapChainSurfaceFormat =
        chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
    swapChainImageFormat = swapChainSurfaceFormat.format;
    swapChainExtent = chooseSwapExtent(surfaceCapabilities);
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    minImageCount = (surfaceCapabilities.maxImageCount > 0 &&
                     minImageCount > surfaceCapabilities.maxImageCount)
                        ? surfaceCapabilities.maxImageCount
                        : minImageCount;
    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .flags = vk::SwapchainCreateFlagsKHR(),
        .surface = surface,
        .minImageCount = minImageCount,
        .imageFormat = swapChainImageFormat,
        .imageColorSpace = swapChainSurfaceFormat.colorSpace,
        .imageExtent = swapChainExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = chooseSwapPresentMode(
            physicalDevice.getSurfacePresentModesKHR(surface)),
        .clipped = vk::True,
        .oldSwapchain = nullptr};
    uint32_t queueFamilyIndices[] = {graphicsFamilyIndex, presentFamilyIndex};

    if (graphicsFamilyIndex != presentFamilyIndex) {
      swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
      swapChainCreateInfo.queueFamilyIndexCount = 2;
      swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
      swapChainCreateInfo.queueFamilyIndexCount = 0;     // Optional
      swapChainCreateInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();
  }

  void createImageViews() {
    swapChainImageViews.clear();

    vk::ImageViewCreateInfo imageViewCreateInfo{
        .viewType = vk::ImageViewType::e2D,
        .format = swapChainImageFormat,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    for (auto &image : swapChainImages) {
      imageViewCreateInfo.image = image;
      swapChainImageViews.emplace_back(device, imageViewCreateInfo);
    }
  }

  void createGraphicsPipeline() {
    auto shaderModule = createShaderModule(readFile("shaders/slang.spv"));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shaderModule,
        .pName = "vertMain"};

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName = "fragMain"};

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
        .topology = vk::PrimitiveTopology::eTriangleList};
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    // vk::Viewport viewport{.x = 0.0f,
    //                       .y = 0.0f,
    //                       .width = static_cast<float>(swapChainExtent.width),
    //                       .height =
    //                       static_cast<float>(swapChainExtent.height),
    //                       .minDepth = 0.0f,
    //                       .maxDepth = 1.0f};
    // vk::Rect2D scissor{.offset = vk::Offset2D(0, 0), .extent =
    // swapChainExtent};

    vk::PipelineViewportStateCreateInfo viewportStateInfo{.viewportCount = 1,
                                                          .pViewports = {},
                                                          .scissorCount = 1,
                                                          .pScissors = {}};

    vk::PipelineRasterizationStateCreateInfo rasterizerInfo{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = vk::False,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f};
    // vk::PipelineMultisampleStateCreateInfo multisamplingInfo{
    //     .rasterizationSamples = vk::SampleCountFlagBits::e1,
    //     .sampleShadingEnable = vk::False};
    // vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{
    // };
    //
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    vk::PipelineColorBlendStateCreateInfo colorBlendingInfo{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 0, .pushConstantRangeCount = 0};

    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
  }

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
          availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return availableFormat;
      }
    }
    return availableFormats[0];
  }

  vk::PresentModeKHR chooseSwapPresentMode(
      const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode : availablePresentModes) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        return availablePresentMode;
      }
    }
    return availablePresentModes[0];
  }

  vk::Extent2D
  chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                             capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height),
    };
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

  [[nodiscard]] vk::raii::ShaderModule
  createShaderModule(const std::vector<char> &code) const {
    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(code.data())};

    return vk::raii::ShaderModule(device, createInfo);
  }

  static std::vector<char> readFile(const std::string &fileName) {
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error(
          std::format("Failed to open file at {}", fileName));
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    assert(file.is_open() == false);
    return buffer;
  }

#pragma region Debug
  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
    std::cerr << "validation layer: type " << to_string(type)
              << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
  }

  void setupDebugMessenger() {
    if (!enableValidationLayers)
      return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        .messageSeverity = severityFlags,
        .messageType = messageTypeFlags,
        .pfnUserCallback = &debugCallback};
    debugMessenger =
        instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
  }
#pragma endregion

private:
  // Vulkan
  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;
  uint32_t graphicsFamilyIndex = 0;
  uint32_t presentFamilyIndex = 0;
  vk::raii::Queue graphicsQueue = nullptr;
  vk::raii::Queue presentQueue = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;
  vk::raii::SwapchainKHR swapChain = nullptr;
  std::vector<vk::Image> swapChainImages;
  std::vector<vk::raii::ImageView> swapChainImageViews;
  vk::SurfaceFormatKHR swapChainSurfaceFormat;
  vk::Extent2D swapChainExtent;
  vk::Format swapChainImageFormat = vk::Format::eUndefined;
  vk::raii::PipelineLayout pipelineLayout = nullptr;

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
