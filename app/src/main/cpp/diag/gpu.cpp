// zdiag: instancia/device e ajudas Vulkan.
#include "gpu.h"

#include <algorithm>

namespace zdiag {

static uint32_t VerNoPatch(uint32_t v) { return v & ~0xFFFu; }

Gpu::~Gpu() {
  if (t_.gpu_dead) {
    // GPU presa / perdida: destruir objetos ainda em uso e indefinido; o processo acaba a seguir.
    Log("Gpu: sem destruicao (%s)", t_.gpu_dead_reason.c_str());
    return;
  }
  if (dev) vk.DeviceWaitIdle(dev);
  Release(0);
  if (pool) vk.DestroyCommandPool(dev, pool, nullptr);
  if (dev) vk.DestroyDevice(dev, nullptr);
  if (inst) vk.DestroyInstance(inst, nullptr);
}

void Gpu::Release(size_t mark) {
  if (t_.gpu_dead) {
    if (trash_.size() > mark) trash_.resize(mark);
    return;
  }
  while (trash_.size() > mark) {
    auto f = std::move(trash_.back());
    trash_.pop_back();
    f();
  }
}

bool Gpu::HasExt(const char* name) const {
  for (const auto& e : devExts)
    if (!strcmp(e.extensionName, name)) return true;
  return false;
}

void Gpu::CreateInstance() {
  instanceVersion = VK_API_VERSION_1_0;
  if (vk.EnumerateInstanceVersion) vk.EnumerateInstanceVersion(&instanceVersion);
  uint32_t n = 0;
  vk.EnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
  std::vector<VkExtensionProperties> exts(n);
  vk.EnumerateInstanceExtensionProperties(nullptr, &n, exts.data());
  std::vector<const char*> enable;
  bool has_p2 = false;
  for (auto& e : exts)
    if (!strcmp(e.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) has_p2 = true;
  if (instanceVersion < VK_API_VERSION_1_1 && has_p2)
    enable.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "zdiag";
  app.pEngineName = "zdiag";
  app.apiVersion = instanceVersion >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_3 : VK_API_VERSION_1_0;
  VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ci.pApplicationInfo = &app;
  ci.enabledExtensionCount = uint32_t(enable.size());
  ci.ppEnabledExtensionNames = enable.data();
  VKC(vk.CreateInstance(&ci, nullptr, &inst));
  LoadInstanceFns(inst);
  props2 = vk.GetPhysicalDeviceProperties2 && vk.GetPhysicalDeviceFeatures2 &&
           (instanceVersion >= VK_API_VERSION_1_1 || has_p2);

  uint32_t count = 0;
  VKC(vk.EnumeratePhysicalDevices(inst, &count, nullptr));
  if (!count) throw std::runtime_error("nenhum VkPhysicalDevice");
  std::vector<VkPhysicalDevice> pds(count);
  VKC(vk.EnumeratePhysicalDevices(inst, &count, pds.data()));
  long idx = t_.opt.num("device", 0);
  pd = pds[size_t(std::clamp<long>(idx, 0, long(count) - 1))];
  vk.GetPhysicalDeviceProperties(pd, &props);
  vk.GetPhysicalDeviceFeatures(pd, &feats);
  vk.GetPhysicalDeviceMemoryProperties(pd, &memProps);
  apiVersion = std::min(VerNoPatch(props.apiVersion), VerNoPatch(app.apiVersion));
  if (VerNoPatch(instanceVersion) < VK_API_VERSION_1_1) apiVersion = VK_API_VERSION_1_0;
  n = 0;
  vk.EnumerateDeviceExtensionProperties(pd, nullptr, &n, nullptr);
  devExts.resize(n);
  vk.EnumerateDeviceExtensionProperties(pd, nullptr, &n, devExts.data());
}

void Gpu::CreateDevice(const DeviceReq& req) {
  uint32_t nq = 0;
  vk.GetPhysicalDeviceQueueFamilyProperties(pd, &nq, nullptr);
  std::vector<VkQueueFamilyProperties> qf(nq);
  vk.GetPhysicalDeviceQueueFamilyProperties(pd, &nq, qf.data());
  qfam = UINT32_MAX;
  for (uint32_t i = 0; i < nq; ++i) {
    if ((qf[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) ==
        (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
      qfam = i;
      break;
    }
  }
  if (qfam == UINT32_MAX) throw std::runtime_error("sem fila graphics+compute");

  const bool v12 = apiVersion >= VK_API_VERSION_1_2, v13 = apiVersion >= VK_API_VERSION_1_3;
  std::vector<const char*> exts;
  auto want_ext = [&](const char* e) {
    if (!HasExt(e)) return false;
    for (const char* x : exts)
      if (!strcmp(x, e)) return true;
    exts.push_back(e);
    return true;
  };

  VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
  VkPhysicalDeviceDynamicRenderingFeatures dr{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
  const bool bda_avail = props2 && (v12 || HasExt(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME));
  const bool dr_avail = props2 && (v13 || HasExt(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME));
  if (props2) {
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    void** next = &f2.pNext;
    if (bda_avail) { *next = &bda; next = &bda.pNext; }
    if (dr_avail) { *next = &dr; next = &dr.pNext; }
    vk.GetPhysicalDeviceFeatures2(pd, &f2);
  }
  hasInt64 = feats.shaderInt64;
  hasBc = feats.textureCompressionBC;
  hasDynRendering = dr_avail && dr.dynamicRendering;
  hasBda = bda_avail && bda.bufferDeviceAddress;
  hasCalibrated = HasExt("VK_KHR_calibrated_timestamps") || HasExt(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);

  VkPhysicalDeviceFeatures enabled{};
  void* chain = nullptr;
  VkPhysicalDeviceBufferDeviceAddressFeatures bda_on{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
  VkPhysicalDeviceDynamicRenderingFeatures dr_on{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
  if (req.bda && hasBda) {
    if (!v12) want_ext(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    bda_on.bufferDeviceAddress = VK_TRUE;
    bda_on.pNext = chain;
    chain = &bda_on;
    enabled.shaderInt64 = feats.shaderInt64;
  } else {
    hasBda = false;
  }
  if (req.dynamicRendering && hasDynRendering) {
    if (!v13) {
      want_ext(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
      if (!v12) {
        want_ext(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
        want_ext(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
        want_ext(VK_KHR_MULTIVIEW_EXTENSION_NAME);
        want_ext(VK_KHR_MAINTENANCE_2_EXTENSION_NAME);
      }
    }
    dr_on.dynamicRendering = VK_TRUE;
    dr_on.pNext = chain;
    chain = &dr_on;
  } else {
    hasDynRendering = false;
  }
  if (req.calibrated && hasCalibrated) {
    if (!want_ext("VK_KHR_calibrated_timestamps")) want_ext(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
  } else {
    hasCalibrated = false;
  }
  if (req.bc && hasBc) enabled.textureCompressionBC = VK_TRUE;
  else hasBc = false;

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = qfam;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;
  VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  ci.pNext = chain;
  ci.queueCreateInfoCount = 1;
  ci.pQueueCreateInfos = &qci;
  ci.enabledExtensionCount = uint32_t(exts.size());
  ci.ppEnabledExtensionNames = exts.data();
  ci.pEnabledFeatures = &enabled;
  VKC(vk.CreateDevice(pd, &ci, nullptr, &dev));
  LoadDeviceFns(dev);
  vk.GetDeviceQueue(dev, qfam, 0, &queue);
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.queueFamilyIndex = qfam;
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VKC(vk.CreateCommandPool(dev, &pci, nullptr, &pool));
  timeoutNs = uint64_t(t_.opt.num("timeout_ms", 10000)) * 1'000'000ull;
}

uint32_t Gpu::MemType(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags fallback) const {
  for (int pass = 0; pass < 2; ++pass) {
    VkMemoryPropertyFlags f = pass ? fallback : want;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
      if ((bits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & f) == f) return i;
  }
  throw std::runtime_error("sem tipo de memoria compativel");
}

Buffer Gpu::MakeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host, bool device_address,
                       bool readback) {
  Buffer b;
  b.size = size;
  VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  ci.size = size;
  ci.usage = usage | (device_address ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);
  VKC(vk.CreateBuffer(dev, &ci, nullptr, &b.buf));
  VkBuffer buf = b.buf;
  Defer([this, buf] { vk.DestroyBuffer(dev, buf, nullptr); });
  VkMemoryRequirements req;
  vk.GetBufferMemoryRequirements(dev, b.buf, &req);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  const VkMemoryPropertyFlags hv = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  ai.memoryTypeIndex =
      host ? MemType(req.memoryTypeBits,
                     hv | (readback ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), hv)
           : MemType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
  b.coherent = memProps.memoryTypes[ai.memoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
  flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
  if (device_address) ai.pNext = &flags;
  VKC(vk.AllocateMemory(dev, &ai, nullptr, &b.mem));
  VkDeviceMemory mem = b.mem;
  Defer([this, mem] { vk.FreeMemory(dev, mem, nullptr); });
  VKC(vk.BindBufferMemory(dev, b.buf, b.mem, 0));
  if (host) {
    void* p = nullptr;
    VKC(vk.MapMemory(dev, b.mem, 0, VK_WHOLE_SIZE, 0, &p));
    b.map = static_cast<uint8_t*>(p);
  }
  if (device_address) {
    VkBufferDeviceAddressInfo ii{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    ii.buffer = b.buf;
    b.addr = vk.GetBufferDeviceAddress(dev, &ii);
  }
  return b;
}

Image Gpu::MakeImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect) {
  Image im;
  im.w = w;
  im.h = h;
  im.format = fmt;
  VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ci.imageType = VK_IMAGE_TYPE_2D;
  ci.format = fmt;
  ci.extent = {w, h, 1};
  ci.mipLevels = 1;
  ci.arrayLayers = 1;
  ci.samples = VK_SAMPLE_COUNT_1_BIT;
  ci.tiling = VK_IMAGE_TILING_OPTIMAL;
  ci.usage = usage;
  ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VKC(vk.CreateImage(dev, &ci, nullptr, &im.img));
  VkImage img = im.img;
  Defer([this, img] { vk.DestroyImage(dev, img, nullptr); });
  VkMemoryRequirements req;
  vk.GetImageMemoryRequirements(dev, im.img, &req);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = MemType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
  VKC(vk.AllocateMemory(dev, &ai, nullptr, &im.mem));
  VkDeviceMemory mem = im.mem;
  Defer([this, mem] { vk.FreeMemory(dev, mem, nullptr); });
  VKC(vk.BindImageMemory(dev, im.img, im.mem, 0));
  if (usage & (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT)) {
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = im.img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange = {aspect, 0, 1, 0, 1};
    VKC(vk.CreateImageView(dev, &vi, nullptr, &im.view));
    VkImageView view = im.view;
    Defer([this, view] { vk.DestroyImageView(dev, view, nullptr); });
  }
  return im;
}

VkShaderModule Gpu::ShaderFromWords(const uint32_t* code, size_t bytes) {
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = bytes;
  ci.pCode = code;
  VkShaderModule m;
  VKC(vk.CreateShaderModule(dev, &ci, nullptr, &m));
  Defer([this, m] { vk.DestroyShaderModule(dev, m, nullptr); });
  return m;
}

VkShaderModule Gpu::Shader(const char* name) {
  const Spirv& s = FindShader(name);
  return ShaderFromWords(s.code, s.bytes);
}

VkRenderPass Gpu::RenderPass(VkFormat color, VkAttachmentLoadOp load, VkImageLayout initial, VkFormat depth,
                             VkAttachmentLoadOp depth_load) {
  VkAttachmentDescription att[2]{};
  att[0].format = color;
  att[0].samples = VK_SAMPLE_COUNT_1_BIT;
  att[0].loadOp = load;
  att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  att[0].initialLayout = load == VK_ATTACHMENT_LOAD_OP_LOAD ? initial : VK_IMAGE_LAYOUT_UNDEFINED;
  att[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  if (load != VK_ATTACHMENT_LOAD_OP_LOAD && initial != VK_IMAGE_LAYOUT_UNDEFINED) att[0].initialLayout = initial;
  att[1] = att[0];
  att[1].format = depth;
  att[1].loadOp = depth_load;
  att[1].stencilLoadOp = depth_load;
  att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
  att[1].initialLayout =
      depth_load == VK_ATTACHMENT_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
  att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  VkAttachmentReference cref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkAttachmentReference dref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &cref;
  if (depth != VK_FORMAT_UNDEFINED) sub.pDepthStencilAttachment = &dref;
  const VkPipelineStageFlags stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT |
                                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  const VkAccessFlags access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                               VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  VkSubpassDependency deps[2]{};
  deps[0] = {VK_SUBPASS_EXTERNAL, 0, stages, stages & ~VK_PIPELINE_STAGE_TRANSFER_BIT, access,
             access & ~VK_ACCESS_TRANSFER_WRITE_BIT, 0};
  deps[1] = {0, VK_SUBPASS_EXTERNAL, stages & ~VK_PIPELINE_STAGE_TRANSFER_BIT, stages,
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             access | VK_ACCESS_TRANSFER_READ_BIT, 0};
  VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  ci.attachmentCount = depth != VK_FORMAT_UNDEFINED ? 2 : 1;
  ci.pAttachments = att;
  ci.subpassCount = 1;
  ci.pSubpasses = &sub;
  ci.dependencyCount = 2;
  ci.pDependencies = deps;
  VkRenderPass rp;
  VKC(vk.CreateRenderPass(dev, &ci, nullptr, &rp));
  Defer([this, rp] { vk.DestroyRenderPass(dev, rp, nullptr); });
  return rp;
}

VkFramebuffer Gpu::Framebuffer(VkRenderPass rp, const Image& color, const Image* depth) {
  VkImageView views[2] = {color.view, depth ? depth->view : VK_NULL_HANDLE};
  VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  ci.renderPass = rp;
  ci.attachmentCount = depth ? 2 : 1;
  ci.pAttachments = views;
  ci.width = color.w;
  ci.height = color.h;
  ci.layers = 1;
  VkFramebuffer fb;
  VKC(vk.CreateFramebuffer(dev, &ci, nullptr, &fb));
  Defer([this, fb] { vk.DestroyFramebuffer(dev, fb, nullptr); });
  return fb;
}

VkPipelineLayout Gpu::Layout(const std::vector<VkDescriptorSetLayout>& sets, uint32_t push_bytes,
                             VkShaderStageFlags push_stages) {
  VkPushConstantRange pr{push_stages, 0, push_bytes};
  VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  ci.setLayoutCount = uint32_t(sets.size());
  ci.pSetLayouts = sets.data();
  ci.pushConstantRangeCount = push_bytes ? 1 : 0;
  ci.pPushConstantRanges = &pr;
  VkPipelineLayout l;
  VKC(vk.CreatePipelineLayout(dev, &ci, nullptr, &l));
  Defer([this, l] { vk.DestroyPipelineLayout(dev, l, nullptr); });
  return l;
}

VkPipeline Gpu::ComputePipeline(VkShaderModule mod, VkPipelineLayout layout, const VkSpecializationInfo* spec) {
  VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  ci.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  ci.stage.module = mod;
  ci.stage.pName = "main";
  ci.stage.pSpecializationInfo = spec;
  ci.layout = layout;
  VkPipeline p;
  VKC(vk.CreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, nullptr, &p));
  Defer([this, p] { vk.DestroyPipeline(dev, p, nullptr); });
  return p;
}

VkCommandBuffer Gpu::Begin() {
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = pool;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cb;
  VKC(vk.AllocateCommandBuffers(dev, &ai, &cb));
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VKC(vk.BeginCommandBuffer(cb, &bi));
  return cb;
}

void Gpu::Submit(const std::vector<VkCommandBuffer>& cbs, bool separate_submits) {
  for (VkCommandBuffer cb : cbs) VKC(vk.EndCommandBuffer(cb));
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence fence;
  VKC(vk.CreateFence(dev, &fci, nullptr, &fence));
  double t0 = NowMs();
  VkResult r = VK_SUCCESS;
  if (separate_submits) {
    for (size_t i = 0; i < cbs.size() && r == VK_SUCCESS; ++i) {
      VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
      si.commandBufferCount = 1;
      si.pCommandBuffers = &cbs[i];
      r = vk.QueueSubmit(queue, 1, &si, i + 1 == cbs.size() ? fence : VK_NULL_HANDLE);
    }
  } else {
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = uint32_t(cbs.size());
    si.pCommandBuffers = cbs.data();
    r = vk.QueueSubmit(queue, 1, &si, fence);
  }
  if (r == VK_SUCCESS) r = vk.WaitForFences(dev, 1, &fence, VK_TRUE, timeoutNs);
  lastSubmitCpuMs = NowMs() - t0;
  if (r != VK_SUCCESS) {
    NoteResult(r);
    throw VkFail(r, "vkQueueSubmit/vkWaitForFences");
  }
  vk.DestroyFence(dev, fence, nullptr);
  vk.FreeCommandBuffers(dev, pool, uint32_t(cbs.size()), cbs.data());
}

void Gpu::Barrier(VkCommandBuffer cb, VkImage img, VkImageLayout from, VkImageLayout to,
                  VkPipelineStageFlags src_stage, VkAccessFlags src_access, VkPipelineStageFlags dst_stage,
                  VkAccessFlags dst_access, VkImageAspectFlags aspect) {
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.srcAccessMask = src_access;
  b.dstAccessMask = dst_access;
  b.oldLayout = from;
  b.newLayout = to;
  b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = img;
  b.subresourceRange = {aspect, 0, 1, 0, 1};
  vk.CmdPipelineBarrier(cb, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

void Gpu::MemoryBarrier(VkCommandBuffer cb, VkPipelineStageFlags src_stage, VkAccessFlags src_access,
                        VkPipelineStageFlags dst_stage, VkAccessFlags dst_access) {
  VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  b.srcAccessMask = src_access;
  b.dstAccessMask = dst_access;
  vk.CmdPipelineBarrier(cb, src_stage, dst_stage, 0, 1, &b, 0, nullptr, 0, nullptr);
}

Buffer Gpu::RecordReadback(VkCommandBuffer cb, const Image& im, VkImageLayout from, VkPipelineStageFlags src_stage,
                           VkAccessFlags src_access) {
  Buffer b = MakeBuffer(VkDeviceSize(im.w) * im.h * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, false, true);
  Barrier(cb, im.img, from, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_stage, src_access, VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_TRANSFER_READ_BIT);
  VkBufferImageCopy c{};
  c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  c.imageExtent = {im.w, im.h, 1};
  vk.CmdCopyImageToBuffer(cb, im.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, b.buf, 1, &c);
  MemoryBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                VK_ACCESS_HOST_READ_BIT);
  return b;
}

std::vector<uint8_t> Gpu::ReadMapped(const Buffer& b) {
  if (!b.coherent) {
    VkMappedMemoryRange r{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    r.memory = b.mem;
    r.size = VK_WHOLE_SIZE;
    vk.InvalidateMappedMemoryRanges(dev, 1, &r);
  }
  return std::vector<uint8_t>(b.map, b.map + b.size);
}

void Gpu::Clear(VkCommandBuffer cb, const Image& im, VkImageLayout from, const float rgba[4], VkImageLayout to) {
  Barrier(cb, im.img, from, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT);
  VkClearColorValue c;
  memcpy(c.float32, rgba, 16);
  VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vk.CmdClearColorImage(cb, im.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &c, 1, &range);
  Barrier(cb, im.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, to, VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_TRANSFER_WRITE_BIT,
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
}

//-----------------------------------------------------------------------------
// Medicao de pixels
//-----------------------------------------------------------------------------
Coverage Measure(const std::vector<uint8_t>& px, int w, int h, uint32_t color, Rect r, bool inside, bool equal) {
  Coverage c;
  c.x0 = w; c.y0 = h; c.x1 = 0; c.y1 = 0;
  const uint32_t* p = reinterpret_cast<const uint32_t*>(px.data());
  for (int y = 0; y < h; ++y) {
    const bool row_in = y >= r.y && y < r.y + r.h;
    for (int x = 0; x < w; ++x) {
      const bool in = row_in && x >= r.x && x < r.x + r.w;
      if (in != inside) continue;
      c.total++;
      if ((p[size_t(y) * w + x] == color) == equal) {
        c.count++;
        c.x0 = std::min(c.x0, x); c.y0 = std::min(c.y0, y);
        c.x1 = std::max(c.x1, x + 1); c.y1 = std::max(c.y1, y + 1);
      }
    }
  }
  if (!c.count) c.x0 = c.y0 = c.x1 = c.y1 = 0;
  return c;
}

void CoverageJson(Json& j, const char* key, const Coverage& c) {
  j.k(key).obj().kv("count", c.count).kv("total", c.total)
      .kv("pct", c.total ? std::round(10000.0 * double(c.count) / double(c.total)) / 100 : 0.0);
  j.k("bbox").arr().v(c.x0).v(c.y0).v(c.x1 - c.x0).v(c.y1 - c.y0).end();
  j.end();
}

void RectJson(Json& j, const char* key, Rect r) { j.k(key).arr().v(r.x).v(r.y).v(r.w).v(r.h).end(); }

//-----------------------------------------------------------------------------
// Pipeline do triangulo de ecra inteiro
//-----------------------------------------------------------------------------
VkPipelineLayout SolidLayout(Gpu& g) {
  return g.Layout({}, 32, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

VkPipeline SolidPipeline(Gpu& g, VkPipelineLayout layout, const SolidPipeOpts& o) {
  VkPipelineShaderStageCreateInfo st[2]{};
  st[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT,
           g.Shader("fs_tri.vert"), "main", nullptr};
  st[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
           g.Shader("solid.frag"), "main", nullptr};
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.pViewports = o.static_viewport;
  vp.scissorCount = 1;
  vp.pScissors = o.static_scissor;
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds.depthTestEnable = o.depth_test;
  ds.depthWriteEnable = o.depth_test;
  ds.depthCompareOp = VK_COMPARE_OP_LESS;
  ds.maxDepthBounds = 1.0f;
  VkPipelineColorBlendAttachmentState ba{};
  ba.colorWriteMask = 0xF;
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &ba;
  std::vector<VkDynamicState> dyn;
  if (!o.static_viewport) dyn.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  if (!o.static_scissor) dyn.push_back(VK_DYNAMIC_STATE_SCISSOR);
  VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dy.dynamicStateCount = uint32_t(dyn.size());
  dy.pDynamicStates = dyn.data();
  VkPipelineRenderingCreateInfo ri{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  ri.colorAttachmentCount = 1;
  ri.pColorAttachmentFormats = &o.color_format;
  ri.depthAttachmentFormat = o.depth_format;
  VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  ci.pNext = o.rp ? nullptr : &ri;
  ci.stageCount = 2;
  ci.pStages = st;
  ci.pVertexInputState = &vi;
  ci.pInputAssemblyState = &ia;
  ci.pViewportState = &vp;
  ci.pRasterizationState = &rs;
  ci.pMultisampleState = &ms;
  ci.pDepthStencilState = o.depth_format != VK_FORMAT_UNDEFINED ? &ds : nullptr;
  ci.pColorBlendState = &cb;
  ci.pDynamicState = dyn.empty() ? nullptr : &dy;
  ci.layout = layout;
  ci.renderPass = o.rp;
  VkPipeline p;
  VKC(vk.CreateGraphicsPipelines(g.dev, VK_NULL_HANDLE, 1, &ci, nullptr, &p));
  VkDevice dev = g.dev;
  g.Defer([dev, p] { vk.DestroyPipeline(dev, p, nullptr); });
  return p;
}

void PushSolid(Gpu&, VkCommandBuffer cb, VkPipelineLayout layout, const float rgba[4], float z) {
  float pc[8] = {rgba[0], rgba[1], rgba[2], rgba[3], z, 0, 0, 0};
  vk.CmdPushConstants(cb, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32, pc);
}

void SetViewportScissor(VkCommandBuffer cb, Rect v, Rect s) {
  VkViewport vp{float(v.x), float(v.y), float(v.w), float(v.h), 0.0f, 1.0f};
  VkRect2D sc{{s.x, s.y}, {uint32_t(s.w), uint32_t(s.h)}};
  vk.CmdSetViewport(cb, 0, 1, &vp);
  vk.CmdSetScissor(cb, 0, 1, &sc);
}

VkFormat PickDepthFormat(Gpu& g) {
  for (VkFormat f : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM}) {
    VkFormatProperties fp;
    vk.GetPhysicalDeviceFormatProperties(g.pd, f, &fp);
    if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) return f;
  }
  return VK_FORMAT_D16_UNORM;
}

}  // namespace zdiag
