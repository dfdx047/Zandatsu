// Android: DXVK native 1.9.2 keeps its VkPipelineCache in memory only, and
// mobile drivers have no disk cache of their own for Vulkan. Each session then
// compiled every pipeline again the first time it was drawn: a 20-100 ms stall
// per new shader/state combination (the "random" hitches in races). This file
// replaces dxvk_pipecache.cpp (prepare.py copies it in) and keeps the driver's
// cache data next to the DXVK state cache (DXVK_STATE_CACHE_PATH).
//
// Safety: data is only offered to a driver whose vendor, device and
// pipelineCacheUUID match its header; a marker file around the load discards a
// cache whose load did not finish (driver crash), so a bad file cannot keep
// the game from starting. Without DXVK_STATE_CACHE_PATH nothing is stored.
#include "dxvk_pipecache.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

namespace dxvk {

  namespace {
    std::string cacheFile() {
      std::string directory = env::getEnvVar("DXVK_STATE_CACHE_PATH");
      if (directory.empty() || env::getEnvVar("DXVK_STATE_CACHE") == "0")
        return std::string();
      return directory + "/vulkan_pipelines.bin";
    }

    bool matchesDevice(const std::vector<char>& data, const VkPhysicalDeviceProperties& device) {
      // VkPipelineCacheHeaderVersionOne: length, version, vendorID, deviceID, UUID.
      if (data.size() < 32)
        return false;
      uint32_t header[4];
      std::memcpy(header, data.data(), sizeof(header));
      return header[0] >= 32 && header[1] == VK_PIPELINE_CACHE_HEADER_VERSION_ONE
          && header[2] == device.vendorID && header[3] == device.deviceID
          && !std::memcmp(data.data() + 16, device.pipelineCacheUUID, VK_UUID_SIZE);
    }
  }

  DxvkPipelineCache::DxvkPipelineCache(
    const Rc<vk::DeviceFn>&          vkd,
    const VkPhysicalDeviceProperties& device)
  : m_vkd(vkd), m_path(cacheFile()) {
    std::vector<char> data;
    if (!m_path.empty()) {
      std::string marker = m_path + ".loading";
      if (std::ifstream(marker).good()) {
        Logger::warn("DxvkPipelineCache: the last load did not finish, discarding the cache");
        std::remove(m_path.c_str());
      } else {
        std::ifstream file(m_path, std::ios::binary);
        data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        if (!data.empty() && !matchesDevice(data, device)) {
          Logger::info("DxvkPipelineCache: cache belongs to another driver, starting empty");
          data.clear();
        }
      }
      if (!data.empty())
        std::ofstream(marker) << "1";
    }

    VkPipelineCacheCreateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.pNext            = nullptr;
    info.flags            = 0;
    info.initialDataSize  = data.size();
    info.pInitialData     = data.empty() ? nullptr : data.data();

    VkResult status = m_vkd->vkCreatePipelineCache(m_vkd->device(), &info, nullptr, &m_handle);
    if (status != VK_SUCCESS && !data.empty()) {
      Logger::warn("DxvkPipelineCache: driver rejected the stored cache, starting empty");
      info.initialDataSize = 0;
      info.pInitialData    = nullptr;
      data.clear();
      status = m_vkd->vkCreatePipelineCache(m_vkd->device(), &info, nullptr, &m_handle);
    }
    if (!m_path.empty())
      std::remove((m_path + ".loading").c_str());
    if (status != VK_SUCCESS)
      throw DxvkError("DxvkPipelineCache: Failed to create cache");

    m_savedSize = data.size();
    if (!data.empty())
      Logger::info(str::format("DxvkPipelineCache: loaded ", data.size(), " bytes"));
    if (!m_path.empty())
      m_writer = std::thread([this] { run(); });
  }


  DxvkPipelineCache::~DxvkPipelineCache() {
    if (m_writer.joinable()) {
      { std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true; }
      m_signal.notify_all();
      m_writer.join();
    }
    m_vkd->vkDestroyPipelineCache(
      m_vkd->device(), m_handle, nullptr);
  }


  // Android ends the process without running destructors: save whenever the
  // cache grew, every few seconds while the game creates pipelines.
  void DxvkPipelineCache::run() {
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_stop) {
      m_signal.wait_for(lock, std::chrono::seconds(15));
      lock.unlock();
      save();
      lock.lock();
    }
  }


  void DxvkPipelineCache::save() {
    size_t size = 0;
    if (m_vkd->vkGetPipelineCacheData(m_vkd->device(), m_handle, &size, nullptr) != VK_SUCCESS
     || size <= m_savedSize)
      return;
    std::vector<char> data(size);
    if (m_vkd->vkGetPipelineCacheData(m_vkd->device(), m_handle, &size, data.data()) != VK_SUCCESS)
      return;
    std::string temporary = m_path + ".tmp";
    { std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
      file.write(data.data(), std::streamsize(size));
      if (!file) {
        Logger::warn("DxvkPipelineCache: could not write the cache");
        return;
      } }
    if (std::rename(temporary.c_str(), m_path.c_str()) == 0)
      m_savedSize = size;
  }

}
