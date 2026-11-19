#pragma once

// Android replacement for dxvk_pipecache.h (see pipeline_cache.cpp).
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

#include "dxvk_include.h"

#include "../util/sha1/sha1_util.h"
#include "../util/util_env.h"
#include "../util/util_time.h"

namespace dxvk {

  /**
   * \brief Pipeline cache
   *
   * Allows the Vulkan implementation to re-use previously compiled
   * pipelines, across sessions when DXVK_STATE_CACHE_PATH is set.
   */
  class DxvkPipelineCache : public RcObject {

  public:

    DxvkPipelineCache(
      const Rc<vk::DeviceFn>&           vkd,
      const VkPhysicalDeviceProperties& device);
    ~DxvkPipelineCache();

    /**
     * \brief Pipeline cache handle
     * \returns Pipeline cache handle
     */
    VkPipelineCache handle() const {
      return m_handle;
    }

  private:

    Rc<vk::DeviceFn>        m_vkd;
    VkPipelineCache         m_handle = VK_NULL_HANDLE;
    std::string             m_path;
    size_t                  m_savedSize = 0;
    std::mutex              m_mutex;
    std::condition_variable m_signal;
    bool                    m_stop = false;
    std::thread             m_writer;

    void run();
    void save();

  };

}
