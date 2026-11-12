// Zandatsu (12/11): DLC importados pelo jogador.
//
// Os DLC do MGR no 360 (Jetstream, Blade Wolf, VR Missions, trajes) são pacotes STFS ("LIVE") só com dados:
// data101..108 (.cpk/.dat), vídeos .usm, som e SPA.BIN. O código que os usa já está no default.xex: falta o jogo
// encontrá-los onde a XAM os procura. A launcher copia cada pacote escolhido para <pasta do app>/dlc/; aqui, antes do
// jogo arrancar, cada pacote novo é instalado com o ContentManager do SDK (o "Install Content" do Xenia: monta o STFS
// e extrai para <dados>/0000000000000000/<TitleID>/00000002/<nome>/, mais o .header com a licença do pacote). Uma
// marca "<nome>.instalado" (com o nome do DLC) evita repetir e o pacote original é apagado (a cópia extraída chega).

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>

#if defined(__ANDROID__)
#include <rex/main_android.h>
#endif

namespace {

bool LooksLikeStfs(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  char magic[4] = {};
  if (!f.read(magic, 4)) return false;
  const std::string m(magic, 4);
  return m == "LIVE" || m == "PIRS" || m == "CON ";
}

// O nome no cabecalho STFS (0x411, UTF-16 big-endian), so ASCII (chega para os nomes dos DLC do MGR).
std::string PackageDisplayName(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  char raw[0x80] = {};
  f.seekg(0x411);
  if (!f.read(raw, sizeof(raw))) return path.filename().string();
  std::string name;
  for (size_t i = 0; i + 1 < sizeof(raw); i += 2) {
    const char16_t c = char16_t((uint8_t(raw[i]) << 8) | uint8_t(raw[i + 1]));
    if (c == 0) break;
    name += c < 0x80 ? char(c) : '?';
  }
  return name.empty() ? path.filename().string() : name;
}

}  // namespace

void SwordManInstallImportedDlc() {
#if defined(__ANDROID__)
  const std::filesystem::path dir = rex::GetAndroidAppDirectory() / "dlc";
#else
  const std::filesystem::path dir = std::filesystem::current_path() / "dlc";
#endif
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return;
  auto* state = REX_KERNEL_STATE();
  auto* content = state ? state->content_manager() : nullptr;
  if (!content) {
    REXLOG_WARN("dlc: sem ContentManager, pacotes por instalar ficam para a próxima");
    return;
  }
  uint32_t installed = 0, skipped = 0, failed = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file(ec)) continue;
    const auto& path = entry.path();
    if (path.extension() == ".instalado") {  // instalado num arranque anterior (o pacote ja saiu)
      ++skipped;
      continue;
    }
    if (path.extension() == ".part") continue;  // a launcher ainda a copiar
    auto marker = path;
    marker += ".instalado";
    if (std::filesystem::exists(marker, ec)) continue;
    if (!LooksLikeStfs(path)) {
      REXLOG_WARN("dlc: {} não é um pacote do 360 (LIVE/PIRS/CON), ignorado", path.filename().string());
      ++failed;
      continue;
    }
    REXLOG_INFO("dlc: a instalar {} ({:.1f} MB)", path.filename().string(),
                double(std::filesystem::file_size(path, ec)) / (1024.0 * 1024.0));
    const auto result = content->InstallContent(path);
    if (result != 0) {
      REXLOG_ERROR("dlc: {} falhou (X_RESULT {:08X})", path.filename().string(), uint32_t(result));
      ++failed;
      continue;
    }
    // A marca guarda o nome do DLC (a launcher mostra-o); o pacote sai, o jogo le o que foi extraido.
    if (FILE* f = std::fopen(marker.c_str(), "w")) {
      std::fputs(PackageDisplayName(path).c_str(), f);
      std::fclose(f);
    }
    std::filesystem::remove(path, ec);
    ++installed;
  }
  REXLOG_INFO("dlc: {} instalados agora, {} já instalados, {} com erro", installed, skipped, failed);
}
