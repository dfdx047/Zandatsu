"""patch_pass_timing.py <prepared DXVK dir>: GPU time per render pass (dxvk_pass_timing.h, env DXVK_PASS_TIMING).
Idempotent; prepare.py runs it after its own patches. Lab diagnostics only: does nothing unless the env is set."""
import pathlib
import shutil
import sys

root = pathlib.Path(sys.argv[1])
shutil.copy2(pathlib.Path(__file__).parent / "dxvk_pass_timing.h", root / "src/dxvk/dxvk_pass_timing.h")


def replace(path, old, new):
    file = root / path
    text = file.read_text()
    if new in text:
        return
    if old not in text:
        raise SystemExit("pass timing patch mismatch: " + path)
    file.write_text(text.replace(old, new, 1))


H = "src/dxvk/dxvk_cmdlist.h"
replace(H, '#include "dxvk_stats.h"', '#include "dxvk_stats.h"\n#include "dxvk_pass_timing.h"')
replace(H, """    void cmdInsertDebugUtilsLabel(VkDebugUtilsLabelEXT *pLabelInfo);

  private:
""", """    void cmdInsertDebugUtilsLabel(VkDebugUtilsLabelEXT *pLabelInfo);

    // DXVK_PASS_TIMING: timestamps around each render pass (outside it, so tilers are not split).
    void passTimingBegin(const passtiming::Label& label) {
      m_ptOpen = false;
      if (m_ptCount + 2 > PtQueries)
        return;
      if (!m_ptPool) {
        VkQueryPoolCreateInfo info = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        info.queryCount = PtQueries;
        if (m_vkd->vkCreateQueryPool(m_vkd->device(), &info, nullptr, &m_ptPool) != VK_SUCCESS)
          return;
        m_vkd->vkCmdResetQueryPool(m_execBuffer, m_ptPool, 0, PtQueries);
      }
      m_vkd->vkCmdWriteTimestamp(m_execBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_ptPool, m_ptCount++);
      m_ptLabels.push_back(label);
      m_ptOpen = true;
    }

    void passTimingEnd(uint32_t draws) {
      if (!m_ptOpen)
        return;
      m_vkd->vkCmdWriteTimestamp(m_execBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_ptPool, m_ptCount++);
      m_ptLabels.back().draws = draws;
      m_ptOpen = false;
    }

  private:

    static constexpr uint32_t PtQueries = 1024;
    VkQueryPool m_ptPool = VK_NULL_HANDLE;
    uint32_t m_ptCount = 0;
    bool m_ptOpen = false;
    std::vector<passtiming::Label> m_ptLabels;
""")

C = "src/dxvk/dxvk_cmdlist.cpp"
replace(C, """  DxvkCommandList::~DxvkCommandList() {
    this->reset();
""", """  DxvkCommandList::~DxvkCommandList() {
    this->reset();
    if (m_ptPool)
      m_vkd->vkDestroyQueryPool(m_vkd->device(), m_ptPool, nullptr);
""")
replace(C, """    m_cmdBuffersUsed = DxvkCmdBuffer::ExecBuffer;
  }""", """    m_cmdBuffersUsed = DxvkCmdBuffer::ExecBuffer;

    if (m_ptPool)
      m_vkd->vkCmdResetQueryPool(m_execBuffer, m_ptPool, 0, PtQueries);
    m_ptCount = 0;
    m_ptOpen = false;
    m_ptLabels.clear();
  }""")
replace(C, """    // Less important stuff
    m_statCounters.reset();
  }""", """    // Less important stuff
    m_statCounters.reset();

    if (m_ptCount && m_ptPool) {
      std::vector<uint64_t> ts(m_ptCount);
      if (m_vkd->vkGetQueryPoolResults(m_vkd->device(), m_ptPool, 0, m_ptCount, ts.size() * sizeof(uint64_t),
          ts.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS)
        passtiming::submit(m_ptLabels, ts.data(), m_ptCount,
          m_device->adapter()->deviceProperties().limits.timestampPeriod);
      m_ptCount = 0;
      m_ptLabels.clear();
    }
  }""")

X = "src/dxvk/dxvk_context.cpp"
replace(X, """    m_cmd->cmdBeginRenderPass(&info,
      VK_SUBPASS_CONTENTS_INLINE);""", """    if (unlikely(passtiming::enabled())) {
      passtiming::Label label = { passtiming::nextOrdinal(), fbSize.width, fbSize.height, 0, 0, 0,
        framebuffer->numAttachments(), 0 };
      if (framebuffer->getColorTarget(0).view != nullptr)
        label.color0 = framebuffer->getColorTarget(0).view->info().format;
      if (framebuffer->getColorTarget(1).view != nullptr)
        label.color1 = framebuffer->getColorTarget(1).view->info().format;
      if (framebuffer->getDepthTarget().view != nullptr)
        label.depth = framebuffer->getDepthTarget().view->info().format;
      m_ptDraws = 0;
      m_cmd->passTimingBegin(label);
    }

    m_cmd->cmdBeginRenderPass(&info,
      VK_SUBPASS_CONTENTS_INLINE);""")
replace(X, """  void DxvkContext::renderPassUnbindFramebuffer() {
    m_cmd->cmdEndRenderPass();
  }""", """  void DxvkContext::renderPassUnbindFramebuffer() {
    m_cmd->cmdEndRenderPass();
    if (unlikely(passtiming::enabled()))
      m_cmd->passTimingEnd(m_ptDraws);
  }


  void DxvkContext::passTimingFrame() {
    if (unlikely(passtiming::enabled()))
      passtiming::frame();
  }""")
replace(X, """  template<bool Indexed, bool Indirect>
  bool DxvkContext::commitGraphicsState() {
""", """  template<bool Indexed, bool Indirect>
  bool DxvkContext::commitGraphicsState() {
    m_ptDraws++;
""")

XH = "src/dxvk/dxvk_context.h"
replace(XH, """    void insertDebugLabel(VkDebugUtilsLabelEXT *label);
  private:
""", """    void insertDebugLabel(VkDebugUtilsLabelEXT *label);

    /** DXVK_PASS_TIMING: a Present was recorded (resets the pass ordinal, logs the table). */
    void passTimingFrame();
  private:
    uint32_t m_ptDraws = 0;
""")

S = "src/d3d9/d3d9_swapchain.cpp"
replace(S, """  void D3D9SwapChainEx::PresentImage(UINT SyncInterval) {
    m_parent->Flush();""", """  void D3D9SwapChainEx::PresentImage(UINT SyncInterval) {
    if (unlikely(passtiming::enabled()))
      m_parent->EmitCs([] (DxvkContext* ctx) { ctx->passTimingFrame(); });
    m_parent->Flush();""")
print("pass timing patch applied to", root)
