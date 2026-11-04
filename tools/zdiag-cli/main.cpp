// zdiag_cli: corre os testes do zdiag a partir de adb shell.
//   zdiag_cli list
//   zdiag_cli <teste|all> <outDir> [opcoes]
// "all" corre cada teste num processo filho (fork), como a app faz, e diz onde um filho morreu.
// Opcoes extra do CLI (no mesmo texto chave=valor;...): driver_dir, driver_name, native_lib_dir.
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#include "zdiag.h"

using namespace zdiag;

static std::string ReadAll(const std::string& path) {
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "list") {
    printf("%s\n", TestsJson().c_str());
    return 0;
  }
  if (argc < 3) {
    fprintf(stderr, "uso: %s list | <teste|all> <outDir> [chave=valor;...]\ntestes: %s\n", argv[0],
            TestsJson().c_str());
    return 2;
  }
  const std::string test = argv[1], out = argv[2], options = argc > 3 ? argv[3] : "";
  Options o = Options::Parse(options);
  LoaderInfo li{o.str("native_lib_dir"), o.str("driver_dir"), o.str("driver_name")};
  if (test != "all") {
    std::string json = RunTest(test, out, li, options);
    printf("%s\n", json.c_str());
    return json.find("\"status\":\"fail\"") != std::string::npos ? 1 : 0;
  }
  int bad = 0;
  for (const TestDef& d : AllTests()) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
      std::string json = RunTest(d.id, out, li, options);
      _exit(json.find("\"status\":\"fail\"") != std::string::npos ? 1 : 0);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    if (WIFSIGNALED(st)) {
      std::string step = ReadAll(out + "/" + d.id + ".progress");
      step = step.substr(0, step.find('\n'));  // linha 1 = passo; linha 2 = signal=N
      printf("%-20s CRASH sinal %d: fechou em %s/%s\n", d.id, WTERMSIG(st), d.id, step.c_str());
      bad++;
      continue;
    }
    std::string json = ReadAll(out + "/" + d.id + ".json");
    auto field = [&](const char* k) {
      std::string key = std::string("\"") + k + "\":\"";
      size_t p = json.find(key);
      if (p == std::string::npos) return std::string("?");
      p += key.size();
      return json.substr(p, json.find('"', p) - p);
    };
    printf("%-20s %-4s %s\n", d.id, field("status").c_str(), field("message").c_str());
    if (WEXITSTATUS(st)) bad++;
  }
  return bad ? 1 : 0;
}
