#include "guest_runtime.h"
#include <cstdio>
int main(int argc,char** argv) {
    if (argc!=2) return 64;
    auto status=connectGuestRuntime(argv[1]);
    std::puts(status.c_str());
    // Initial bring-up must execute the real CRT and reach a named HLE
    // boundary, not fail PE parsing, memory/copy checks or dispatch.
    return status.find("HLE pendiente:")!=std::string::npos ||
        status.find("D3D9 CreateDevice requires Android surface")!=std::string::npos ? 0 : 1;
}
