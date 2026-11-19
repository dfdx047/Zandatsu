#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
struct AndroidRenderMode {uint32_t width,height;};
inline std::array<AndroidRenderMode,6> androidRenderModes(uint32_t width,uint32_t height,uint32_t initialWidth,uint32_t initialHeight) {
    auto scaled=[&](uint32_t percent){return AndroidRenderMode{
        std::max(320u,(width*percent/100u)&~1u),std::max(240u,(height*percent/100u)&~1u)};};
    return {scaled(25),scaled(50),scaled(75),AndroidRenderMode{width,height},
        AndroidRenderMode{1280,720},AndroidRenderMode{initialWidth,initialHeight}};
}
