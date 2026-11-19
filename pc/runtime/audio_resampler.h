#pragma once
#include <array>
#include <cmath>

namespace nfs_audio {
// Upsampling must suppress spectral images. A two-point linear interpolator
// leaves audible images when the game's 22.05 kHz mixer feeds 48 kHz output.
// Precompute a normalized 16-tap Blackman-windowed sinc outside the callback;
// interpolate between phases to avoid quantizing the fractional play cursor.
struct UpsampleKernel {
    static constexpr unsigned taps=16,phases=256;
    std::array<std::array<float,taps>,phases+1> weights{};
    UpsampleKernel(){
        constexpr double pi=3.14159265358979323846;
        for(unsigned phase=0;phase<=phases;++phase){
            double sum=0;
            for(unsigned tap=0;tap<taps;++tap){
                double x=int(tap)-7-double(phase)/phases;
                double sinc=std::abs(x)<1e-12?1:std::sin(pi*x)/(pi*x);
                double window=std::abs(x)>8?0:0.42+0.5*std::cos(pi*x/8)+0.08*std::cos(2*pi*x/8);
                weights[phase][tap]=float(sinc*window);sum+=weights[phase][tap];
            }
            for(float& weight:weights[phase])weight=float(weight/sum);
        }
    }
};
inline const UpsampleKernel& upsampleKernel(){static const UpsampleKernel kernel;return kernel;}
}
