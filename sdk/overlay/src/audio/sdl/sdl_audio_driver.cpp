/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2020 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <array>
#include <cstdio>
#include <cstring>

#include <rex/assert.h>
#include <rex/audio/conversion.h>
#include <rex/audio/downmix.h>
#include <rex/audio/flags.h>
#include <rex/audio/sdl/sdl_audio_driver.h>
#include <rex/cvar.h>
#include <rex/dbg.h>
#include <rex/logging.h>
#include <rex/perf/counter.h>
#include <SDL3/SDL.h>

REXCVAR_DEFINE_BOOL(audio_mute, false, "Audio", "Mute audio output");
REXCVAR_DEFINE_DOUBLE(audio_gain, 1.0, "Audio", "Output gain (0.0 - 1.0)");
REXCVAR_DEFINE_STRING(dbz3_audio_dump, "", "DBZ3/Dev",
                      "Graba la salida de audio (float32 estereo, 48 kHz, sin cabecera) en este archivo para los videos; "
                      "se graba aunque el juego este silenciado. Vacio = no");
// dbz3 - focus behaviour. `dbz3_window_focused` is written by the app when the
// window gains/loses focus (same name in the launcher registry, so the value is
// visible in the config); with `dbz3_mute_unfocused` (launcher: "mute when
// leaving the window") the mix is silenced while the window is in the
// background, like most emulators and games do.
REXCVAR_DEFINE_BOOL(dbz3_mute_unfocused, true, "DBZ3/Audio",
                    "Silence audio while the window is in the background");
REXCVAR_DEFINE_BOOL(dbz3_window_focused, true, "DBZ3/Dev",
                    "Window focus state (written by the app, not a user setting)");

namespace rex::audio::sdl {

SDLAudioDriver::SDLAudioDriver(memory::Memory* memory, rex::thread::Semaphore* semaphore)
    : AudioDriver(memory), semaphore_(semaphore) {}

SDLAudioDriver::~SDLAudioDriver() {
  assert_true(frames_queued_.empty());
  assert_true(frames_unused_.empty());
}

bool SDLAudioDriver::Initialize() {
  // Set audio category for proper OS audio handling
  SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");

  // Set app name for audio device identification
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "rexglue");

  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    REXAPU_ERROR("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: {}", SDL_GetError());
    return false;
  }
  sdl_initialized_ = true;

  SDL_AudioSpec desired_spec = {};
  SDL_AudioSpec obtained_spec = {};
  desired_spec.freq = frame_frequency_;
  desired_spec.format = SDL_AUDIO_F32LE;
  desired_spec.channels = frame_channels_;
  sdl_device_channels_ = frame_channels_;
  sdl_stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec,
                                          SDLCallback, this);
  if (!sdl_stream_) {
    REXAPU_ERROR("SDL_OpenAudioDeviceStream() failed: {}", SDL_GetError());
    return false;
  }

  SDL_AudioDeviceID sdl_device = SDL_GetAudioStreamDevice(sdl_stream_);
  if (!sdl_device) {
    REXAPU_ERROR("SDL_GetAudioStreamDevice() failed: {}", SDL_GetError());
    return false;
  }

  if (!SDL_GetAudioDeviceFormat(sdl_device, &obtained_spec, NULL)) {
    REXAPU_WARN("SDL_GetAudioDeviceFormat() failed: {}", SDL_GetError());
    obtained_spec = desired_spec;
  }

  // A 1-channel device gets the stereo fold too, then SDL collapses to mono.
  // Handing it a 6ch stream instead would use SDL's own downmix.
  if (obtained_spec.channels <= 2) {
    SDL_DestroyAudioStream(sdl_stream_);
    sdl_stream_ = nullptr;
    desired_spec.channels = 2;
    sdl_device_channels_ = 2;
    sdl_stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec,
                                            SDLCallback, this);
    if (!sdl_stream_) {
      REXAPU_ERROR("SDL_OpenAudioDeviceStream() stereo fallback failed: {}", SDL_GetError());
      return false;
    }
    sdl_device = SDL_GetAudioStreamDevice(sdl_stream_);
    if (!sdl_device) {
      REXAPU_ERROR("SDL_GetAudioStreamDevice() failed after stereo fallback: {}", SDL_GetError());
      return false;
    }
  }

  // The endpoint layout decides which mix the callback runs, and it is the
  // first thing worth knowing when a report says the balance is wrong on one
  // speaker setup and right on another.
  const char* device_name = SDL_GetAudioDeviceName(sdl_device);
  REXAPU_INFO("audio endpoint '{}': {} ch, {} Hz, format 0x{:04X}; submitting {} ch",
              device_name ? device_name : "?", obtained_spec.channels, obtained_spec.freq,
              static_cast<uint32_t>(obtained_spec.format), static_cast<int>(sdl_device_channels_));

  if (!SDL_ResumeAudioDevice(sdl_device)) {
    REXAPU_ERROR("SDL_ResumeAudioDevice() failed: {}", SDL_GetError());
    return false;
  }

  return true;
}

void SDLAudioDriver::SubmitFrame(uint32_t frame_ptr) {
  const auto input_frame = memory_->TranslateVirtual<float*>(frame_ptr);
  float* output_frame;
  {
    std::unique_lock<std::mutex> guard(frames_mutex_);
    if (frames_unused_.empty()) {
      output_frame = new float[frame_samples_];
    } else {
      output_frame = frames_unused_.top();
      frames_unused_.pop();
    }
  }

  std::memcpy(output_frame, input_frame, frame_samples_ * sizeof(float));

  static uint32_t sdl_submit_count = 0;
  if (sdl_submit_count < 10) {
    REXAPU_DEBUG("SDLAudioDriver::SubmitFrame: frame_ptr={:08X} queued_count={}", frame_ptr,
                 frames_queued_.size() + 1);
    sdl_submit_count++;
  }

  {
    std::unique_lock<std::mutex> guard(frames_mutex_);
    frames_queued_.push(output_frame);
    PROFILE_BUFFER_QUEUE_DEPTH(static_cast<int64_t>(frames_queued_.size()));
  }
}

void SDLAudioDriver::Shutdown() {
  if (sdl_stream_) {
    SDL_DestroyAudioStream(sdl_stream_);
    sdl_stream_ = nullptr;
  }
  if (sdl_initialized_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    sdl_initialized_ = false;
  }
  std::unique_lock<std::mutex> guard(frames_mutex_);
  while (!frames_unused_.empty()) {
    delete[] frames_unused_.top();
    frames_unused_.pop();
  }
  while (!frames_queued_.empty()) {
    delete[] frames_queued_.front();
    frames_queued_.pop();
  }
}

// DBZ3 (v1.4.1): nivel de la mezcla del guest (antes de mute/volumen), una linea cada
// 5 s con `dbz3_perf_logging`: `audio pico=0.000` sostenido = el juego no esta
// generando sonido (p. ej. "con el mod de personajes no suena la musica"), distinto
// de "lo silencia el launcher" (mute/volumen/ventana sin foco).
static void Dbz3TrackAudioLevel(const float* buffer, uint32_t channel_samples) {
  static float peak = 0.0f;
  static double sum_sq = 0.0;
  static uint64_t samples = 0;
  static std::chrono::steady_clock::time_point window_start;
  // 6 canales secuenciales en big-endian (el formato del XAudio del guest).
  const uint32_t n = channel_samples * 6;
  const uint32_t* words = reinterpret_cast<const uint32_t*>(buffer);
  for (uint32_t i = 0; i < n; ++i) {
    uint32_t w = words[i];
    w = (w >> 24) | ((w >> 8) & 0xFF00u) | ((w << 8) & 0xFF0000u) | (w << 24);
    float v;
    std::memcpy(&v, &w, sizeof(v));
    if (!(v == v)) continue;  // NaN
    const float a = v < 0 ? -v : v;
    if (a > peak) peak = a;
    sum_sq += double(v) * double(v);
  }
  samples += n;
  const auto now = std::chrono::steady_clock::now();
  if (window_start.time_since_epoch().count() == 0) {
    window_start = now;
    return;
  }
  if (now - window_start < std::chrono::seconds(5)) return;
  if (rex::cvar::GetFlagByName("dbz3_perf_logging") == "true") {
    REXAPU_INFO("dbz3: audio pico={:.3f} rms={:.4f}", peak,
                samples ? std::sqrt(sum_sq / double(samples)) : 0.0);
  }
  peak = 0.0f;
  sum_sq = 0.0;
  samples = 0;
  window_start = now;
}

// dbz3_audio_dump: la mezcla estereo (antes del silencio por mute/foco) a un archivo crudo f32le. La primera escritura
// deja en el log su hora del sistema en ms, para juntarla con el video (ffmpeg -itsoffset).
static void Dbz3DumpAudio(const float* data, size_t floats) {
  static std::FILE* f = nullptr;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const std::string path = REXCVAR_GET(dbz3_audio_dump);
    if (!path.empty()) {
      f = std::fopen(path.c_str(), "wb");
      const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count();
      REXAPU_INFO("dbz3 audio: volcado {} -> {} (inicio {} ms, 48000 Hz, 2 canales f32le)", f ? "abierto" : "FALLO", path,
                  static_cast<long long>(ms));
    }
  }
  if (f) std::fwrite(data, sizeof(float), floats, f);
}

void SDLAudioDriver::SDLCallback(void* userdata, SDL_AudioStream* stream, int additional_amount,
                                 [[maybe_unused]] int total_amount) {
  SCOPE_profile_cpu_f("apu");
  if (!userdata || !stream) {
    REXAPU_ERROR("SDLAudioDriver::SDLCallback called with nullptr.");
    return;
  }
  const auto driver = static_cast<SDLAudioDriver*>(userdata);
  const int sample_count =
      static_cast<int>(channel_samples_ * std::max<uint8_t>(driver->sdl_device_channels_, 1));
  const int len = static_cast<int>(sizeof(float) * sample_count);
  float* data = SDL_stack_alloc(float, sample_count);
  if (!data) {
    REXAPU_ERROR("SDLAudioDriver::SDLCallback failed to allocate {} samples", sample_count);
    return;
  }
  // Snapshot once. A change mid-callback would split the frame across two mixes.
  const StereoFold fold = GetStereoFold();
  const SurroundMix mix = GetSurroundMix();
  // User-facing output gain (launcher "Volume" slider). Clamped so a stale or
  // hand-edited config can never boost/clip the mix; 0.0 silences it just like
  // audio_mute, while 1.0 leaves the guest mix untouched.
  const double gain_scale =
      std::min(std::max(REXCVAR_GET(audio_gain), 0.0), 1.0);
  const float gain = GetOutputGain() * static_cast<float>(gain_scale);
  while (additional_amount > 0) {
    static uint32_t sdl_callback_count = 0;
    std::unique_lock<std::mutex> guard(driver->frames_mutex_);
    if (driver->frames_queued_.empty()) {
      if (sdl_callback_count < 10) {
        REXAPU_DEBUG("SDLCallback: no frames queued (silence)");
        sdl_callback_count++;
      }
      std::memset(data, 0, len);
      if (driver->sdl_device_channels_ == 2) Dbz3DumpAudio(data, size_t(sample_count));
      if (!SDL_PutAudioStreamData(stream, data, len)) {
        REXAPU_ERROR("SDL_PutAudioStreamData() failed while filling silence: {}", SDL_GetError());
        break;
      }
      additional_amount -= len;
    } else {
      auto buffer = driver->frames_queued_.front();
      driver->frames_queued_.pop();
      Dbz3TrackAudioLevel(buffer, channel_samples_);
      // se mezcla siempre (el volcado de dbz3_audio_dump graba aunque este silenciado) y despues se calla si toca
      switch (driver->sdl_device_channels_) {
        case 2:
          conversion::sequential_6_BE_to_interleaved_2_LE(data, buffer, channel_samples_, fold, gain);
          Dbz3DumpAudio(data, size_t(sample_count));
          break;
        case 6:
          conversion::sequential_6_BE_to_interleaved_6_LE(data, buffer, channel_samples_, mix, gain);
          break;
        default:
          assert_unhandled_case(driver->sdl_device_channels_);
          break;
      }
      if (REXCVAR_GET(audio_mute) ||
          (REXCVAR_GET(dbz3_mute_unfocused) && !REXCVAR_GET(dbz3_window_focused))) {
        std::memset(data, 0, len);
      }
      if (!SDL_PutAudioStreamData(stream, data, len)) {
        REXAPU_ERROR("SDL_PutAudioStreamData() failed: {}", SDL_GetError());
        driver->frames_unused_.push(buffer);
        break;
      }
      driver->frames_unused_.push(buffer);

      auto ret = driver->semaphore_->Release(1, nullptr);
      assert_true(ret);
      additional_amount -= len;
    }
  }
  SDL_stack_free(data);
}

}  // namespace rex::audio::sdl
