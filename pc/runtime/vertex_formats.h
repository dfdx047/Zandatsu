#pragma once
// D3D9 vertex declaration types and the Vulkan formats DXVK fetches them with
// (DecodeDecltype). DXVK assumes every one can feed a vertex buffer; the SCALED
// ones are optional in Vulkan, and a GPU without them reads garbage positions
// (stretched geometry). Shared by the game runtime and the launcher's probe.
#include <vulkan/vulkan.h>
#include <string>

struct D3DVertexFormat { unsigned type; const char* name; VkFormat format; };
inline constexpr D3DVertexFormat kD3DVertexFormats[] = {
    {0, "FLOAT1", VK_FORMAT_R32_SFLOAT},
    {1, "FLOAT2", VK_FORMAT_R32G32_SFLOAT},
    {2, "FLOAT3", VK_FORMAT_R32G32B32_SFLOAT},
    {3, "FLOAT4", VK_FORMAT_R32G32B32A32_SFLOAT},
    {4, "D3DCOLOR", VK_FORMAT_B8G8R8A8_UNORM},
    {5, "UBYTE4", VK_FORMAT_R8G8B8A8_USCALED},
    {6, "SHORT2", VK_FORMAT_R16G16_SSCALED},
    {7, "SHORT4", VK_FORMAT_R16G16B16A16_SSCALED},
    {8, "UBYTE4N", VK_FORMAT_R8G8B8A8_UNORM},
    {9, "SHORT2N", VK_FORMAT_R16G16_SNORM},
    {10, "SHORT4N", VK_FORMAT_R16G16B16A16_SNORM},
    {11, "USHORT2N", VK_FORMAT_R16G16_UNORM},
    {12, "USHORT4N", VK_FORMAT_R16G16B16A16_UNORM},
    {13, "UDEC3", VK_FORMAT_A2B10G10R10_USCALED_PACK32},
    {14, "DEC3N", VK_FORMAT_A2B10G10R10_SNORM_PACK32},
    {15, "FLOAT16_2", VK_FORMAT_R16G16_SFLOAT},
    {16, "FLOAT16_4", VK_FORMAT_R16G16B16A16_SFLOAT},
};

// Space-separated names of the declaration types this GPU cannot fetch as vertex attributes.
// The query is passed in: the probe loads Vulkan (possibly a custom driver) dynamically.
inline std::string unsupportedD3DVertexFormats(VkPhysicalDevice gpu, PFN_vkGetPhysicalDeviceFormatProperties query) {
    std::string missing;
    for (auto& entry : kD3DVertexFormats) {
        VkFormatProperties properties{};
        query(gpu, entry.format, &properties);
        if (!(properties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)) {
            if (!missing.empty()) missing += ' ';
            missing += entry.name;
        }
    }
    return missing;
}
