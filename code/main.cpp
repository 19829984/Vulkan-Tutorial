#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ostream>
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
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>

struct Vertex
{
  glm::vec2 pos;
  glm::vec3 color;

  static vk::VertexInputBindingDescription getBindingDescription()
  {
    return { .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
  }
  static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
  {
    vk::VertexInputAttributeDescription posAttributeDesc = {
      .location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, pos)
    };
    decltype(posAttributeDesc) colorAttributeDesc = {
      .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)
    };
    return { posAttributeDesc, colorAttributeDesc };
  }
};

const std::vector<Vertex> vertices = { { { 0.0f, -0.5f }, { 1.0f, 1.0f, 1.0f } },
                                       { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
                                       { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } } };

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

const std::vector<char const*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
cosntexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

std::vector<const char*> requiredDeviceExtensions = { vk::KHRSwapchainExtensionName,
                                                      vk::KHRSpirv14ExtensionName,
                                                      vk::KHRSynchronization2ExtensionName,
                                                      vk::KHRCreateRenderpass2ExtensionName };

std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

class HelloTriangleApplication
{
public:
  void run()
  {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  void initVulkan()
  {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    recreateSwapChain();
    createGraphicsPipeline();
    createCommandPool();
    createCommandBuffers();
    createVertexBuffer();
    createSyncObjects();
  }

  void createInstance()
  {
    constexpr vk::ApplicationInfo appInfo{ .pApplicationName = "Vulkan Tutorial",
                                           .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                           .pEngineName = "No Engine",
                                           .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                           .apiVersion = vk::ApiVersion14 };
    std::vector<char const*> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    auto layerProperties = context.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) {
          return std::ranges::none_of(layerProperties, [requiredLayer](const auto& layerProperty) {
            return strcmp(layerProperty.layerName, requiredLayer) == 0;
          });
        })) {
      throw std::runtime_error("One or more required layers are not supported!");
    }

    auto requiredExtensions = getRequiredExtensions();

    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    std::cout << "available extensions:\n";
    for (const auto& extension : extensionProperties) {
      std::cout << '\t' << extension.extensionName << '\n';
    }
    for (uint32_t i = 0; i < requiredExtensions.size(); ++i) {
      if (std::ranges::none_of(extensionProperties,
                               [glfwExtension = requiredExtensions[i]](auto const& extensionProperty) {
                                 return strcmp(extensionProperty.extensionName, glfwExtension) == 0;
                               })) {
        throw std::runtime_error("Required GLFW extension not supported: " + std::string(requiredExtensions[i]));
      }
    }

    vk::InstanceCreateInfo createInfo{ .pApplicationInfo = &appInfo,
                                       .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
                                       .ppEnabledLayerNames = requiredLayers.data(),
                                       .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
                                       .ppEnabledExtensionNames = requiredExtensions.data() };
    instance = vk::raii::Instance(context, createInfo);
  }

  void createSurface()
  {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("Failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  bool isDeviceSuitable(const vk::raii::PhysicalDevice& device)
  {
    // Vulkan 1.3 version check
    bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

    // Supports graphics
    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();
    bool supportsGraphics = std::ranges::any_of(queueFamilies, [](vk::QueueFamilyProperties const& qfp) {
      return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
    });

    // Check Required Device Extension
    auto availableExtensions = device.enumerateDeviceExtensionProperties();
    bool supportsAllRequiredExtensions =
      std::ranges::all_of(requiredDeviceExtensions, [&availableExtensions](const auto& requiredDeviceExtension) {
        return std::ranges::any_of(availableExtensions,
                                   [&requiredDeviceExtension](const vk::ExtensionProperties& availableExtension) {
                                     return strcmp(availableExtension.extensionName, requiredDeviceExtension) == 0;
                                   });
      });

    // Check Required Features
    auto features = device.getFeatures2<vk::PhysicalDeviceFeatures2,
                                        vk::PhysicalDeviceVulkan11Features,
                                        vk::PhysicalDeviceVulkan13Features,
                                        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supportsRequiredFeatures =
      features.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
      features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
      features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
      features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

    return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
  }

  void pickPhysicalDevice()
  {
    std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    std::cout << "Number of devices: " << devices.size() << std::endl;

    const auto devIter =
      std::ranges::find_if(devices, [&](const vk::raii::PhysicalDevice& device) { return isDeviceSuitable(device); });

    if (devIter == devices.end()) {
      throw std::runtime_error("Failed to fidn a suitable GPU!");
    }

    physicalDevice = *devIter;
  }

  void createLogicalDevice()
  {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    size_t numFamilyProperties = queueFamilyProperties.size();
    auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const& qfp) {
      return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
    });
    graphicsFamilyIndex =
      static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

    // Check if graphicsIndex also supports presentation
    presentFamilyIndex = physicalDevice.getSurfaceSupportKHR(graphicsFamilyIndex, *surface)
                           ? graphicsFamilyIndex
                           : static_cast<uint32_t>(numFamilyProperties);
    if (presentFamilyIndex == numFamilyProperties) {
      // Otherwise, find a new family index that does support both
      for (uint32_t qFamilyIndex = 0; qFamilyIndex < numFamilyProperties; ++qFamilyIndex) {
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
        for (uint32_t qFamilyIndex = 0; qFamilyIndex < numFamilyProperties; ++qFamilyIndex) {

          auto qfp = queueFamilyProperties[qFamilyIndex];
          if (physicalDevice.getSurfaceSupportKHR(qFamilyIndex, *surface)) {
            presentFamilyIndex = qFamilyIndex;
            break;
          }
        }
      }
    }
    if ((graphicsFamilyIndex == numFamilyProperties) || (presentFamilyIndex == numFamilyProperties)) {
      throw std::runtime_error("Could not find a queue for graphics or present");
    }

    float queuePriority = 0.5f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{ .queueFamilyIndex = graphicsFamilyIndex,
                                                     .queueCount = 1,
                                                     .pQueuePriorities = &queuePriority };

    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                       vk::PhysicalDeviceShaderDrawParametersFeatures>
      featureChain{ {},
                    { .synchronization2 = true, .dynamicRendering = true },
                    { .extendedDynamicState = true },
                    { .shaderDrawParameters = true } };

    vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                           .queueCreateInfoCount = 1,
                                           .pQueueCreateInfos = &deviceQueueCreateInfo,
                                           .enabledExtensionCount =
                                             static_cast<uint32_t>(requiredDeviceExtensions.size()),
                                           .ppEnabledExtensionNames = requiredDeviceExtensions.data() };

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);

    graphicsQueue = vk::raii::Queue(device, graphicsFamilyIndex, 0);
    presentQueue = vk::raii::Queue(device, presentFamilyIndex, 0);
  }

  uint32_t findQueueFamilies(vk::raii::PhysicalDevice physicalDevice)
  {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    uint32_t queueIndex = ~0;
    for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyProperties.size(); ++queueFamilyIndex) {
      auto qfp = queueFamilyProperties[queueFamilyIndex];
      if (qfp.queueFlags & vk::QueueFlagBits::eGraphics &&
          physicalDevice.getSurfaceSupportKHR(queueFamilyIndex, *surface)) {
        queueIndex = queueFamilyIndex;
        break;
      }
    }
    if (queueIndex == ~0) {
      throw std::runtime_error("Could not find a queue for graphis and surface");
    }
    return queueIndex;
  }

  std::vector<const char*> getRequiredExtensions()
  {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
      extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
  }

  void createSwapChain()
  {
    auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    swapChainSurfaceFormat = chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));
    swapChainImageFormat = swapChainSurfaceFormat.format;
    swapChainExtent = chooseSwapExtent(surfaceCapabilities);
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount)
                      ? surfaceCapabilities.maxImageCount
                      : minImageCount;
    vk::SwapchainCreateInfoKHR swapChainCreateInfo{ .flags = vk::SwapchainCreateFlagsKHR(),
                                                    .surface = *surface,
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
                                                      physicalDevice.getSurfacePresentModesKHR(*surface)),
                                                    .clipped = vk::True,
                                                    .oldSwapchain = nullptr };
    uint32_t queueFamilyIndices[] = { graphicsFamilyIndex, presentFamilyIndex };

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

  void cleanupSwapChain()
  {
    device.waitIdle();
    swapChainImageViews.clear();
    swapChainImages.clear();
    swapChain = nullptr;
  }

  void recreateSwapChain()
  {
    cleanupSwapChain();

    createSwapChain();
    createImageViews();
  }

  void createImageViews()
  {
    swapChainImageViews.clear();

    vk::ImageViewCreateInfo imageViewCreateInfo{ .viewType = vk::ImageViewType::e2D,
                                                 .format = swapChainImageFormat,
                                                 .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };
    for (auto& image : swapChainImages) {
      imageViewCreateInfo.image = image;
      swapChainImageViews.emplace_back(device, imageViewCreateInfo);
    }
  }

  void createGraphicsPipeline()
  {
    auto shaderModule = createShaderModule(readFile("shaders/slang.spv"));

    std::cout << "Shader module created" << std::endl;
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eVertex,
                                                           .module = shaderModule,
                                                           .pName = "vertMain" };

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eFragment,
                                                           .module = shaderModule,
                                                           .pName = "fragMain" };

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{ .topology = vk::PrimitiveTopology::eTriangleList };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescription = Vertex::getAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{ .vertexBindingDescriptionCount = 1,
                                                            .pVertexBindingDescriptions = &bindingDescription,
                                                            .vertexAttributeDescriptionCount =
                                                              attributeDescription.size(),
                                                            .pVertexAttributeDescriptions =
                                                              attributeDescription.data() };

    vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                     .pDynamicStates = dynamicStates.data() };

    // vk::Viewport viewport{.x = 0.0f,
    //                       .y = 0.0f,
    //                       .width = static_cast<float>(swapChainExtent.width),
    //                       .height =
    //                       static_cast<float>(swapChainExtent.height),
    //                       .minDepth = 0.0f,
    //                       .maxDepth = 1.0f};
    // vk::Rect2D scissor{.offset = vk::Offset2D(0, 0), .extent =
    // swapChainExtent};

    vk::PipelineViewportStateCreateInfo viewportStateInfo{
      .viewportCount = 1, .pViewports = {}, .scissorCount = 1, .pScissors = {}
    };

    vk::PipelineRasterizationStateCreateInfo rasterizerInfo{ .depthClampEnable = vk::False,
                                                             .rasterizerDiscardEnable = vk::False,
                                                             .polygonMode = vk::PolygonMode::eFill,
                                                             .cullMode = vk::CullModeFlagBits::eBack,
                                                             .frontFace = vk::FrontFace::eClockwise,
                                                             .depthBiasEnable = vk::False,
                                                             .depthBiasSlopeFactor = 1.0f,
                                                             .lineWidth = 1.0f };
    vk::PipelineMultisampleStateCreateInfo multisamplingInfo{ .rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                              .sampleShadingEnable = vk::False };
    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = vk::False,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    vk::PipelineColorBlendStateCreateInfo colorBlendingInfo{ .logicOpEnable = vk::False,
                                                             .logicOp = vk::LogicOp::eCopy,
                                                             .attachmentCount = 1,
                                                             .pAttachments = &colorBlendAttachment };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{ .setLayoutCount = 0, .pushConstantRangeCount = 0 };

    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    // Dynamic Rendering
    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
      { .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pViewportState = &viewportStateInfo,
        .pRasterizationState = &rasterizerInfo,
        .pMultisampleState = &multisamplingInfo,
        .pColorBlendState = &colorBlendingInfo,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = nullptr },
      { .colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainImageFormat }
    };

    graphicsPipeline =
      vk::raii::Pipeline(device, VK_NULL_HANDLE, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
  }

  void createVertexBuffer()
  {
    vk::BufferCreateInfo bufferInfo{ .size = sizeof(vertices[0]) * vertices.size(),
                                     .usage = vk::BufferUsageFlagBits::eVertexBuffer,
                                     .sharingMode = vk::SharingMode::eExclusive };
    vertexBuffer = vk::raii::Buffer(device, bufferInfo);

    vk::MemoryRequirements memRequirements = vertexBuffer.getMemoryRequirements();
    vk::MemoryAllocateInfo memoryAllocateInfo = { .allocationSize = memRequirements.size,
                                                  .memoryTypeIndex =
                                                    findMemoryType(memRequirements.memoryTypeBits,
                                                                   vk::MemoryPropertyFlagBits::eHostVisible |
                                                                     vk::MemoryPropertyFlagBits::eHostCoherent) };
    vertexBufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
    vertexBuffer.bindMemory(*vertexBufferMemory, 0);

    void* data = vertexBufferMemory.mapMemory(0, bufferInfo.size);
    memcpy(data, vertices.data(), bufferInfo.size);
    vertexBufferMemory.unmapMemory();
  }

  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
  {
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
        return i;
      }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
  }
  void createCommandPool()
  {
    vk::CommandPoolCreateInfo poolInfo{ .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                        .queueFamilyIndex = graphicsFamilyIndex };
    commandPool = vk::raii::CommandPool(device, poolInfo);
  }

  void createCommandBuffers()
  {
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool,
                                             .level = vk::CommandBufferLevel::ePrimary,
                                             .commandBufferCount = MAX_FRAMES_IN_FLIGHT };
    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  }

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
  {
    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
          availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return availableFormat;
      }
    }
    return availableFormats[0];
  }

  vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
  {
    for (const auto& availablePresentMode : availablePresentModes) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        return availablePresentMode;
      }
    }
    return availablePresentModes[0];
  }

  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
  {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
      std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
      std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
  }

  void initWindow()
  {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);
  }

  void mainLoop()
  {
    // for (uint32_t i = 0; i < 1000; ++i) {

#if defined(DEBUG)
    std::cout << "Debug Mode Enabled" << std::endl;
#endif
    std::cout << "Entering Main Loop" << std::endl;
    // }
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      drawFrame();
      // std::cout << "Framing" << std::endl;
    }
    device.waitIdle();
    std::cout << "Loop exit" << std::endl;
  }

  void drawFrame()
  {
    auto& fence = *inFlightFences[frameIndex];
    auto& presentCompleteSemaphore = *presentCompleteSemaphores[frameIndex];
    auto& commandBuffer = commandBuffers[frameIndex];
    // Fence
    auto fenceResult = device.waitForFences(fence, vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess) {
      throw std::runtime_error("Failed to wait for fence!");
    }

    // Acquire Image
    auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, presentCompleteSemaphore, nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR) {
      recreateSwapChain();
      return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
      throw std::runtime_error("Failed to acquire swap chain image!");
    }

    device.resetFences(fence);
    commandBuffer.reset();
    recordCommandBuffer(imageIndex);

    // Submit to graphics queue
    auto& renderFinishedSemaphore = *renderFinishedSemaphores[imageIndex];
    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submitInfo{ .waitSemaphoreCount = 1,
                                     .pWaitSemaphores = &presentCompleteSemaphore,
                                     .pWaitDstStageMask = &waitDestinationStageMask,
                                     .commandBufferCount = 1,
                                     .pCommandBuffers = &*commandBuffer,
                                     .signalSemaphoreCount = 1,
                                     .pSignalSemaphores = &renderFinishedSemaphore };
    graphicsQueue.submit(submitInfo, fence);

    // Submit to Present Queue
    const vk::PresentInfoKHR presentInfoKHR{ .waitSemaphoreCount = 1,
                                             .pWaitSemaphores = &renderFinishedSemaphore,
                                             .swapchainCount = 1,
                                             .pSwapchains = &*swapChain,
                                             .pImageIndices = &imageIndex };
    result = presentQueue.presentKHR(presentInfoKHR);
    switch (result) {
      case vk::Result::eSuccess:
        break;
      case vk::Result::eSuboptimalKHR:
      case vk::Result::eErrorOutOfDateKHR:
        recreateSwapChain();
        std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
        break;
      default:
        break; // an unexpected result is returned!
    }

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  void recordCommandBuffer(uint32_t imageIndex)
  {
    auto& commandBuffer = commandBuffers[frameIndex];
    commandBuffer.begin({});

    transition_image_layout(imageIndex,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            {},
                            vk::AccessFlagBits2::eColorAttachmentWrite,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::ClearValue clearColor = vk::ClearColorValue(.0f, .0f, .0f, .1f);
    vk::RenderingAttachmentInfo attachmentInfo = { .imageView = swapChainImageViews[imageIndex],
                                                   .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                   .loadOp = vk::AttachmentLoadOp::eClear,
                                                   .storeOp = vk::AttachmentStoreOp::eStore,
                                                   .clearValue = clearColor };
    vk::RenderingInfo renderingInfo = { .renderArea = { .offset = { 0, 0 }, .extent = swapChainExtent },
                                        .layerCount = 1,
                                        .colorAttachmentCount = 1,
                                        .pColorAttachments = &attachmentInfo };
    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
    commandBuffer.setViewport(
      0,
      vk::Viewport(
        .0f, .0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), .0f, .1f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

    commandBuffer.bindVertexBuffers(0, *vertexBuffer, { 0 });

    commandBuffer.draw(vertices.size(), 1, 0, 0);
    commandBuffer.endRendering();

    transition_image_layout(imageIndex,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::ePresentSrcKHR,
                            vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
                            {},                                                 // dstAccessMask
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
                            vk::PipelineStageFlagBits2::eBottomOfPipe           // dstStage
    );
    commandBuffer.end();
  }

  void createSyncObjects()
  {
    assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());
    for (size_t i = 0; i < swapChainImages.size(); ++i) {
      renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
      inFlightFences.emplace_back(device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });
    }
  }

  void transition_image_layout(uint32_t imageIndex,
                               vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout,
                               vk::AccessFlags2 src_access_mask,
                               vk::AccessFlags2 dst_access_mask,
                               vk::PipelineStageFlags2 src_stage_mask,
                               vk::PipelineStageFlags2 dst_stage_mask)
  {
    vk::ImageMemoryBarrier2 barrier = { .srcStageMask = src_stage_mask,
                                        .srcAccessMask = src_access_mask,
                                        .dstStageMask = dst_stage_mask,
                                        .dstAccessMask = dst_access_mask,
                                        .oldLayout = old_layout,
                                        .newLayout = new_layout,
                                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                        .image = swapChainImages[imageIndex],
                                        .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1 } };
    vk::DependencyInfo dependency_info = { .dependencyFlags = {},
                                           .imageMemoryBarrierCount = 1,
                                           .pImageMemoryBarriers = &barrier };
    commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
  }

  void cleanup()
  {
    cleanupSwapChain();
    std::cout << "Entering cleanup" << std::endl;
    glfwDestroyWindow(window);

    glfwTerminate();
    std::cout << "GLFW Terminated" << std::endl;
  }

  [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
  {
    vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size() * sizeof(char),
                                           .pCode = reinterpret_cast<const uint32_t*>(code.data()) };

    return vk::raii::ShaderModule(device, createInfo);
  }

  static std::vector<char> readFile(const std::string& fileName)
  {
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error(std::format("Failed to open file at {}", fileName));
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    assert(file.is_open() == false);
    return buffer;
  }

#pragma region Debug
  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                        vk::DebugUtilsMessageTypeFlagsEXT type,
                                                        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void*)
  {
    std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
  }

  void setupDebugMessenger()
  {
    if (!enableValidationLayers)
      return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{ .messageSeverity = severityFlags,
                                                                           .messageType = messageTypeFlags,
                                                                           .pfnUserCallback = &debugCallback };
    debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
  }
#pragma endregion

private:
  // GLFW
  GLFWwindow* window = nullptr;
  // Vulkan
  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;
  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;
  vk::raii::Queue graphicsQueue = nullptr;
  vk::raii::Queue presentQueue = nullptr;
  vk::raii::SwapchainKHR swapChain = nullptr;
  std::vector<vk::Image> swapChainImages;
  std::vector<vk::raii::ImageView> swapChainImageViews;
  vk::SurfaceFormatKHR swapChainSurfaceFormat;
  vk::Extent2D swapChainExtent;
  vk::Format swapChainImageFormat = vk::Format::eUndefined;
  uint32_t graphicsFamilyIndex = 0;
  uint32_t presentFamilyIndex = 0;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline graphicsPipeline = nullptr;
  vk::raii::CommandPool commandPool = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers;
  uint32_t frameIndex = 0;

  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;

  vk::raii::Buffer vertexBuffer = nullptr;
  vk::raii::DeviceMemory vertexBufferMemory = nullptr;
};

int
main()
{
  HelloTriangleApplication app;

  try {
    app.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
