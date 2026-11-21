#pragma once
// Present effects for the DXVK 1.9.2b D3D9 swap chain (MGR:R PC port on Android), off unless enabled by env:
//   DXVK_FSR=0|1|2          0 off (plain DXVK blit), 1 AMD FSR 1 (EASU upscale + RCAS sharpen), 2 CAS only
//                           (sharpen + upscale in one pass). Runs from the back buffer (the game's render size)
//                           to the swap chain image (the window size).
//   DXVK_FSR_SHARPNESS=0..1 1 = sharpest (default 0.8). RCAS: 2 * (1 - s) stops; CAS: lerp(8, 5, s).
//   DXVK_FSR_OUT=0|720|1080 output height: 0 = the screen; 720 = 720p window buffers that the compositor then
//                           scales (cheaper on weak GPUs). Read by android_wsi.cpp, which sizes the window.
//   DXVK_FI=0|1             frame generation: before each game frame, a frame generated halfway between the
//                           previous one and it (fx/dxvk_fi.glsl, port of the 360 port's 0092). Forces FIFO;
//                           +1 display frame of latency. For a game capped at 30 fps on a 60 Hz display.
//   DXVK_FI_QUALITY=0|1|2   0 blend only, 1 motion search (default), 2 motion + 4 neighbour block vectors.
//   DXVK_FI_HZ=<Hz>         display refresh rate (launcher); each frame is presented round(Hz / 60) times.
// Statistics (GPU ms per pass, generated / not generated frames) go to logcat tag MGR, "present-fx", every
// 300 game frames while an effect is on.
// Hooks: prepare.py patches D3D9SwapChainEx::Present / PresentImage, DxvkSwapchainBlitter (gamma ramp) and
// DxvkContext (timestamp outside a render pass); this file and present_fx.cpp are compiled from the port dir.
#include "src/dxvk/dxvk_device.h"
#include "src/dxvk/dxvk_context.h"
#include <array>
#include <chrono>

namespace dxvk {

  class DxvkSwapchainBlitter;

  struct DxvkPresentFxConfig {
    int      fsr        = 0;     // 0 off, 1 FSR 1 (EASU + RCAS), 2 CAS
    float    sharpness  = 0.8f;
    bool     fi         = false;
    int      fiQuality  = 1;
    uint32_t displayHz  = 60;
  };

  const DxvkPresentFxConfig& presentFxConfig();

  class DxvkPresentFx : public RcObject {

  public:

    explicit DxvkPresentFx(const Rc<DxvkDevice>& device);
    ~DxvkPresentFx();

    /** Frame generation needs FIFO: the generated and the real frame are each shown for one vblank. */
    static bool forceVsync() { return presentFxConfig().fi; }

    /** Presents per frame (generated and real alike): round(display Hz / 60) with frame generation, else 1. */
    uint32_t repeat() const { return m_repeat; }

    /** Start of a game Present: true if a generated frame is presented before this one. */
    bool beginFrame(const Rc<DxvkImageView>& backBuffer);

    /** Records the frame generation (also stores the back buffer as the next history); returns the generated
     *  image, at the back buffer's size. Only after beginFrame returned true. */
    Rc<DxvkImageView> generate(DxvkContext* ctx, const Rc<DxvkImageView>& backBuffer);

    /** Frame generation on but nothing generated this time: store the back buffer as the history. */
    void keepHistory(DxvkContext* ctx, const Rc<DxvkImageView>& backBuffer);

    /** FSR / CAS from srcView to the swap chain image. False = not handled (effect off, MSAA, sub-rectangles,
     *  downscale): the caller uses the stock blitter. */
    bool present(
            DxvkContext*            ctx,
            DxvkSwapchainBlitter*   blitter,
      const Rc<DxvkImageView>&      dstView,
            VkRect2D                dstRect,
      const Rc<DxvkImageView>&      srcView,
            VkRect2D                srcRect);

    /** End of a game Present: statistics. */
    void endFrame();

  private:

    static constexpr uint32_t Slots = 8;

    struct FiImage {
      Rc<DxvkImage>     image;
      Rc<DxvkImageView> sampled;
      Rc<DxvkImageView> storage;
    };

    struct FiPush {
      int32_t full_size[2];
      int32_t q_size[2];
      int32_t s_size[2];
      int32_t b_size[2];
      int32_t k;
      int32_t quality;
      int32_t mode;
      int32_t pad;
    };

    struct Timer {
      Rc<DxvkGpuQuery> begin, end;
      bool pending = false;
    };

    struct Slot {
      Timer up;               // FSR / CAS passes (all presents of the frame)
      Timer fi;               // frame generation passes
      bool  countersPending = false;
      bool  generated = false;
    };

    Rc<DxvkDevice>      m_device;
    DxvkPresentFxConfig m_config;
    uint32_t            m_repeat = 1;

    Rc<DxvkShader>      m_vs;
    Rc<DxvkShader>      m_fsEasu;
    Rc<DxvkShader>      m_fsRcas;
    Rc<DxvkShader>      m_fsCas;
    std::array<Rc<DxvkShader>, 4> m_csFi;
    Rc<DxvkSampler>     m_linear;

    Rc<DxvkImage>       m_easuImage;
    Rc<DxvkImageView>   m_easuTarget;
    Rc<DxvkImageView>   m_easuView;

    // Frame generation (sizes as in fx/dxvk_fi.glsl).
    VkExtent2D          m_fiSize = { 0, 0 };
    int32_t             m_fiK = 1;
    VkExtent2D          m_fiQ = { }, m_fiS = { }, m_fiB = { };
    FiImage             m_hist[2], m_q[2], m_s[2], m_vs8, m_v8, m_gen;
    uint32_t            m_parity = 0;   // [m_parity] holds the previous frame
    bool                m_historyValid = false;
    bool                m_fiBroken = false;
    Rc<DxvkBuffer>      m_counters;
    std::chrono::steady_clock::time_point m_lastFrame;

    // Statistics, per frame slot (a slot comes round again 8 frames later).
    std::array<Slot, Slots> m_slots;
    uint64_t            m_frame = 0;
    float               m_tsPeriod = 1.0f;
    struct {
      uint64_t frames = 0, generated = 0, notGenerated = 0, cuts = 0, upscaled = 0;
      uint64_t pixels = 0, moving = 0, fallback = 0;
      double   upNs = 0, fiNs = 0;
      uint64_t upSamples = 0, fiSamples = 0;
    } m_stats;
    std::string         m_upLabel;

    // Lab: DXVK_FX_DUMP=<game frame> writes that frame's back buffer, generated frame and upscaled output as PPM
    // files to DXVK_FX_DUMP_DIR (default: DXVK_STATE_CACHE_PATH, the app's files dir), read 4 frames later.
    struct Dump {
      Rc<DxvkBuffer> buffer;
      VkExtent2D     extent;
      bool           bgra;
      std::string    name;
    };
    int64_t             m_dumpFrame = -1;
    std::vector<Dump>   m_dumps;
    bool dumping() const { return m_dumpFrame >= 0 && int64_t(m_frame) == m_dumpFrame; }
    void dumpImage(DxvkContext* ctx, const Rc<DxvkImage>& image, const char* name);
    void writeDumps();

    void createShaders();
    bool ensureFi(VkExtent2D size);
    FiImage createImage(VkExtent2D size, VkFormat format, bool attachment);
    void recordFi(DxvkContext* ctx, const Rc<DxvkImageView>& cur, bool generate);
    void collect(Slot& slot);
    double timerNs(Timer& timer);
    void startTimer(DxvkContext* ctx, Timer& timer);
    void stopTimer(DxvkContext* ctx, Timer& timer);

    void draw(
            DxvkContext*            ctx,
      const Rc<DxvkShader>&         fs,
      const Rc<DxvkImageView>&      dstView,
            VkExtent2D              dstExtent,
      const Rc<DxvkImageView>&      srcView,
      const Rc<DxvkImageView>&      gammaView,
      const void*                   push,
            uint32_t                pushSize,
            uint32_t                specValue);

  };

}
