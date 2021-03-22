#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "RaZ/Math/Angle.hpp"
#include "RaZ/Math/Matrix.hpp"
#include "RaZ/Math/Transform.hpp"
#include "RaZ/Render/Camera.hpp"
#include "RaZ/Render/RendererVk.hpp"
#include "RaZ/Utils/FilePath.hpp"
#include "RaZ/Utils/Image.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>

using namespace std::literals;

namespace Raz {

namespace {

struct Vertex {
  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription {};

    bindingDescription.binding   = 0;
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions {};

    // Position
    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset   = 0; // offsetof(Vertex, position)

    // Texcoords
    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset   = sizeof(Vertex::position); // offsetof(Vertex, texcoords)

    // Color
    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset   = sizeof(Vertex::position) + sizeof(Vertex::texcoords); // offsetof(Vertex, color)

    return attributeDescriptions;
  }

  Vec3f position;
  Vec2f texcoords;
  Vec3f color;
};

struct UniformMatrices {
  Mat4f model {};
  Mat4f view {};
  Mat4f projection {};
};

constexpr std::array<Vertex, 4> vertices = {
  Vertex { Vec3f(-0.5f, -0.5f, 0.f), Vec2f(0.f, 0.f), Vec3f(1.f, 0.f, 0.f) },
  Vertex { Vec3f( 0.5f, -0.5f, 0.f), Vec2f(1.f, 0.f), Vec3f(0.f, 1.f, 0.f) },
  Vertex { Vec3f( 0.5f,  0.5f, 0.f), Vec2f(1.f, 1.f), Vec3f(0.f, 0.f, 1.f) },
  Vertex { Vec3f(-0.5f,  0.5f, 0.f), Vec2f(0.f, 1.f), Vec3f(1.f, 1.f, 1.f) }
};

constexpr std::array<uint32_t, 6> indices = {
  0, 1, 2,
  2, 3, 0
};

///////////////////////
// Validation layers //
///////////////////////

constexpr std::array<const char*, 1> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

inline bool checkValidationLayersSupport() {
  uint32_t layerCount {};
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char* layerName : validationLayers) {
    bool layerFound = false;

    for (const VkLayerProperties& layerProperties : availableLayers) {
      if (std::strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound)
      return false;
  }

  return true;
}

////////////////////
// Debug Callback //
////////////////////

inline VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT /* messageSeverity */,
                                                    VkDebugUtilsMessageTypeFlagsEXT /* messageType */,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                    void* /* userData */) {
  std::cerr << "[Validation layer] Error: " << callbackData->pMessage << std::endl;

  return VK_FALSE;
}

inline VkResult createDebugUtilsMessengerEXT(VkInstance instance,
                                             const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
                                             const VkAllocationCallbacks* allocator,
                                             VkDebugUtilsMessengerEXT& debugMessenger) {
  const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

  if (func != nullptr)
    return func(instance, createInfo, allocator, &debugMessenger);

  return VK_ERROR_EXTENSION_NOT_PRESENT;
}

inline void destroyDebugUtilsMessengerEXT(VkInstance instance,
                                          VkDebugUtilsMessengerEXT debugMessenger,
                                          const VkAllocationCallbacks* allocator) {
  const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

  if (func != nullptr)
    func(instance, debugMessenger, allocator);
}

constexpr VkDebugUtilsMessengerCreateInfoEXT createDebugMessengerCreateInfo(void* userData = nullptr) noexcept {
  VkDebugUtilsMessengerCreateInfoEXT createInfo {};

  createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                             //| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
  createInfo.pUserData       = userData;

  return createInfo;
}

inline void setupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger, void* userData = nullptr) {
  const VkDebugUtilsMessengerCreateInfoEXT createInfo = createDebugMessengerCreateInfo(userData);

  if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, debugMessenger) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to set up debug messenger.");
}

////////////////
// Extensions //
////////////////

inline std::vector<const char*> getRequiredExtensions() {
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions {};
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#if !defined(NDEBUG)
  extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  return extensions;
}

////////////////////
// Queue families //
////////////////////

struct QueueFamilyIndices {
  constexpr bool isComplete() const noexcept { return graphicsFamily.has_value() && presentFamily.has_value(); }

  std::optional<uint32_t> graphicsFamily {};
  std::optional<uint32_t> presentFamily {};
};

inline QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilyIndices queueIndices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  for (std::size_t queueIndex = 0; queueIndex < queueFamilies.size(); ++queueIndex) {
    if (queueFamilies[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      queueIndices.graphicsFamily = static_cast<uint32_t>(queueIndex);

      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(queueIndex), surface, &presentSupport);

      if (presentSupport)
        queueIndices.presentFamily = static_cast<uint32_t>(queueIndex);

      if (queueIndices.isComplete())
        break;
    }
  }

  return queueIndices;
}

///////////////
// Swapchain //
///////////////

constexpr std::array<const char*, 1> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

inline bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount {};
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

  std::size_t validExtensionCount = 0;

  for (const char* extensionName : deviceExtensions) {
    for (const VkExtensionProperties& extensionProperties : availableExtensions) {
      if (std::strcmp(extensionName, extensionProperties.extensionName) == 0)
        ++validExtensionCount;
    }
  }

  return (validExtensionCount == deviceExtensions.size());
}

struct SwapchainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities {};
  std::vector<VkSurfaceFormatKHR> formats {};
  std::vector<VkPresentModeKHR> presentModes {};
};

inline VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
  for (const VkSurfaceFormatKHR& availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return availableFormat;
  }

  return availableFormats.front();
}

inline VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
  for (const VkPresentModeKHR& availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
      return availablePresentMode;
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

inline VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* windowHandle) {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    return capabilities.currentExtent;

  // Width & height may be user-defined
  int width {};
  int height {};
  glfwGetWindowSize(windowHandle, &width, &height);

  VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

  actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return actualExtent;
}

inline SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
  SwapchainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  uint32_t formatCount {};
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount {};
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

inline void createSwapchain(VkSurfaceKHR surface,
                            GLFWwindow* windowHandle,
                            VkSwapchainKHR& swapchain,
                            std::vector<VkImage>& swapchainImages,
                            VkFormat& swapchainImageFormat,
                            VkExtent2D& swapchainExtent,
                            VkPhysicalDevice physicalDevice,
                            VkDevice logicalDevice) {
  const SwapchainSupportDetails swapchainSupport = querySwapchainSupport(physicalDevice, surface);

  const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
  const VkPresentModeKHR presentMode     = chooseSwapPresentMode(swapchainSupport.presentModes);
  const VkExtent2D extent                = chooseSwapExtent(swapchainSupport.capabilities, windowHandle);

  uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;

  if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount)
    imageCount = swapchainSupport.capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR swapchainCreateInfo {};
  swapchainCreateInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCreateInfo.surface          = surface;
  swapchainCreateInfo.minImageCount    = imageCount;
  swapchainCreateInfo.imageFormat      = surfaceFormat.format;
  swapchainCreateInfo.imageColorSpace  = surfaceFormat.colorSpace;
  swapchainCreateInfo.imageExtent      = extent;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  const QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice, surface);

  if (queueIndices.graphicsFamily != queueIndices.presentFamily) {
    const std::array<uint32_t, 2> queueFamilyIndices = { queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value() };

    swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    swapchainCreateInfo.queueFamilyIndexCount = 2;
    swapchainCreateInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
  } else {
    swapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 0;
    swapchainCreateInfo.pQueueFamilyIndices   = nullptr;
  }

  swapchainCreateInfo.preTransform   = swapchainSupport.capabilities.currentTransform;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCreateInfo.presentMode    = presentMode;
  swapchainCreateInfo.clipped        = VK_TRUE;
  swapchainCreateInfo.oldSwapchain   = nullptr;

  if (vkCreateSwapchainKHR(logicalDevice, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a swapchain.");

  vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, nullptr);

  swapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, swapchainImages.data());

  swapchainImageFormat = surfaceFormat.format;
  swapchainExtent      = extent;
}

/////////////////////
// Physical device //
/////////////////////

inline bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);

  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  // TODO: pick the best device

  if (!deviceFeatures.samplerAnisotropy)
    return false;

  if (!findQueueFamilies(device, surface).isComplete())
    return false;

  if (!checkDeviceExtensionSupport(device))
    return false;

  const SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device, surface);

  return (!swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty());
}

/////////////
// Shaders //
/////////////

inline std::vector<char> readFile(const std::string& filePath) {
  std::ifstream file(filePath, std::ios::in | std::ios::binary | std::ios::ate);

  if (!file)
    throw std::runtime_error("Error: Couldn't open the file '" + filePath + "'.");

  const auto fileSize = static_cast<std::size_t>(file.tellg());
  file.seekg(0, std::ios::beg);

  std::vector<char> bytes(fileSize);
  file.read(bytes.data(), static_cast<std::streamsize>(fileSize));

  return bytes;
}

/////////////////
// Image views //
/////////////////

inline void createImageViews(std::vector<VkImageView>& swapchainImageViews,
                             const std::vector<VkImage>& swapchainImages,
                             VkFormat swapchainImageFormat) {
  swapchainImageViews.resize(swapchainImages.size());

  for (std::size_t i = 0; i < swapchainImages.size(); ++i) {
    Renderer::createImageView(swapchainImageViews[i],
                              swapchainImages[i],
                              ImageViewType::IMAGE_2D,
                              swapchainImageFormat,
                              ComponentSwizzle::IDENTITY,
                              ComponentSwizzle::IDENTITY,
                              ComponentSwizzle::IDENTITY,
                              ComponentSwizzle::IDENTITY,
                              ImageAspect::COLOR,
                              0,
                              1,
                              0,
                              1);
  }
}

//////////////
// Pipeline //
//////////////

inline void createGraphicsPipeline(VkPipeline& graphicsPipeline,
                                   VkPipelineLayout& pipelineLayout,
                                   const std::string& vertexShaderPath,
                                   const std::string& fragmentShaderPath,
                                   VkExtent2D swapchainExtent,
                                   VkDescriptorSetLayout descriptorSetLayout,
                                   VkRenderPass renderPass,
                                   VkDevice logicalDevice) {
  const std::vector<char> vertShaderCode = readFile(vertexShaderPath);
  const std::vector<char> fragShaderCode = readFile(fragmentShaderPath);

  VkShaderModule vertShaderModule {};
  Renderer::createShaderModule(vertShaderModule, vertShaderCode.size(), vertShaderCode.data());

  VkShaderModule fragShaderModule {};
  Renderer::createShaderModule(fragShaderModule, fragShaderCode.size(), fragShaderCode.data());

  VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
  vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage  = static_cast<VkShaderStageFlagBits>(ShaderStage::VERTEX);
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName  = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
  fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage  = static_cast<VkShaderStageFlagBits>(ShaderStage::FRAGMENT);
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName  = "main";

  const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

  const VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
  const std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = Vertex::getAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
  vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount   = 1;
  vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
  inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport {};
  viewport.x        = 0.f;
  viewport.y        = 0.f;
  viewport.width    = static_cast<float>(swapchainExtent.width);
  viewport.height   = static_cast<float>(swapchainExtent.height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  VkRect2D scissor {};
  scissor.offset = { 0, 0 };
  scissor.extent = swapchainExtent;

  VkPipelineViewportStateCreateInfo viewportState {};
  viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports    = &viewport;
  viewportState.scissorCount  = 1;
  viewportState.pScissors     = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer {};
  rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode             = static_cast<VkPolygonMode>(PolygonMode::FILL);
  rasterizer.lineWidth               = 1.f;
  rasterizer.cullMode                = static_cast<uint32_t>(CullingMode::BACK);
  rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable         = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.f;
  rasterizer.depthBiasClamp          = 0.f;
  rasterizer.depthBiasSlopeFactor    = 0.f;

  VkPipelineMultisampleStateCreateInfo multisampling {};
  multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable   = VK_FALSE;
  multisampling.rasterizationSamples  = static_cast<VkSampleCountFlagBits>(SampleCount::ONE);
  multisampling.minSampleShading      = 1.f;
  multisampling.pSampleMask           = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable      = VK_FALSE;

  VkPipelineColorBlendAttachmentState colorBlendAttachment {};
  colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable         = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending {};
  colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable     = VK_FALSE;
  colorBlending.logicOp           = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount   = 1;
  colorBlending.pAttachments      = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.f;
  colorBlending.blendConstants[1] = 0.f;
  colorBlending.blendConstants[2] = 0.f;
  colorBlending.blendConstants[3] = 0.f;

  Renderer::createPipelineLayout(pipelineLayout, { descriptorSetLayout }, {});

  VkGraphicsPipelineCreateInfo pipelineInfo {};
  pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages             = shaderStages.data();
  pipelineInfo.pVertexInputState   = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState      = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState   = &multisampling;
  pipelineInfo.pDepthStencilState  = nullptr;
  pipelineInfo.pColorBlendState    = &colorBlending;
  pipelineInfo.pDynamicState       = nullptr;
  pipelineInfo.layout              = pipelineLayout;
  pipelineInfo.renderPass          = renderPass;
  pipelineInfo.subpass             = 0;
  pipelineInfo.basePipelineHandle  = nullptr;
  pipelineInfo.basePipelineIndex   = -1;

  if (vkCreateGraphicsPipelines(logicalDevice, nullptr, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a graphics pipeline.");

  Renderer::destroyShaderModule(fragShaderModule);
  Renderer::destroyShaderModule(vertShaderModule);
}

//////////////////
// Framebuffers //
//////////////////

inline void createFramebuffers(std::vector<VkFramebuffer>& swapchainFramebuffers,
                               const std::vector<VkImageView>& swapchainImageViews,
                               VkRenderPass renderPass,
                               VkExtent2D swapchainExtent,
                               VkDevice logicalDevice) {
  swapchainFramebuffers.resize(swapchainImageViews.size());

  for (std::size_t i = 0; i < swapchainImageViews.size(); ++i) {
    const std::array<VkImageView, 1> attachments = { swapchainImageViews[i] };

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = swapchainExtent.width;
    framebufferInfo.height          = swapchainExtent.height;
    framebufferInfo.layers          = 1;

    if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
      throw std::runtime_error("Error: Failed to create a framebuffer.");
  }
}

/////////////
// Texture //
/////////////

void transitionImageLayout(VkImage image,
                           ImageLayout oldLayout,
                           ImageLayout newLayout,
                           ImageAspect imgAspect,
                           VkFormat imageFormat,
                           VkCommandPool commandPool,
                           VkQueue graphicsQueue) {
  VkCommandBuffer commandBuffer {};
  Renderer::beginCommandBuffer(commandBuffer, commandPool, CommandBufferLevel::PRIMARY, CommandBufferUsage::ONE_TIME_SUBMIT);

  VkImageMemoryBarrier barrier {};
  barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask       = {};
  barrier.dstAccessMask       = {};
  barrier.oldLayout           = static_cast<VkImageLayout>(oldLayout);
  barrier.newLayout           = static_cast<VkImageLayout>(newLayout);
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image               = image;

  barrier.subresourceRange.aspectMask     = static_cast<VkImageAspectFlags>(imgAspect);
  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;

  VkPipelineStageFlags srcStage {};
  VkPipelineStageFlags dstStage {};

  if (oldLayout == ImageLayout::UNDEFINED && newLayout == ImageLayout::TRANSFER_DST) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = static_cast<VkAccessFlags>(MemoryAccess::TRANSFER_WRITE);

    srcStage = static_cast<VkPipelineStageFlags>(PipelineStage::TOP_OF_PIPE);
    dstStage = static_cast<VkPipelineStageFlags>(PipelineStage::TRANSFER);
  } else if (oldLayout == ImageLayout::TRANSFER_DST && newLayout == ImageLayout::SHADER_READ_ONLY) {
    barrier.srcAccessMask = static_cast<VkAccessFlags>(MemoryAccess::TRANSFER_WRITE);
    barrier.dstAccessMask = static_cast<VkAccessFlags>(MemoryAccess::SHADER_READ);

    srcStage = static_cast<VkPipelineStageFlags>(PipelineStage::TRANSFER);
    dstStage = static_cast<VkPipelineStageFlags>(PipelineStage::FRAGMENT_SHADER);
  } else {
    throw std::invalid_argument("Error: Unsupported layout transition.");
  }

  vkCmdPipelineBarrier(commandBuffer,
                       srcStage,
                       dstStage,
                       0,
                       0,
                       nullptr,
                       0,
                       nullptr,
                       1,
                       &barrier);

  Renderer::endCommandBuffer(commandBuffer, graphicsQueue, commandPool);
}

inline void createTexture(VkImage& textureImage,
                          VkDeviceMemory& textureMemory,
                          const FilePath& texturePath,
                          VkCommandPool commandPool,
                          VkQueue graphicsQueue,
                          VkDevice logicalDevice) {
  const Image img(texturePath);

  uint8_t channelCount {};
  switch (img.getColorspace()) {
    case ImageColorspace::GRAY:
    case ImageColorspace::DEPTH:
      channelCount = 1;
      break;

    case ImageColorspace::GRAY_ALPHA:
      channelCount = 2;
      break;

    case ImageColorspace::RGB:
    default:
      channelCount = 3;
      break;

    case ImageColorspace::RGBA:
      channelCount = 4;
      break;
  }

  const std::size_t imgSize = img.getWidth() * img.getHeight() * channelCount;

  VkBuffer stagingBuffer {};
  VkDeviceMemory stagingBufferMemory {};

  Renderer::createBuffer(stagingBuffer,
                         stagingBufferMemory,
                         BufferUsage::TRANSFER_SRC,
                         MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT,
                         imgSize);

  void* data {};
  vkMapMemory(logicalDevice, stagingBufferMemory, 0, imgSize, 0, &data);
  std::memcpy(data, img.getDataPtr(), imgSize);
  vkUnmapMemory(logicalDevice, stagingBufferMemory);

  Renderer::createImage(textureImage,
                        textureMemory,
                        ImageType::IMAGE_2D,
                        img.getWidth(),
                        img.getHeight(),
                        1,
                        1,
                        1,
                        SampleCount::ONE,
                        ImageTiling::OPTIMAL,
                        ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED,
                        SharingMode::EXCLUSIVE,
                        ImageLayout::UNDEFINED);

  transitionImageLayout(textureImage,
                        ImageLayout::UNDEFINED,
                        ImageLayout::TRANSFER_DST,
                        ImageAspect::COLOR,
                        VK_FORMAT_R8G8B8A8_SRGB,
                        commandPool,
                        graphicsQueue);

  Renderer::copyBuffer(stagingBuffer,
                       textureImage,
                       ImageAspect::COLOR,
                       img.getWidth(),
                       img.getHeight(),
                       1,
                       ImageLayout::TRANSFER_DST,
                       commandPool,
                       graphicsQueue);

  transitionImageLayout(textureImage,
                        ImageLayout::TRANSFER_DST,
                        ImageLayout::SHADER_READ_ONLY,
                        ImageAspect::COLOR,
                        VK_FORMAT_R8G8B8A8_SRGB,
                        commandPool,
                        graphicsQueue);

  Renderer::destroyBuffer(stagingBuffer, stagingBufferMemory);
}

/////////////
// Buffers //
/////////////

inline uint32_t findMemoryType(MemoryProperty properties, uint32_t typeFilter, VkPhysicalDevice physicalDevice) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1u << i)) && ((memProperties.memoryTypes[i].propertyFlags & static_cast<uint32_t>(properties)) == static_cast<uint32_t>(properties)))
      return i;
  }

  throw std::runtime_error("Error: Failed to find a suitable memory type.");
}

/////////////////////
// Uniform buffers //
/////////////////////

inline void createUniformBuffers(std::vector<VkBuffer>& uniformBuffers,
                                 std::vector<VkDeviceMemory>& uniformBuffersMemory,
                                 std::size_t bufferCount) {
  constexpr std::size_t bufferSize = sizeof(UniformMatrices);

  uniformBuffers.resize(bufferCount);
  uniformBuffersMemory.resize(bufferCount);

  for (std::size_t i = 0; i < bufferCount; ++i) {
    Renderer::createBuffer(uniformBuffers[i],
                           uniformBuffersMemory[i],
                           BufferUsage::UNIFORM_BUFFER,
                           MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT,
                           bufferSize);
  }
}

inline void createDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets,
                                 VkDescriptorSetLayout descriptorSetLayout,
                                 VkDescriptorPool descriptorPool,
                                 std::size_t descriptorCount,
                                 const std::vector<VkBuffer>& uniformBuffers,
                                 VkSampler textureSampler,
                                 VkImageView textureImageView,
                                 VkDevice logicalDevice) {
  const std::vector<VkDescriptorSetLayout> layouts(descriptorCount, descriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool     = descriptorPool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorCount);
  allocInfo.pSetLayouts        = layouts.data();

  descriptorSets.resize(descriptorCount);

  if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to allocate descriptor sets.");

  for (std::size_t i = 0; i < descriptorCount; ++i) {
    VkDescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = uniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(UniformMatrices);

    VkDescriptorImageInfo imageInfo {};
    imageInfo.sampler     = textureSampler;
    imageInfo.imageView   = textureImageView;
    imageInfo.imageLayout = static_cast<VkImageLayout>(ImageLayout::SHADER_READ_ONLY);

    std::array<VkWriteDescriptorSet, 2> descriptorWrites {};

    descriptorWrites[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet           = descriptorSets[i];
    descriptorWrites[0].dstBinding       = 0;
    descriptorWrites[0].dstArrayElement  = 0;
    descriptorWrites[0].descriptorCount  = 1;
    descriptorWrites[0].descriptorType   = static_cast<VkDescriptorType>(DescriptorType::UNIFORM_BUFFER);
    descriptorWrites[0].pImageInfo       = nullptr;
    descriptorWrites[0].pBufferInfo      = &bufferInfo;
    descriptorWrites[0].pTexelBufferView = nullptr;

    descriptorWrites[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet           = descriptorSets[i];
    descriptorWrites[1].dstBinding       = 1;
    descriptorWrites[1].dstArrayElement  = 0;
    descriptorWrites[1].descriptorCount  = 1;
    descriptorWrites[1].descriptorType   = static_cast<VkDescriptorType>(DescriptorType::COMBINED_IMAGE_SAMPLER);
    descriptorWrites[1].pImageInfo       = &imageInfo;
    descriptorWrites[1].pBufferInfo      = nullptr;
    descriptorWrites[1].pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }
}

inline void updateUniformBuffer(VkExtent2D swapchainExtent,
                                uint32_t currentImageIndex,
                                const std::vector<VkDeviceMemory>& uniformBuffersMemory,
                                VkDevice logicalDevice) {
  static Transform transform {};
  static auto startTime = std::chrono::high_resolution_clock::now();

  const auto currentTime = std::chrono::high_resolution_clock::now();
  const float totalTime  = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

  Camera camera(swapchainExtent.width, swapchainExtent.height, Degreesf(45.f), 0.1f, 100.f, ProjectionType::PERSPECTIVE);
  transform.setRotation(Quaternionf(Degreesf(90.f) * totalTime, Axis::Z));

  UniformMatrices matrices {};
  matrices.model      = transform.computeTransformMatrix();
  matrices.view       = camera.computeLookAt(Vec3f(0.f, 2.f, 2.f));
  matrices.projection = camera.computePerspectiveMatrix();

  void* data {};
  vkMapMemory(logicalDevice, uniformBuffersMemory[currentImageIndex], 0, sizeof(UniformMatrices), 0, &data);
  std::memcpy(data, &matrices, sizeof(UniformMatrices));
  vkUnmapMemory(logicalDevice, uniformBuffersMemory[currentImageIndex]);
}

/////////////////////
// Command buffers //
/////////////////////

inline void createCommandBuffers(std::vector<VkCommandBuffer>& commandBuffers,
                                 const std::vector<VkFramebuffer>& swapchainFramebuffers,
                                 VkCommandPool commandPool,
                                 VkRenderPass renderPass,
                                 VkExtent2D swapchainExtent,
                                 VkPipeline graphicsPipeline,
                                 VkBuffer vertexBuffer,
                                 VkBuffer indexBuffer,
                                 VkPipelineLayout pipelineLayout,
                                 const std::vector<VkDescriptorSet>& descriptorSets,
                                 VkDevice logicalDevice) {
  commandBuffers.resize(swapchainFramebuffers.size());

  VkCommandBufferAllocateInfo allocInfo {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = commandPool;
  allocInfo.level              = static_cast<VkCommandBufferLevel>(CommandBufferLevel::PRIMARY);
  allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

  if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to allocate command buffers.");

  for (std::size_t i = 0; i < commandBuffers.size(); ++i) {
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
      throw std::runtime_error("Error: Failed to begin recording a command buffer.");

    VkRenderPassBeginInfo renderPassInfo {};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass;
    renderPassInfo.framebuffer       = swapchainFramebuffers[i];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchainExtent;

    const VkClearValue clearColor  = { 0.15f, 0.15f, 0.15f, 1.f };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues    = &clearColor;

    vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(commandBuffers[i], static_cast<VkPipelineBindPoint>(PipelineBindPoint::GRAPHICS), graphicsPipeline);

      const std::array<VkBuffer, 1> vertexBuffers = { vertexBuffer };
      const std::array<VkDeviceSize, 1> offsets = { 0 };
      vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers.data(), offsets.data());
      vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdBindDescriptorSets(commandBuffers[i], static_cast<VkPipelineBindPoint>(PipelineBindPoint::GRAPHICS), pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

      vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(commandBuffers[i]);

    if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
      throw std::runtime_error("Error: Failed to record a command buffer.");
  }
}

inline void destroySwapchain(const std::vector<VkFramebuffer>& swapchainFramebuffers,
                             VkCommandPool commandPool,
                             const std::vector<VkCommandBuffer>& commandBuffers,
                             VkDescriptorPool descriptorPool,
                             const std::vector<VkBuffer>& uniformBuffers,
                             const std::vector<VkDeviceMemory>& uniformBuffersMemory,
                             VkPipeline graphicsPipeline,
                             VkPipelineLayout pipelineLayout,
                             VkRenderPass renderPass,
                             const std::vector<VkImageView>& swapchainImageViews,
                             VkSwapchainKHR swapchain,
                             VkDevice logicalDevice) {
  for (VkFramebuffer framebuffer : swapchainFramebuffers)
    Renderer::destroyFramebuffer(framebuffer);

  vkFreeCommandBuffers(logicalDevice, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

  vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

  for (std::size_t i = 0; i < uniformBuffers.size(); ++i)
    Renderer::destroyBuffer(uniformBuffers[i], uniformBuffersMemory[i]);

  vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
  vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
  Renderer::destroyRenderPass(renderPass);

  for (VkImageView imageView : swapchainImageViews)
    Renderer::destroyImageView(imageView);

  vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
}

} // namespace

void Renderer::initialize(GLFWwindow* windowHandle) {
  // To dynamically load Vulkan, see: https://github.com/charles-lunarg/vk-bootstrap/blob/master/src/VkBootstrap.cpp

  // If already initialized, do nothing
  if (s_isInitialized)
    return;

  ///////////////////////
  // Validation layers //
  ///////////////////////

#if !defined(NDEBUG)
  if (!checkValidationLayersSupport())
    throw std::runtime_error("Error: Validation layers are not supported.");
#endif

  //////////////
  // Instance //
  //////////////

  VkApplicationInfo appInfo {};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName   = "RaZ";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
  appInfo.pEngineName        = "RaZ";
  appInfo.engineVersion      = VK_MAKE_VERSION(0, 0, 0);
  appInfo.apiVersion         = VK_API_VERSION_1_0;

  VkInstanceCreateInfo instanceCreateInfo {};
  instanceCreateInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &appInfo;

  const std::vector<const char*> requiredExtensions = getRequiredExtensions();
  instanceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
  instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();

#if !defined(NDEBUG)
  instanceCreateInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
  instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();

  const VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = createDebugMessengerCreateInfo();
  instanceCreateInfo.pNext = &debugCreateInfo;
#endif

  if (vkCreateInstance(&instanceCreateInfo, nullptr, &s_instance) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a Vulkan instance.");

  ////////////////////
  // Debug Callback //
  ////////////////////

#if !defined(NDEBUG)
  setupDebugMessenger(s_instance, m_debugMessenger);
#endif

  ////////////////////
  // Window surface //
  ////////////////////

  if (windowHandle != nullptr) {
    s_windowHandle = windowHandle;

    if (glfwCreateWindowSurface(s_instance, s_windowHandle, nullptr, &s_surface) != VK_SUCCESS)
      throw std::runtime_error("Error: Failed to create a window surface.");
  }

  ////////////////
  // Extensions //
  ////////////////

  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> extensions(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

  std::cout << "Available Vulkan extensions:\n";

  for (const VkExtensionProperties& extension : extensions)
    std::cout << "\t" << extension.extensionName << '\n';

  std::cout << std::flush;

  /////////////////////
  // Physical device //
  /////////////////////

  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(s_instance, &deviceCount, nullptr);

  if (deviceCount == 0)
    throw std::runtime_error("Error: No GPU available with Vulkan support.");

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(s_instance, &deviceCount, devices.data());

  for (const VkPhysicalDevice& device : devices) {
    if (isDeviceSuitable(device, s_surface)) {
      s_physicalDevice = device;
      break;
    }
  }

  if (s_physicalDevice == nullptr)
    throw std::runtime_error("Error: No suitable GPU available.");

  ////////////////////
  // Logical device //
  ////////////////////

  const QueueFamilyIndices queueIndices = findQueueFamilies(s_physicalDevice, s_surface);

  // Recovering the necessary queues
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  const std::set<uint32_t> uniqueQueueFamilies = { queueIndices.graphicsFamily.value(), queueIndices.presentFamily.value() };
  const float queuePriority = 1.f;

  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo {};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.emplace_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures {};
  deviceFeatures.samplerAnisotropy = true;

  VkDeviceCreateInfo deviceCreateInfo {};
  deviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
  deviceCreateInfo.pQueueCreateInfos       = queueCreateInfos.data();
  deviceCreateInfo.pEnabledFeatures        = &deviceFeatures;
  deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
#if !defined(NDEBUG)
  deviceCreateInfo.enabledLayerCount       = static_cast<uint32_t>(validationLayers.size());
  deviceCreateInfo.ppEnabledLayerNames     = validationLayers.data();
#endif

  if (vkCreateDevice(s_physicalDevice, &deviceCreateInfo, nullptr, &s_logicalDevice) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a logical device.");

  // Recovering our queues
  vkGetDeviceQueue(s_logicalDevice, queueIndices.graphicsFamily.value(), 0, &s_graphicsQueue);
  vkGetDeviceQueue(s_logicalDevice, queueIndices.presentFamily.value(), 0, &s_presentQueue);

  ///////////////
  // Swapchain //
  ///////////////

  createSwapchain(s_surface, s_windowHandle, m_swapchain, m_swapchainImages, m_swapchainImageFormat, m_swapchainExtent, s_physicalDevice, s_logicalDevice);

  /////////////////
  // Image views //
  /////////////////

  createImageViews(m_swapchainImageViews, m_swapchainImages, m_swapchainImageFormat);

  /////////////////
  // Render pass //
  /////////////////

  Renderer::createRenderPass(m_renderPass,
                             m_swapchainImageFormat,
                             SampleCount::ONE,
                             AttachmentLoadOp::CLEAR,
                             AttachmentStoreOp::STORE,
                             AttachmentLoadOp::DONT_CARE,
                             AttachmentStoreOp::DONT_CARE,
                             ImageLayout::UNDEFINED,
                             ImageLayout::PRESENT_SRC,
                             ImageLayout::COLOR_ATTACHMENT,
                             PipelineBindPoint::GRAPHICS,
                             VK_SUBPASS_EXTERNAL,
                             0,
                             PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                             PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                             {},
                             MemoryAccess::COLOR_ATTACHMENT_READ | MemoryAccess::COLOR_ATTACHMENT_WRITE);

  ////////////////////
  // Descriptor set //
  ////////////////////

  VkDescriptorSetLayoutBinding uniformLayoutBinding {};
  uniformLayoutBinding.binding            = 0;
  uniformLayoutBinding.descriptorType     = static_cast<VkDescriptorType>(DescriptorType::UNIFORM_BUFFER);
  uniformLayoutBinding.descriptorCount    = 1;
  uniformLayoutBinding.stageFlags         = static_cast<VkShaderStageFlags>(ShaderStage::VERTEX);
  uniformLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding samplerLayoutBinding {};
  samplerLayoutBinding.binding            = 1;
  samplerLayoutBinding.descriptorType     = static_cast<VkDescriptorType>(DescriptorType::COMBINED_IMAGE_SAMPLER);
  samplerLayoutBinding.descriptorCount    = 1;
  samplerLayoutBinding.stageFlags         = static_cast<VkShaderStageFlags>(ShaderStage::FRAGMENT);
  samplerLayoutBinding.pImmutableSamplers = nullptr;

  Renderer::createDescriptorSetLayout(m_descriptorSetLayout, { uniformLayoutBinding, samplerLayoutBinding });

  //////////////
  // Pipeline //
  //////////////

  createGraphicsPipeline(m_graphicsPipeline,
                         m_pipelineLayout,
                         RAZ_ROOT + "shaders/triangle_vk_vert.spv"s,
                         RAZ_ROOT + "shaders/triangle_vk_frag.spv"s,
                         m_swapchainExtent,
                         m_descriptorSetLayout,
                         m_renderPass,
                         s_logicalDevice);

  //////////////////
  // Framebuffers //
  //////////////////

  createFramebuffers(m_swapchainFramebuffers, m_swapchainImageViews, m_renderPass, m_swapchainExtent, s_logicalDevice);

  //////////////////
  // Command pool //
  //////////////////

  Renderer::createCommandPool(m_commandPool, CommandPoolOption::TRANSIENT, queueIndices.graphicsFamily.value());

  /////////////
  // Texture //
  /////////////

  createTexture(m_textureImage, m_textureMemory, RAZ_ROOT + "assets/textures/default.png"s, m_commandPool, s_graphicsQueue, s_logicalDevice);

  Renderer::createImageView(m_textureImageView,
                            m_textureImage,
                            ImageViewType::IMAGE_2D,
                            VK_FORMAT_R8G8B8A8_SRGB,
                            ComponentSwizzle::IDENTITY,
                            ComponentSwizzle::IDENTITY,
                            ComponentSwizzle::IDENTITY,
                            ComponentSwizzle::IDENTITY,
                            ImageAspect::COLOR,
                            0,
                            1,
                            0,
                            1);

  Renderer::createSampler(m_textureSampler,
                          TextureFilter::LINEAR,
                          TextureFilter::LINEAR,
                          SamplerMipmapMode::LINEAR,
                          SamplerAddressMode::REPEAT,
                          SamplerAddressMode::REPEAT,
                          SamplerAddressMode::REPEAT,
                          0.f,
                          true,
                          16.f,
                          false,
                          ComparisonOperation::ALWAYS,
                          0.f,
                          0.f,
                          BorderColor::INT_OPAQUE_BLACK,
                          false);

  ///////////////////
  // Vertex buffer //
  ///////////////////

  Renderer::createStagedBuffer(m_vertexBuffer,
                               m_vertexBufferMemory,
                               BufferUsage::VERTEX_BUFFER,
                               vertices.data(),
                               sizeof(vertices.front()) * vertices.size(),
                               s_graphicsQueue,
                               m_commandPool);

  //////////////////
  // Index buffer //
  //////////////////

  Renderer::createStagedBuffer(m_indexBuffer,
                               m_indexBufferMemory,
                               BufferUsage::INDEX_BUFFER,
                               indices.data(),
                               sizeof(indices.front()) * indices.size(),
                               s_graphicsQueue,
                               m_commandPool);

  /////////////////////
  // Uniform buffers //
  /////////////////////

  createUniformBuffers(m_uniformBuffers, m_uniformBuffersMemory, m_swapchainImages.size());

  {
    VkDescriptorPoolSize uniformPoolSize {};
    uniformPoolSize.type            = static_cast<VkDescriptorType>(DescriptorType::UNIFORM_BUFFER);
    uniformPoolSize.descriptorCount = static_cast<uint32_t>(m_swapchainImages.size());

    VkDescriptorPoolSize samplerPoolSize {};
    samplerPoolSize.type            = static_cast<VkDescriptorType>(DescriptorType::COMBINED_IMAGE_SAMPLER);
    samplerPoolSize.descriptorCount = static_cast<uint32_t>(m_swapchainImages.size());

    Renderer::createDescriptorPool(m_descriptorPool, static_cast<uint32_t>(m_swapchainImages.size()), { uniformPoolSize, samplerPoolSize });
  }

  createDescriptorSets(m_descriptorSets,
                       m_descriptorSetLayout,
                       m_descriptorPool,
                       m_swapchainImages.size(),
                       m_uniformBuffers,
                       m_textureSampler,
                       m_textureImageView,
                       s_logicalDevice);

  /////////////////////
  // Command buffers //
  /////////////////////

  createCommandBuffers(m_commandBuffers,
                       m_swapchainFramebuffers,
                       m_commandPool,
                       m_renderPass,
                       m_swapchainExtent,
                       m_graphicsPipeline,
                       m_vertexBuffer,
                       m_indexBuffer,
                       m_pipelineLayout,
                       m_descriptorSets,
                       s_logicalDevice);

  ////////////////
  // Semaphores //
  ////////////////

  m_imagesInFlight.resize(m_swapchainImages.size(), nullptr);

  VkSemaphoreCreateInfo semaphoreInfo {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (std::size_t i = 0; i < MaxFramesInFlight; ++i) {
    if (vkCreateSemaphore(s_logicalDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS
      || vkCreateSemaphore(s_logicalDevice, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS
      || vkCreateFence(s_logicalDevice, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
      throw std::runtime_error("Error: Failed to create a synchronization objects.");
  }

  s_isInitialized = true;
}

void Renderer::createRenderPass(VkRenderPass& renderPass,
                                VkFormat swapchainImageFormat,
                                SampleCount sampleCount,
                                AttachmentLoadOp colorDepthLoadOp,
                                AttachmentStoreOp colorDepthStoreOp,
                                AttachmentLoadOp stencilLoadOp,
                                AttachmentStoreOp stencilStoreOp,
                                ImageLayout initialLayout,
                                ImageLayout finalLayout,
                                ImageLayout referenceLayout,
                                PipelineBindPoint bindPoint,
                                uint32_t srcSubpass,
                                uint32_t dstSubpass,
                                PipelineStage srcStage,
                                PipelineStage dstStage,
                                MemoryAccess srcAccess,
                                MemoryAccess dstAccess) {
  VkAttachmentDescription attachment {};
  attachment.format         = swapchainImageFormat;
  attachment.samples        = static_cast<VkSampleCountFlagBits>(sampleCount);
  attachment.loadOp         = static_cast<VkAttachmentLoadOp>(colorDepthLoadOp);
  attachment.storeOp        = static_cast<VkAttachmentStoreOp>(colorDepthStoreOp);
  attachment.stencilLoadOp  = static_cast<VkAttachmentLoadOp>(stencilLoadOp);
  attachment.stencilStoreOp = static_cast<VkAttachmentStoreOp>(stencilStoreOp);
  attachment.initialLayout  = static_cast<VkImageLayout>(initialLayout);
  attachment.finalLayout    = static_cast<VkImageLayout>(finalLayout);

  VkAttachmentReference attachmentRef {};
  attachmentRef.attachment = 0;
  attachmentRef.layout     = static_cast<VkImageLayout>(referenceLayout);

  VkSubpassDescription subpass {};
  subpass.pipelineBindPoint    = static_cast<VkPipelineBindPoint>(bindPoint);
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments    = &attachmentRef;

  VkSubpassDependency dependency {};
  dependency.srcSubpass    = srcSubpass;
  dependency.dstSubpass    = dstSubpass;
  dependency.srcStageMask  = static_cast<VkPipelineStageFlags>(srcStage);
  dependency.dstStageMask  = static_cast<VkPipelineStageFlags>(dstStage);
  dependency.srcAccessMask = static_cast<VkAccessFlags>(srcAccess);
  dependency.dstAccessMask = static_cast<VkAccessFlags>(dstAccess);

  VkRenderPassCreateInfo renderPassInfo {};
  renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments    = &attachment;
  renderPassInfo.subpassCount    = 1;
  renderPassInfo.pSubpasses      = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies   = &dependency;

  if (vkCreateRenderPass(s_logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a render pass.");
}

void Renderer::createDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout,
                                         std::initializer_list<VkDescriptorSetLayoutBinding> layoutBindings) {
  VkDescriptorSetLayoutCreateInfo layoutInfo {};
  layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
  layoutInfo.pBindings    = layoutBindings.begin();

  if (vkCreateDescriptorSetLayout(s_logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a descriptor set layout.");
}

void Renderer::createShaderModule(VkShaderModule& shaderModule, std::size_t shaderCodeSize, const char* shaderCodeStr) {
  VkShaderModuleCreateInfo createInfo {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = shaderCodeSize;
  createInfo.pCode    = reinterpret_cast<const uint32_t*>(shaderCodeStr);

  if (vkCreateShaderModule(s_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a shader module.");
}

void Renderer::destroyShaderModule(VkShaderModule shaderModule) {
  vkDestroyShaderModule(s_logicalDevice, shaderModule, nullptr);
}

void Renderer::createPipelineLayout(VkPipelineLayout& pipelineLayout,
                                    uint32_t descriptorSetLayoutCount,
                                    const VkDescriptorSetLayout* descriptorSetLayouts,
                                    uint32_t pushConstantRangeCount,
                                    const VkPushConstantRange* pushConstantRanges) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
  pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount         = descriptorSetLayoutCount;
  pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts;
  pipelineLayoutInfo.pushConstantRangeCount = pushConstantRangeCount;
  pipelineLayoutInfo.pPushConstantRanges    = pushConstantRanges;

  if (vkCreatePipelineLayout(s_logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a pipeline layout.");
}

void Renderer::createPipelineLayout(VkPipelineLayout& pipelineLayout,
                                    std::initializer_list<VkDescriptorSetLayout> descriptorSetLayouts,
                                    std::initializer_list<VkPushConstantRange> pushConstantRanges) {
  createPipelineLayout(pipelineLayout,
                       static_cast<uint32_t>(descriptorSetLayouts.size()),
                       descriptorSetLayouts.begin(),
                       static_cast<uint32_t>(pushConstantRanges.size()),
                       pushConstantRanges.begin());
}

void Renderer::createCommandPool(VkCommandPool& commandPool, CommandPoolOption options, uint32_t queueFamilyIndex) {
  VkCommandPoolCreateInfo commandPoolInfo {};
  commandPoolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.flags            = static_cast<VkCommandPoolCreateFlags>(options);
  commandPoolInfo.queueFamilyIndex = queueFamilyIndex;

  if (vkCreateCommandPool(s_logicalDevice, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a command pool.");
}

void Renderer::createImage(VkImage& image,
                           VkDeviceMemory& imageMemory,
                           ImageType imgType,
                           uint32_t imgWidth,
                           uint32_t imgHeight,
                           uint32_t imgDepth,
                           uint32_t mipLevelCount,
                           uint32_t arrayLayerCount,
                           SampleCount sampleCount,
                           ImageTiling imgTiling,
                           ImageUsage imgUsage,
                           SharingMode sharingMode,
                           ImageLayout initialLayout) {
  assert("Error: A 1D image must have both an height and a depth of 1." && (imgType != ImageType::IMAGE_1D || (imgHeight == 1 && imgDepth == 1)));
  assert("Error: A 2D image must have a depth of 1." && (imgType != ImageType::IMAGE_2D || imgDepth == 1));

  VkImageCreateInfo imageInfo {};
  imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.flags         = 0;
  imageInfo.imageType     = static_cast<VkImageType>(imgType);
  imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
  imageInfo.extent.width  = imgWidth;
  imageInfo.extent.height = imgHeight;
  imageInfo.extent.depth  = imgDepth;
  imageInfo.mipLevels     = mipLevelCount;
  imageInfo.arrayLayers   = arrayLayerCount;
  imageInfo.samples       = static_cast<VkSampleCountFlagBits>(sampleCount);
  imageInfo.tiling        = static_cast<VkImageTiling>(imgTiling);
  imageInfo.usage         = static_cast<VkImageUsageFlags>(imgUsage);
  imageInfo.sharingMode   = static_cast<VkSharingMode>(sharingMode);
  imageInfo.initialLayout = static_cast<VkImageLayout>(initialLayout);

  if (vkCreateImage(s_logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create an image.");

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(s_logicalDevice, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo {};
  allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize  = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(MemoryProperty::DEVICE_LOCAL, memRequirements.memoryTypeBits, s_physicalDevice);

  if (vkAllocateMemory(s_logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to allocate image memory.");

  vkBindImageMemory(s_logicalDevice, image, imageMemory, 0);
}

void Renderer::destroyImage(VkImage image, VkDeviceMemory imageMemory) {
  vkDestroyImage(s_logicalDevice, image, nullptr);
  vkFreeMemory(s_logicalDevice, imageMemory, nullptr);
}

void Renderer::createImageView(VkImageView& imageView,
                               VkImage image,
                               ImageViewType imageViewType,
                               VkFormat imageFormat,
                               ComponentSwizzle redComp,
                               ComponentSwizzle greenComp,
                               ComponentSwizzle blueComp,
                               ComponentSwizzle alphaComp,
                               ImageAspect imageAspect,
                               uint32_t firstMipLevel,
                               uint32_t mipLevelCount,
                               uint32_t firstArrayLayer,
                               uint32_t arrayLayerCount) {
  VkImageViewCreateInfo imageViewCreateInfo {};
  imageViewCreateInfo.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.image        = image;
  imageViewCreateInfo.viewType     = static_cast<VkImageViewType>(imageViewType);
  imageViewCreateInfo.format       = imageFormat;
  imageViewCreateInfo.components.r = static_cast<VkComponentSwizzle>(redComp);
  imageViewCreateInfo.components.g = static_cast<VkComponentSwizzle>(greenComp);
  imageViewCreateInfo.components.b = static_cast<VkComponentSwizzle>(blueComp);
  imageViewCreateInfo.components.a = static_cast<VkComponentSwizzle>(alphaComp);

  imageViewCreateInfo.subresourceRange.aspectMask     = static_cast<VkImageAspectFlags>(imageAspect);
  imageViewCreateInfo.subresourceRange.baseMipLevel   = firstMipLevel;
  imageViewCreateInfo.subresourceRange.levelCount     = mipLevelCount;
  imageViewCreateInfo.subresourceRange.baseArrayLayer = firstArrayLayer;
  imageViewCreateInfo.subresourceRange.layerCount     = arrayLayerCount;

  if (vkCreateImageView(s_logicalDevice, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create an image view.");
}

void Renderer::destroyImageView(VkImageView imageView) {
  vkDestroyImageView(s_logicalDevice, imageView, nullptr);
}

void Renderer::createSampler(VkSampler& sampler,
                             TextureFilter magnifyFilter,
                             TextureFilter minifyFilter,
                             SamplerMipmapMode mipmapMode,
                             SamplerAddressMode addressModeU,
                             SamplerAddressMode addressModeV,
                             SamplerAddressMode addressModeW,
                             float mipmapLodBias,
                             bool enableAnisotropy,
                             float maxAnisotropy,
                             bool enableComparison,
                             ComparisonOperation comparisonOp,
                             float mipmapMinLod,
                             float mipmapMaxLod,
                             BorderColor borderColor,
                             bool unnormalizedCoordinates) {
  VkSamplerCreateInfo samplerInfo {};
  samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter               = static_cast<VkFilter>(magnifyFilter);
  samplerInfo.minFilter               = static_cast<VkFilter>(minifyFilter);
  samplerInfo.mipmapMode              = static_cast<VkSamplerMipmapMode>(mipmapMode);
  samplerInfo.addressModeU            = static_cast<VkSamplerAddressMode>(addressModeU);
  samplerInfo.addressModeV            = static_cast<VkSamplerAddressMode>(addressModeV);
  samplerInfo.addressModeW            = static_cast<VkSamplerAddressMode>(addressModeW);
  samplerInfo.mipLodBias              = mipmapLodBias;
  samplerInfo.anisotropyEnable        = enableAnisotropy;
  samplerInfo.maxAnisotropy           = maxAnisotropy;
  samplerInfo.compareEnable           = enableComparison;
  samplerInfo.compareOp               = static_cast<VkCompareOp>(comparisonOp);
  samplerInfo.minLod                  = mipmapMinLod;
  samplerInfo.maxLod                  = mipmapMaxLod;
  samplerInfo.borderColor             = static_cast<VkBorderColor>(borderColor);
  samplerInfo.unnormalizedCoordinates = unnormalizedCoordinates;

  if (vkCreateSampler(s_logicalDevice, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a sampler.");
}

void Renderer::destroySampler(VkSampler sampler) {
  vkDestroySampler(s_logicalDevice, sampler, nullptr);
}

void Renderer::createBuffer(VkBuffer& buffer,
                            VkDeviceMemory& bufferMemory,
                            BufferUsage usageFlags,
                            MemoryProperty propertyFlags,
                            std::size_t bufferSize) {
  VkBufferCreateInfo bufferInfo {};
  bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size        = bufferSize;
  bufferInfo.usage       = static_cast<uint32_t>(usageFlags);
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(s_logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a buffer.");

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(s_logicalDevice, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo {};
  allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize  = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(propertyFlags, memRequirements.memoryTypeBits, s_physicalDevice);

  if (vkAllocateMemory(s_logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to allocate a buffer's memory.");

  vkBindBufferMemory(s_logicalDevice, buffer, bufferMemory, 0);
}

void Renderer::createStagedBuffer(VkBuffer& buffer,
                                  VkDeviceMemory& bufferMemory,
                                  BufferUsage bufferType,
                                  const void* bufferData,
                                  std::size_t bufferSize,
                                  VkQueue queue,
                                  VkCommandPool commandPool) {
  VkBuffer stagingBuffer {};
  VkDeviceMemory stagingBufferMemory {};

  Renderer::createBuffer(stagingBuffer,
                         stagingBufferMemory,
                         BufferUsage::TRANSFER_SRC,
                         MemoryProperty::HOST_VISIBLE | MemoryProperty::HOST_COHERENT,
                         bufferSize);

  void* data {};
  vkMapMemory(s_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
  std::memcpy(data, bufferData, bufferSize);
  vkUnmapMemory(s_logicalDevice, stagingBufferMemory);

  Renderer::createBuffer(buffer,
                         bufferMemory,
                         BufferUsage::TRANSFER_DST | bufferType,
                         MemoryProperty::DEVICE_LOCAL,
                         bufferSize);

  Renderer::copyBuffer(stagingBuffer, buffer, bufferSize, commandPool, queue);

  Renderer::destroyBuffer(stagingBuffer, stagingBufferMemory);
}

void Renderer::copyBuffer(VkBuffer srcBuffer,
                          VkBuffer dstBuffer,
                          VkDeviceSize bufferSize,
                          VkCommandPool commandPool,
                          VkQueue queue) {
  VkCommandBuffer commandBuffer {};
  Renderer::beginCommandBuffer(commandBuffer, commandPool, CommandBufferLevel::PRIMARY, CommandBufferUsage::ONE_TIME_SUBMIT);

  VkBufferCopy copyRegion {};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size      = bufferSize;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  Renderer::endCommandBuffer(commandBuffer, queue, commandPool);
}

void Renderer::copyBuffer(VkBuffer srcBuffer,
                          VkImage dstImage,
                          ImageAspect imgAspect,
                          uint32_t imgWidth,
                          uint32_t imgHeight,
                          uint32_t imgDepth,
                          ImageLayout imgLayout,
                          VkCommandPool commandPool,
                          VkQueue queue) {
  VkCommandBuffer commandBuffer {};
  Renderer::beginCommandBuffer(commandBuffer, commandPool, CommandBufferLevel::PRIMARY, CommandBufferUsage::ONE_TIME_SUBMIT);

  VkBufferImageCopy region {};
  region.bufferOffset      = 0;
  region.bufferRowLength   = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask     = static_cast<VkImageAspectFlags>(imgAspect);
  region.imageSubresource.mipLevel       = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount     = 1;

  region.imageOffset.x = 0;
  region.imageOffset.y = 0;
  region.imageOffset.z = 0;

  region.imageExtent.width  = imgWidth;
  region.imageExtent.height = imgHeight;
  region.imageExtent.depth  = imgDepth;

  vkCmdCopyBufferToImage(commandBuffer,
                         srcBuffer,
                         dstImage,
                         static_cast<VkImageLayout>(imgLayout),
                         1,
                         &region);

  Renderer::endCommandBuffer(commandBuffer, queue, commandPool);
}

void Renderer::destroyBuffer(VkBuffer buffer, VkDeviceMemory bufferMemory) {
  vkDestroyBuffer(s_logicalDevice, buffer, nullptr);
  vkFreeMemory(s_logicalDevice, bufferMemory, nullptr);
}

void Renderer::beginCommandBuffer(VkCommandBuffer& commandBuffer,
                                  VkCommandPool commandPool,
                                  CommandBufferLevel commandBufferLevel,
                                  CommandBufferUsage commandBufferUsage) {
  VkCommandBufferAllocateInfo allocInfo {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = commandPool;
  allocInfo.level              = static_cast<VkCommandBufferLevel>(commandBufferLevel);
  allocInfo.commandBufferCount = 1;

  vkAllocateCommandBuffers(s_logicalDevice, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = static_cast<VkCommandBufferUsageFlags>(commandBufferUsage);

  vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

void Renderer::endCommandBuffer(VkCommandBuffer& commandBuffer, VkQueue queue, VkCommandPool commandPool) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo {};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &commandBuffer;

  vkQueueSubmit(queue, 1, &submitInfo, nullptr);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(s_logicalDevice, commandPool, 1, &commandBuffer);
}

void Renderer::createDescriptorPool(VkDescriptorPool& descriptorPool,
                                    uint32_t maxSetCount,
                                    std::initializer_list<VkDescriptorPoolSize> poolSizes) {
  VkDescriptorPoolCreateInfo poolInfo {};
  poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets       = maxSetCount;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes    = poolSizes.begin();

  if (vkCreateDescriptorPool(s_logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to create a descriptor pool.");
}

void Renderer::recreateSwapchain() {
  // If the window is minimized, pausing the application to wait for the window to be on screen again
  int width {};
  int height {};
  glfwGetFramebufferSize(s_windowHandle, &width, &height);

  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(s_windowHandle, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(s_logicalDevice);

  createSwapchain(s_surface, s_windowHandle, m_swapchain, m_swapchainImages, m_swapchainImageFormat, m_swapchainExtent, s_physicalDevice, s_logicalDevice);
  createImageViews(m_swapchainImageViews, m_swapchainImages, m_swapchainImageFormat);
  Renderer::createRenderPass(m_renderPass,
                             m_swapchainImageFormat,
                             SampleCount::ONE,
                             AttachmentLoadOp::CLEAR,
                             AttachmentStoreOp::STORE,
                             AttachmentLoadOp::DONT_CARE,
                             AttachmentStoreOp::DONT_CARE,
                             ImageLayout::UNDEFINED,
                             ImageLayout::PRESENT_SRC,
                             ImageLayout::COLOR_ATTACHMENT,
                             PipelineBindPoint::GRAPHICS,
                             VK_SUBPASS_EXTERNAL,
                             0,
                             PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                             PipelineStage::COLOR_ATTACHMENT_OUTPUT,
                             {},
                             MemoryAccess::COLOR_ATTACHMENT_READ | MemoryAccess::COLOR_ATTACHMENT_WRITE);
  createGraphicsPipeline(m_graphicsPipeline,
                         m_pipelineLayout,
                         RAZ_ROOT + "shaders/triangle_vk_vert.spv"s,
                         RAZ_ROOT + "shaders/triangle_vk_frag.spv"s,
                         m_swapchainExtent,
                         m_descriptorSetLayout,
                         m_renderPass,
                         s_logicalDevice);
  createFramebuffers(m_swapchainFramebuffers, m_swapchainImageViews, m_renderPass, m_swapchainExtent, s_logicalDevice);
  createUniformBuffers(m_uniformBuffers, m_uniformBuffersMemory, m_swapchainImages.size());

  {
    VkDescriptorPoolSize uniformPoolSize {};
    uniformPoolSize.type            = static_cast<VkDescriptorType>(DescriptorType::UNIFORM_BUFFER);
    uniformPoolSize.descriptorCount = static_cast<uint32_t>(m_swapchainImages.size());

    VkDescriptorPoolSize samplerPoolSize {};
    samplerPoolSize.type            = static_cast<VkDescriptorType>(DescriptorType::COMBINED_IMAGE_SAMPLER);
    samplerPoolSize.descriptorCount = static_cast<uint32_t>(m_swapchainImages.size());

    Renderer::createDescriptorPool(m_descriptorPool, static_cast<uint32_t>(m_swapchainImages.size()), { uniformPoolSize, samplerPoolSize });
  }

  createDescriptorSets(m_descriptorSets,
                       m_descriptorSetLayout,
                       m_descriptorPool,
                       m_swapchainImages.size(),
                       m_uniformBuffers,
                       m_textureSampler,
                       m_textureImageView,
                       s_logicalDevice);
  createCommandBuffers(m_commandBuffers,
                       m_swapchainFramebuffers,
                       m_commandPool,
                       m_renderPass,
                       m_swapchainExtent,
                       m_graphicsPipeline,
                       m_vertexBuffer,
                       m_indexBuffer,
                       m_pipelineLayout,
                       m_descriptorSets,
                       s_logicalDevice);
}

void Renderer::drawFrame() {
  vkWaitForFences(s_logicalDevice, 1, &m_inFlightFences[m_currentFrameIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());

  uint32_t imageIndex {};
  const VkResult imageResult = vkAcquireNextImageKHR(s_logicalDevice,
                                                     m_swapchain,
                                                     std::numeric_limits<uint64_t>::max(),
                                                     m_imageAvailableSemaphores[m_currentFrameIndex],
                                                     nullptr,
                                                     &imageIndex);

  if (imageResult == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    return;
  } else if (imageResult != VK_SUCCESS && imageResult != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Error: Failed to acquire a swapchain image.");
  }

  updateUniformBuffer(m_swapchainExtent, imageIndex, m_uniformBuffersMemory, s_logicalDevice);

  if (m_imagesInFlight[imageIndex] != nullptr)
    vkWaitForFences(s_logicalDevice, 1, &m_imagesInFlight[imageIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());

  m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrameIndex];

  const std::array<VkSemaphore, 1> waitSemaphores      = { m_imageAvailableSemaphores[m_currentFrameIndex] };
  const std::array<VkPipelineStageFlags, 1> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  const std::array<VkSemaphore, 1> signalSemaphores    = { m_renderFinishedSemaphores[m_currentFrameIndex] };

  VkSubmitInfo submitInfo {};
  submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount   = static_cast<uint32_t>(waitSemaphores.size());
  submitInfo.pWaitSemaphores      = waitSemaphores.data();
  submitInfo.pWaitDstStageMask    = waitStages.data();
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &m_commandBuffers[imageIndex];
  submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
  submitInfo.pSignalSemaphores    = signalSemaphores.data();

  vkResetFences(s_logicalDevice, 1, &m_inFlightFences[m_currentFrameIndex]);

  if (vkQueueSubmit(s_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrameIndex]) != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to submit a draw command buffer.");

  VkPresentInfoKHR presentInfo {};
  presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
  presentInfo.pWaitSemaphores    = signalSemaphores.data();

  const std::array<VkSwapchainKHR, 1> swapchains = { m_swapchain };
  presentInfo.swapchainCount = static_cast<uint32_t>(swapchains.size());
  presentInfo.pSwapchains    = swapchains.data();
  presentInfo.pImageIndices  = &imageIndex;
  presentInfo.pResults       = nullptr;

  const VkResult presentResult = vkQueuePresentKHR(s_presentQueue, &presentInfo);

  if (presentResult != VK_SUCCESS)
    throw std::runtime_error("Error: Failed to present a swapchain image.");

  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
    m_framebufferResized = false;
    recreateSwapchain();
  }

  m_currentFrameIndex = (m_currentFrameIndex + 1) % MaxFramesInFlight;
}

void Renderer::destroyFramebuffer(VkFramebuffer framebuffer) {
  vkDestroyFramebuffer(s_logicalDevice, framebuffer, nullptr);
}

void Renderer::destroyRenderPass(VkRenderPass renderPass) {
  vkDestroyRenderPass(s_logicalDevice, renderPass, nullptr);
}

void Renderer::destroy() {
  vkDeviceWaitIdle(s_logicalDevice);

#if !defined(NDEBUG)
  destroyDebugUtilsMessengerEXT(s_instance, m_debugMessenger, nullptr);
#endif

  destroySwapchain(m_swapchainFramebuffers,
                   m_commandPool,
                   m_commandBuffers,
                   m_descriptorPool,
                   m_uniformBuffers,
                   m_uniformBuffersMemory,
                   m_graphicsPipeline,
                   m_pipelineLayout,
                   m_renderPass,
                   m_swapchainImageViews,
                   m_swapchain,
                   s_logicalDevice);

  vkDestroyDescriptorSetLayout(s_logicalDevice, m_descriptorSetLayout, nullptr);

  for (std::size_t i = 0; i < MaxFramesInFlight; ++i) {
    vkDestroySemaphore(s_logicalDevice, m_renderFinishedSemaphores[i], nullptr);
    vkDestroySemaphore(s_logicalDevice, m_imageAvailableSemaphores[i], nullptr);
    vkDestroyFence(s_logicalDevice, m_inFlightFences[i], nullptr);
  }

  Renderer::destroyBuffer(m_indexBuffer, m_indexBufferMemory);
  Renderer::destroyBuffer(m_vertexBuffer, m_vertexBufferMemory);

  Renderer::destroySampler(m_textureSampler);
  Renderer::destroyImageView(m_textureImageView);
  Renderer::destroyImage(m_textureImage, m_textureMemory);

  vkDestroyCommandPool(s_logicalDevice, m_commandPool, nullptr);

  vkDestroyDevice(s_logicalDevice, nullptr);
  vkDestroySurfaceKHR(s_instance, s_surface, nullptr);
  vkDestroyInstance(s_instance, nullptr);

  s_windowHandle = nullptr;
  s_isInitialized = false;
}

} // namespace Raz
