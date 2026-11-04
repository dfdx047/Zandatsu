// zdiag: teste "info" — instancia, devices, propriedades, limites, features, extensoes, memoria, filas, formatos.
#include <cstdio>

#include "gpu.h"

namespace zdiag {
namespace {

#define ZD_FEATURES10(X) \
  X(robustBufferAccess) X(fullDrawIndexUint32) X(imageCubeArray) X(independentBlend) X(geometryShader) \
  X(tessellationShader) X(sampleRateShading) X(dualSrcBlend) X(logicOp) X(multiDrawIndirect) \
  X(drawIndirectFirstInstance) X(depthClamp) X(depthBiasClamp) X(fillModeNonSolid) X(depthBounds) X(wideLines) \
  X(largePoints) X(alphaToOne) X(multiViewport) X(samplerAnisotropy) X(textureCompressionETC2) \
  X(textureCompressionASTC_LDR) X(textureCompressionBC) X(occlusionQueryPrecise) X(pipelineStatisticsQuery) \
  X(vertexPipelineStoresAndAtomics) X(fragmentStoresAndAtomics) X(shaderTessellationAndGeometryPointSize) \
  X(shaderImageGatherExtended) X(shaderStorageImageExtendedFormats) X(shaderStorageImageMultisample) \
  X(shaderStorageImageReadWithoutFormat) X(shaderStorageImageWriteWithoutFormat) \
  X(shaderUniformBufferArrayDynamicIndexing) X(shaderSampledImageArrayDynamicIndexing) \
  X(shaderStorageBufferArrayDynamicIndexing) X(shaderStorageImageArrayDynamicIndexing) X(shaderClipDistance) \
  X(shaderCullDistance) X(shaderFloat64) X(shaderInt64) X(shaderInt16) X(shaderResourceResidency) \
  X(shaderResourceMinLod) X(sparseBinding) X(sparseResidencyBuffer) X(sparseResidencyImage2D) \
  X(sparseResidencyImage3D) X(sparseResidency2Samples) X(sparseResidency4Samples) X(sparseResidency8Samples) \
  X(sparseResidency16Samples) X(sparseResidencyAliased) X(variableMultisampleRate) X(inheritedQueries)

#define ZD_FEATURES11(X) \
  X(storageBuffer16BitAccess) X(uniformAndStorageBuffer16BitAccess) X(storagePushConstant16) \
  X(storageInputOutput16) X(multiview) X(multiviewGeometryShader) X(multiviewTessellationShader) \
  X(variablePointersStorageBuffer) X(variablePointers) X(protectedMemory) X(samplerYcbcrConversion) \
  X(shaderDrawParameters)

#define ZD_FEATURES12(X) \
  X(samplerMirrorClampToEdge) X(drawIndirectCount) X(storageBuffer8BitAccess) X(uniformAndStorageBuffer8BitAccess) \
  X(storagePushConstant8) X(shaderBufferInt64Atomics) X(shaderSharedInt64Atomics) X(shaderFloat16) X(shaderInt8) \
  X(descriptorIndexing) X(shaderInputAttachmentArrayDynamicIndexing) \
  X(shaderUniformTexelBufferArrayDynamicIndexing) X(shaderStorageTexelBufferArrayDynamicIndexing) \
  X(shaderUniformBufferArrayNonUniformIndexing) X(shaderSampledImageArrayNonUniformIndexing) \
  X(shaderStorageBufferArrayNonUniformIndexing) X(shaderStorageImageArrayNonUniformIndexing) \
  X(shaderInputAttachmentArrayNonUniformIndexing) X(shaderUniformTexelBufferArrayNonUniformIndexing) \
  X(shaderStorageTexelBufferArrayNonUniformIndexing) X(descriptorBindingUniformBufferUpdateAfterBind) \
  X(descriptorBindingSampledImageUpdateAfterBind) X(descriptorBindingStorageImageUpdateAfterBind) \
  X(descriptorBindingStorageBufferUpdateAfterBind) X(descriptorBindingUniformTexelBufferUpdateAfterBind) \
  X(descriptorBindingStorageTexelBufferUpdateAfterBind) X(descriptorBindingUpdateUnusedWhilePending) \
  X(descriptorBindingPartiallyBound) X(descriptorBindingVariableDescriptorCount) X(runtimeDescriptorArray) \
  X(samplerFilterMinmax) X(scalarBlockLayout) X(imagelessFramebuffer) X(uniformBufferStandardLayout) \
  X(shaderSubgroupExtendedTypes) X(separateDepthStencilLayouts) X(hostQueryReset) X(timelineSemaphore) \
  X(bufferDeviceAddress) X(bufferDeviceAddressCaptureReplay) X(bufferDeviceAddressMultiDevice) \
  X(vulkanMemoryModel) X(vulkanMemoryModelDeviceScope) X(vulkanMemoryModelAvailabilityVisibilityChains) \
  X(shaderOutputViewportIndex) X(shaderOutputLayer) X(subgroupBroadcastDynamicId)

#define ZD_FEATURES13(X) \
  X(robustImageAccess) X(inlineUniformBlock) X(descriptorBindingInlineUniformBlockUpdateAfterBind) \
  X(pipelineCreationCacheControl) X(privateData) X(shaderDemoteToHelperInvocation) X(shaderTerminateInvocation) \
  X(subgroupSizeControl) X(computeFullSubgroups) X(synchronization2) X(textureCompressionASTC_HDR) \
  X(shaderZeroInitializeWorkgroupMemory) X(dynamicRendering) X(shaderIntegerDotProduct) X(maintenance4)

#define ZD_LIMITS(X) \
  X(maxImageDimension1D) X(maxImageDimension2D) X(maxImageDimension3D) X(maxImageDimensionCube) \
  X(maxImageArrayLayers) X(maxTexelBufferElements) X(maxUniformBufferRange) X(maxStorageBufferRange) \
  X(maxPushConstantsSize) X(maxMemoryAllocationCount) X(maxSamplerAllocationCount) X(bufferImageGranularity) \
  X(sparseAddressSpaceSize) X(maxBoundDescriptorSets) X(maxPerStageDescriptorSamplers) \
  X(maxPerStageDescriptorUniformBuffers) X(maxPerStageDescriptorStorageBuffers) \
  X(maxPerStageDescriptorSampledImages) X(maxPerStageDescriptorStorageImages) \
  X(maxPerStageDescriptorInputAttachments) X(maxPerStageResources) X(maxDescriptorSetSamplers) \
  X(maxDescriptorSetUniformBuffers) X(maxDescriptorSetUniformBuffersDynamic) X(maxDescriptorSetStorageBuffers) \
  X(maxDescriptorSetStorageBuffersDynamic) X(maxDescriptorSetSampledImages) X(maxDescriptorSetStorageImages) \
  X(maxDescriptorSetInputAttachments) X(maxVertexInputAttributes) X(maxVertexInputBindings) \
  X(maxVertexInputAttributeOffset) X(maxVertexInputBindingStride) X(maxVertexOutputComponents) \
  X(maxTessellationGenerationLevel) X(maxTessellationPatchSize) \
  X(maxTessellationControlPerVertexInputComponents) X(maxTessellationControlPerVertexOutputComponents) \
  X(maxTessellationControlPerPatchOutputComponents) X(maxTessellationControlTotalOutputComponents) \
  X(maxTessellationEvaluationInputComponents) X(maxTessellationEvaluationOutputComponents) \
  X(maxGeometryShaderInvocations) X(maxGeometryInputComponents) X(maxGeometryOutputComponents) \
  X(maxGeometryOutputVertices) X(maxGeometryTotalOutputComponents) X(maxFragmentInputComponents) \
  X(maxFragmentOutputAttachments) X(maxFragmentDualSrcAttachments) X(maxFragmentCombinedOutputResources) \
  X(maxComputeSharedMemorySize) X(maxComputeWorkGroupCount) X(maxComputeWorkGroupInvocations) \
  X(maxComputeWorkGroupSize) X(subPixelPrecisionBits) X(subTexelPrecisionBits) X(mipmapPrecisionBits) \
  X(maxDrawIndexedIndexValue) X(maxDrawIndirectCount) X(maxSamplerLodBias) X(maxSamplerAnisotropy) \
  X(maxViewports) X(maxViewportDimensions) X(viewportBoundsRange) X(viewportSubPixelBits) \
  X(minMemoryMapAlignment) X(minTexelBufferOffsetAlignment) X(minUniformBufferOffsetAlignment) \
  X(minStorageBufferOffsetAlignment) X(minTexelOffset) X(maxTexelOffset) X(minTexelGatherOffset) \
  X(maxTexelGatherOffset) X(minInterpolationOffset) X(maxInterpolationOffset) \
  X(subPixelInterpolationOffsetBits) X(maxFramebufferWidth) X(maxFramebufferHeight) X(maxFramebufferLayers) \
  X(framebufferColorSampleCounts) X(framebufferDepthSampleCounts) X(framebufferStencilSampleCounts) \
  X(framebufferNoAttachmentsSampleCounts) X(maxColorAttachments) X(sampledImageColorSampleCounts) \
  X(sampledImageIntegerSampleCounts) X(sampledImageDepthSampleCounts) X(sampledImageStencilSampleCounts) \
  X(storageImageSampleCounts) X(maxSampleMaskWords) X(timestampComputeAndGraphics) X(timestampPeriod) \
  X(maxClipDistances) X(maxCullDistances) X(maxCombinedClipAndCullDistances) X(discreteQueuePriorities) \
  X(pointSizeRange) X(lineWidthRange) X(pointSizeGranularity) X(lineWidthGranularity) X(strictLines) \
  X(standardSampleLocations) X(optimalBufferCopyOffsetAlignment) X(optimalBufferCopyRowPitchAlignment) \
  X(nonCoherentAtomSize)

#define ZD_FMT_BC(X) \
  X(BC1_RGB_UNORM_BLOCK) X(BC1_RGB_SRGB_BLOCK) X(BC1_RGBA_UNORM_BLOCK) X(BC1_RGBA_SRGB_BLOCK) \
  X(BC2_UNORM_BLOCK) X(BC2_SRGB_BLOCK) X(BC3_UNORM_BLOCK) X(BC3_SRGB_BLOCK) X(BC4_UNORM_BLOCK) \
  X(BC4_SNORM_BLOCK) X(BC5_UNORM_BLOCK) X(BC5_SNORM_BLOCK) X(BC6H_UFLOAT_BLOCK) X(BC6H_SFLOAT_BLOCK) \
  X(BC7_UNORM_BLOCK) X(BC7_SRGB_BLOCK)
#define ZD_FMT_ETC(X) \
  X(ETC2_R8G8B8_UNORM_BLOCK) X(ETC2_R8G8B8_SRGB_BLOCK) X(ETC2_R8G8B8A1_UNORM_BLOCK) \
  X(ETC2_R8G8B8A1_SRGB_BLOCK) X(ETC2_R8G8B8A8_UNORM_BLOCK) X(ETC2_R8G8B8A8_SRGB_BLOCK) \
  X(EAC_R11_UNORM_BLOCK) X(EAC_R11_SNORM_BLOCK) X(EAC_R11G11_UNORM_BLOCK) X(EAC_R11G11_SNORM_BLOCK)
#define ZD_ASTC_SIZES(X) \
  X(4x4) X(5x4) X(5x5) X(6x5) X(6x6) X(8x5) X(8x6) X(8x8) X(10x5) X(10x6) X(10x8) X(10x10) X(12x10) X(12x12)
#define ZD_FMT_PLAIN(X) \
  X(R8G8B8A8_UNORM) X(R8G8B8A8_SRGB) X(B8G8R8A8_UNORM) X(R16G16B16A16_SFLOAT) X(R32G32B32A32_SFLOAT) \
  X(A2R10G10B10_UNORM_PACK32) X(A2B10G10R10_UNORM_PACK32) X(B10G11R11_UFLOAT_PACK32) X(D24_UNORM_S8_UINT) \
  X(D32_SFLOAT) X(D32_SFLOAT_S8_UINT) X(D16_UNORM) X(R5G6B5_UNORM_PACK16) X(B5G6R5_UNORM_PACK16) \
  X(R4G4B4A4_UNORM_PACK16) X(B4G4R4A4_UNORM_PACK16) X(R8_UNORM) X(R8G8_UNORM) X(R16_UNORM) X(R16_SFLOAT) \
  X(R32_SFLOAT)

struct Fmt {
  VkFormat f;
  const char* name;
};

const std::vector<Fmt>& Formats(bool astc_hdr) {
#define F(n) {VK_FORMAT_##n, #n},
#define A(s) {VK_FORMAT_ASTC_##s##_UNORM_BLOCK, "ASTC_" #s "_UNORM_BLOCK"}, \
             {VK_FORMAT_ASTC_##s##_SRGB_BLOCK, "ASTC_" #s "_SRGB_BLOCK"},
#define H(s) {VK_FORMAT_ASTC_##s##_SFLOAT_BLOCK, "ASTC_" #s "_SFLOAT_BLOCK"},
  static const std::vector<Fmt> base = {ZD_FMT_BC(F) ZD_FMT_ETC(F) ZD_ASTC_SIZES(A) ZD_FMT_PLAIN(F)};
  static const std::vector<Fmt> hdr = [] {
    std::vector<Fmt> v = base;
    std::vector<Fmt> h = {ZD_ASTC_SIZES(H)};
    v.insert(v.end(), h.begin(), h.end());
    return v;
  }();
#undef F
#undef A
#undef H
  return astc_hdr ? hdr : base;
}

std::string FormatFlags(VkFormatFeatureFlags f) {
  static const struct { VkFormatFeatureFlags bit; const char* n; } names[] = {
      {VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "sampled"},
      {VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "linear"},
      {VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, "storage"},
      {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, "color"},
      {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, "blend"},
      {VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "depth"},
      {VK_FORMAT_FEATURE_BLIT_SRC_BIT, "blitsrc"},
      {VK_FORMAT_FEATURE_BLIT_DST_BIT, "blitdst"},
      {VK_FORMAT_FEATURE_TRANSFER_SRC_BIT, "xfersrc"},
      {VK_FORMAT_FEATURE_TRANSFER_DST_BIT, "xferdst"},
      {VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT, "utb"},
      {VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT, "stb"},
      {VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT, "vertex"},
  };
  std::string s;
  for (auto& n : names)
    if (f & n.bit) {
      if (!s.empty()) s += '|';
      s += n.n;
    }
  return s;
}

const char* DeviceTypeName(VkPhysicalDeviceType t) {
  switch (t) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    default: return "other";
  }
}

const char* DriverIdName(uint32_t id) {
  switch (id) {
    case 0: return "";
    case 1: return "AMD_PROPRIETARY";
    case 2: return "AMD_OPEN_SOURCE";
    case 3: return "MESA_RADV";
    case 4: return "NVIDIA_PROPRIETARY";
    case 5: return "INTEL_PROPRIETARY_WINDOWS";
    case 6: return "INTEL_OPEN_SOURCE_MESA";
    case 7: return "IMAGINATION_PROPRIETARY";
    case 8: return "QUALCOMM_PROPRIETARY";
    case 9: return "ARM_PROPRIETARY";
    case 10: return "GOOGLE_SWIFTSHADER";
    case 11: return "GGP_PROPRIETARY";
    case 12: return "BROADCOM_PROPRIETARY";
    case 13: return "MESA_LLVMPIPE";
    case 14: return "MOLTENVK";
    case 15: return "COREAVI_PROPRIETARY";
    case 16: return "JUICE_PROPRIETARY";
    case 17: return "VERISILICON_PROPRIETARY";
    case 18: return "MESA_TURNIP";
    case 19: return "MESA_V3DV";
    case 20: return "MESA_PANVK";
    case 21: return "SAMSUNG_PROPRIETARY";
    case 22: return "MESA_VENUS";
    case 23: return "MESA_DOZEN";
    case 24: return "MESA_NVK";
    case 25: return "IMAGINATION_OPEN_SOURCE_MESA";
    case 26: return "MESA_HONEYKRISP";
    default: return "?";
  }
}

std::string VersionStr(uint32_t v) {
  char b[48];
  snprintf(b, sizeof(b), "%u.%u.%u", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
  return b;
}

// driverVersion com as macros antigas VK_VERSION_* (major de 10 bits, sem 'variant'): Qualcomm usa
// 512.x.y, que com VK_API_VERSION_MAJOR daria 0.x.y.
std::string DriverVersionStr(uint32_t v) {
  char b[48];
  snprintf(b, sizeof(b), "%u.%u.%u", v >> 22, (v >> 12) & 0x3FF, v & 0xFFF);
  return b;
}

// driverVersion: cada fabricante codifica a sua maneira.
void DriverVersionJson(Json& j, const VkPhysicalDeviceProperties& p, uint32_t driver_id) {
  const uint32_t v = p.driverVersion;
  char b[64];
  j.k("driverVersion").obj().kv("raw", v).hex("hex", v);
  if (p.vendorID == 0x10DE) {  // NVIDIA: 10.8.8.6
    snprintf(b, sizeof(b), "%u.%u.%u.%u", (v >> 22) & 0x3FF, (v >> 14) & 0xFF, (v >> 6) & 0xFF, v & 0x3F);
    j.kv("decoded", b).kv("scheme", "nvidia");
  } else if (p.vendorID == 0x13B5 && driver_id != 20) {  // ARM Mali proprietario: rXpY
    snprintf(b, sizeof(b), "r%up%u (%s)", v >> 22, (v >> 12) & 0x3FF, DriverVersionStr(v).c_str());
    j.kv("decoded", b).kv("scheme", "arm-rXpY");
  } else if (p.vendorID == 0x1010 && driver_id == 7) {  // Imagination proprietario: numero de build (changelist)
    snprintf(b, sizeof(b), "build %u (%s)", v, DriverVersionStr(v).c_str());
    j.kv("decoded", b).kv("scheme", "imagination-build");
  } else if (p.vendorID == 0x5143 && driver_id == 8) {  // Qualcomm proprietario: VK_MAKE_VERSION (ex. 512.744.0)
    j.kv("decoded", DriverVersionStr(v)).kv("scheme", "qualcomm");
  } else {  // Mesa (Turnip/PanVK), Samsung Xclipse e outros: VK_MAKE_API_VERSION
    j.kv("decoded", DriverVersionStr(v)).kv("scheme", "vk-standard");
  }
  j.end();
}

std::string Uuid(const uint8_t* u, size_t n) {
  std::string s;
  char b[4];
  for (size_t i = 0; i < n; ++i) {
    snprintf(b, sizeof(b), "%02x", u[i]);
    s += b;
  }
  return s;
}

std::string QueueFlags(VkQueueFlags f) {
  std::string s;
  auto add = [&](VkQueueFlags bit, const char* n) {
    if (f & bit) { if (!s.empty()) s += '|'; s += n; }
  };
  add(VK_QUEUE_GRAPHICS_BIT, "graphics"); add(VK_QUEUE_COMPUTE_BIT, "compute");
  add(VK_QUEUE_TRANSFER_BIT, "transfer"); add(VK_QUEUE_SPARSE_BINDING_BIT, "sparse");
  add(VK_QUEUE_PROTECTED_BIT, "protected");
  return s;
}

std::string MemFlags(VkMemoryPropertyFlags f) {
  std::string s;
  auto add = [&](VkMemoryPropertyFlags bit, const char* n) {
    if (f & bit) { if (!s.empty()) s += '|'; s += n; }
  };
  add(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "device_local"); add(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "host_visible");
  add(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "host_coherent"); add(VK_MEMORY_PROPERTY_HOST_CACHED_BIT, "host_cached");
  add(VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, "lazily_allocated"); add(VK_MEMORY_PROPERTY_PROTECTED_BIT, "protected");
  return s;
}

std::string StageFlags(VkShaderStageFlags f) {
  std::string s;
  auto add = [&](VkShaderStageFlags bit, const char* n) {
    if (f & bit) { if (!s.empty()) s += '|'; s += n; }
  };
  add(VK_SHADER_STAGE_VERTEX_BIT, "vertex"); add(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tesc");
  add(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tese"); add(VK_SHADER_STAGE_GEOMETRY_BIT, "geometry");
  add(VK_SHADER_STAGE_FRAGMENT_BIT, "fragment"); add(VK_SHADER_STAGE_COMPUTE_BIT, "compute");
  return s;
}

std::string SubgroupOps(VkSubgroupFeatureFlags f) {
  std::string s;
  auto add = [&](VkSubgroupFeatureFlags bit, const char* n) {
    if (f & bit) { if (!s.empty()) s += '|'; s += n; }
  };
  add(VK_SUBGROUP_FEATURE_BASIC_BIT, "basic"); add(VK_SUBGROUP_FEATURE_VOTE_BIT, "vote");
  add(VK_SUBGROUP_FEATURE_ARITHMETIC_BIT, "arithmetic"); add(VK_SUBGROUP_FEATURE_BALLOT_BIT, "ballot");
  add(VK_SUBGROUP_FEATURE_SHUFFLE_BIT, "shuffle"); add(VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT, "shuffle_relative");
  add(VK_SUBGROUP_FEATURE_CLUSTERED_BIT, "clustered"); add(VK_SUBGROUP_FEATURE_QUAD_BIT, "quad");
  return s;
}

struct DeviceSummary {
  bool bc = false, astc = false, etc2 = false;
  uint32_t driverID = 0;
  std::string driverName, driverInfo, deviceName, driverVersion;
};

// Uma entrada de "devices": tudo o que da para saber sem criar o VkDevice.
void DescribeDevice(Gpu& g, VkPhysicalDevice pd, Json& j, DeviceSummary& sum) {
  VkPhysicalDeviceProperties p;
  vk.GetPhysicalDeviceProperties(pd, &p);
  const uint32_t api = VK_API_VERSION_MINOR(p.apiVersion) | (VK_API_VERSION_MAJOR(p.apiVersion) << 8);
  const bool v11 = api >= 0x101 && g.instanceVersion >= VK_API_VERSION_1_1, v12 = v11 && api >= 0x102,
             v13 = v11 && api >= 0x103;
  uint32_t n = 0;
  vk.EnumerateDeviceExtensionProperties(pd, nullptr, &n, nullptr);
  std::vector<VkExtensionProperties> exts(n);
  vk.EnumerateDeviceExtensionProperties(pd, nullptr, &n, exts.data());
  auto has = [&](const char* e) {
    for (auto& x : exts)
      if (!strcmp(x.extensionName, e)) return true;
    return false;
  };
  const bool p2 = g.props2;

  // Propriedades encadeadas.
  VkPhysicalDeviceDriverProperties drv{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
  VkPhysicalDeviceIDProperties idp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
  VkPhysicalDeviceSubgroupProperties sg{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
  VkPhysicalDeviceVulkan12Properties p12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
  VkPhysicalDeviceVulkan13Properties p13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};
  const bool has_drv = p2 && (v12 || has(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME));
  if (p2) {
    VkPhysicalDeviceProperties2 pp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    void** next = &pp.pNext;
    if (has_drv) { *next = &drv; next = &drv.pNext; }
    if (v11) { *next = &idp; next = &idp.pNext; *next = &sg; next = &sg.pNext; }
    if (v12) { *next = &p12; next = &p12.pNext; }
    if (v13) { *next = &p13; next = &p13.pNext; }
    vk.GetPhysicalDeviceProperties2(pd, &pp);
  }

  j.obj();
  j.kv("deviceName", p.deviceName).hex("vendorID", p.vendorID).hex("deviceID", p.deviceID)
      .kv("deviceType", DeviceTypeName(p.deviceType)).kv("apiVersion", VersionStr(p.apiVersion));
  DriverVersionJson(j, p, has_drv ? drv.driverID : 0);
  if (has_drv) {
    j.k("driver").obj().kv("driverID", uint32_t(drv.driverID)).kv("driverIDName", DriverIdName(drv.driverID))
        .kv("driverName", drv.driverName).kv("driverInfo", drv.driverInfo);
    char cv[32];
    snprintf(cv, sizeof(cv), "%u.%u.%u.%u", drv.conformanceVersion.major, drv.conformanceVersion.minor,
             drv.conformanceVersion.subminor, drv.conformanceVersion.patch);
    j.kv("conformanceVersion", cv).end();
  }
  j.kv("pipelineCacheUUID", Uuid(p.pipelineCacheUUID, VK_UUID_SIZE));
  if (v11) {
    j.k("id").obj().kv("deviceUUID", Uuid(idp.deviceUUID, VK_UUID_SIZE))
        .kv("driverUUID", Uuid(idp.driverUUID, VK_UUID_SIZE)).kv("deviceLUIDValid", bool(idp.deviceLUIDValid)).end();
    j.k("subgroup").obj().kv("subgroupSize", sg.subgroupSize).kv("supportedStages", StageFlags(sg.supportedStages))
        .kv("supportedOperations", SubgroupOps(sg.supportedOperations))
        .kv("quadOperationsInAllStages", bool(sg.quadOperationsInAllStages));
    if (v13)
      j.kv("minSubgroupSize", p13.minSubgroupSize).kv("maxSubgroupSize", p13.maxSubgroupSize)
          .kv("maxComputeWorkgroupSubgroups", p13.maxComputeWorkgroupSubgroups)
          .kv("requiredSubgroupSizeStages", StageFlags(p13.requiredSubgroupSizeStages));
    j.end();
  }
  if (v12) {
    j.k("floatControls").obj().kv("denormBehaviorIndependence", int(p12.denormBehaviorIndependence))
        .kv("roundingModeIndependence", int(p12.roundingModeIndependence))
        .kv("shaderSignedZeroInfNanPreserveFloat16", bool(p12.shaderSignedZeroInfNanPreserveFloat16))
        .kv("shaderSignedZeroInfNanPreserveFloat32", bool(p12.shaderSignedZeroInfNanPreserveFloat32))
        .kv("shaderDenormPreserveFloat32", bool(p12.shaderDenormPreserveFloat32))
        .kv("shaderDenormFlushToZeroFloat32", bool(p12.shaderDenormFlushToZeroFloat32))
        .kv("shaderRoundingModeRTEFloat32", bool(p12.shaderRoundingModeRTEFloat32))
        .kv("shaderRoundingModeRTZFloat32", bool(p12.shaderRoundingModeRTZFloat32))
        .kv("shaderDenormPreserveFloat16", bool(p12.shaderDenormPreserveFloat16))
        .kv("shaderDenormFlushToZeroFloat16", bool(p12.shaderDenormFlushToZeroFloat16)).end();
  }
  if (v13) j.kv("maxBufferSize", uint64_t(p13.maxBufferSize)).kv("maxInlineUniformBlockSize", p13.maxInlineUniformBlockSize);

  j.k("limits").obj();
#define L(n) j.kv(#n, p.limits.n);
  ZD_LIMITS(L)
#undef L
  j.end();
  j.k("sparseProperties").obj()
      .kv("residencyStandard2DBlockShape", bool(p.sparseProperties.residencyStandard2DBlockShape))
      .kv("residencyNonResidentStrict", bool(p.sparseProperties.residencyNonResidentStrict)).end();

  // Features
  VkPhysicalDeviceFeatures f10;
  vk.GetPhysicalDeviceFeatures(pd, &f10);
  VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceTextureCompressionASTCHDRFeaturesEXT hdr{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES_EXT};
  const bool hdr_ext = has(VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME);
  if (p2 && (v12 || hdr_ext)) {
    VkPhysicalDeviceFeatures2 ff{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    void** next = &ff.pNext;
    if (v12) { *next = &f11; next = &f11.pNext; *next = &f12; next = &f12.pNext; }
    if (v13) { *next = &f13; next = &f13.pNext; }
    if (!v13 && hdr_ext) { *next = &hdr; next = &hdr.pNext; }
    vk.GetPhysicalDeviceFeatures2(pd, &ff);
  }
  const bool astc_hdr = v13 ? f13.textureCompressionASTC_HDR : hdr_ext && hdr.textureCompressionASTC_HDR;
  j.k("features").obj();
  j.k("core10").obj();
#define B(n) j.kv(#n, bool(f10.n));
  ZD_FEATURES10(B)
#undef B
  j.end();
  if (v12) {
    j.k("vulkan11").obj();
#define B(n) j.kv(#n, bool(f11.n));
    ZD_FEATURES11(B)
#undef B
    j.end().k("vulkan12").obj();
#define B(n) j.kv(#n, bool(f12.n));
    ZD_FEATURES12(B)
#undef B
    j.end();
  }
  if (v13) {
    j.k("vulkan13").obj();
#define B(n) j.kv(#n, bool(f13.n));
    ZD_FEATURES13(B)
#undef B
    j.end();
  }
  j.kv("textureCompressionASTC_HDR", astc_hdr);
  j.end();

  j.k("extensions").obj();
  for (auto& e : exts) j.kv(e.extensionName, e.specVersion);
  j.end();

  VkPhysicalDeviceMemoryProperties mp;
  vk.GetPhysicalDeviceMemoryProperties(pd, &mp);
  j.k("memory").obj().k("heaps").arr();
  for (uint32_t i = 0; i < mp.memoryHeapCount; ++i)
    j.obj().kv("sizeMB", uint64_t(mp.memoryHeaps[i].size >> 20))
        .kv("deviceLocal", bool(mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)).end();
  j.end().k("types").arr();
  for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
    j.obj().kv("heap", mp.memoryTypes[i].heapIndex).kv("flags", MemFlags(mp.memoryTypes[i].propertyFlags)).end();
  j.end().end();

  uint32_t nq = 0;
  vk.GetPhysicalDeviceQueueFamilyProperties(pd, &nq, nullptr);
  std::vector<VkQueueFamilyProperties> qf(nq);
  vk.GetPhysicalDeviceQueueFamilyProperties(pd, &nq, qf.data());
  j.k("queueFamilies").arr();
  for (auto& q : qf) {
    j.obj().kv("flags", QueueFlags(q.queueFlags)).kv("count", q.queueCount)
        .kv("timestampValidBits", q.timestampValidBits);
    j.k("minImageTransferGranularity").arr().v(q.minImageTransferGranularity.width)
        .v(q.minImageTransferGranularity.height).v(q.minImageTransferGranularity.depth).end();
    j.end();
  }
  j.end();

  // Formatos: hex por tipo de tiling + lista curta do optimal.
  j.k("formats").obj();
  for (const Fmt& f : Formats(astc_hdr)) {
    VkFormatProperties fp{};
    vk.GetPhysicalDeviceFormatProperties(pd, f.f, &fp);
    j.k(f.name).obj().hex("linear", fp.linearTilingFeatures).hex("optimal", fp.optimalTilingFeatures)
        .hex("buffer", fp.bufferFeatures).kv("optimalDecoded", FormatFlags(fp.optimalTilingFeatures)).end();
  }
  j.end();
  j.end();

  sum.bc = f10.textureCompressionBC;
  sum.astc = f10.textureCompressionASTC_LDR;
  sum.etc2 = f10.textureCompressionETC2;
  sum.deviceName = p.deviceName;
  sum.driverVersion = DriverVersionStr(p.driverVersion);
  if (has_drv) {
    sum.driverID = drv.driverID;
    sum.driverName = drv.driverName;
    sum.driverInfo = drv.driverInfo;
  }
}

}  // namespace

void TestInfo(Test& t) {
  Gpu g(t);
  std::vector<VkPhysicalDevice> pds;
  t.Step("instance", [&](Json& info) {
    g.CreateInstance();
    info.kv("instanceVersion", VersionStr(g.instanceVersion)).kv("properties2", g.props2);
    uint32_t n = 0;
    vk.EnumeratePhysicalDevices(g.inst, &n, nullptr);
    pds.resize(n);
    vk.EnumeratePhysicalDevices(g.inst, &n, pds.data());
    info.kv("physicalDevices", n);
    return St::Pass;
  });
  if (pds.empty()) {
    t.message = "sem instancia Vulkan / sem GPU";
    return;
  }
  // Camadas e extensoes da instancia.
  t.details.kv("instanceVersion", VersionStr(g.instanceVersion));
  std::vector<DeviceSummary> sums(pds.size());
  std::string devices;
  t.Step("devices", [&](Json& info) {
    Json d;
    d.arr();
    for (size_t i = 0; i < pds.size(); ++i) {
      t.Progress("devices/" + std::to_string(i));
      DescribeDevice(g, pds[i], d, sums[i]);
    }
    d.end();
    devices = d.str();
    info.kv("count", pds.size());
    return St::Pass;
  });
  const DeviceSummary& s = sums[0];
  t.details.kv("deviceName", s.deviceName).kv("textureCompressionBC", s.bc)
      .kv("textureCompressionASTC_LDR", s.astc).kv("textureCompressionETC2", s.etc2)
      .kv("driverID", s.driverID).kv("driverIDName", DriverIdName(s.driverID)).kv("driverName", s.driverName)
      .kv("driverInfo", s.driverInfo);
  {
    uint32_t n = 0;
    vk.EnumerateInstanceLayerProperties(&n, nullptr);
    std::vector<VkLayerProperties> layers(n);
    vk.EnumerateInstanceLayerProperties(&n, layers.data());
    t.details.k("layers").arr();
    for (auto& l : layers) t.details.obj().kv("name", l.layerName).kv("spec", VersionStr(l.specVersion)).end();
    t.details.end();
    n = 0;
    vk.EnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> exts(n);
    vk.EnumerateInstanceExtensionProperties(nullptr, &n, exts.data());
    t.details.k("instanceExtensions").obj();
    for (auto& e : exts) t.details.kv(e.extensionName, e.specVersion);
    t.details.end();
  }
  t.details.k("devices").raw(devices.empty() ? "[]" : devices);

  // Confirma que o vkCreateDevice funciona no device escolhido (sem features extra).
  t.Step("create_device", [&](Json& info) {
    g.CreateDevice({});
    info.kv("queueFamily", g.qfam).kv("apiVersionUsed", VersionStr(g.apiVersion));
    return St::Pass;
  });

  // Mensagem curta: so a primeira linha do driverInfo (o da Qualcomm tem varias).
  std::string info1 = s.driverInfo.substr(0, s.driverInfo.find('\n'));
  if (info1.size() > 80) info1.resize(80);
  char msg[512];
  snprintf(msg, sizeof(msg), "%s | %s %s (%s) | BC %s, ASTC %s, ETC2 %s", s.deviceName.c_str(),
           s.driverName.empty() ? DriverIdName(s.driverID) : s.driverName.c_str(), info1.c_str(),
           s.driverVersion.c_str(),
           s.bc ? "sim" : "nao", s.astc ? "sim" : "nao", s.etc2 ? "sim" : "nao");
  t.message = msg;
}

}  // namespace zdiag
