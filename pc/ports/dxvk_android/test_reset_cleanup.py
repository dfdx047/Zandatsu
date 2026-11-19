"""Emit a host fault-injection test using the actual prepared DXVK method bodies.

Usage: python ports/dxvk_android/test_reset_cleanup.py build/dxvk-source work/reset_cleanup.cpp
Compile the emitted file with a C++17 host compiler and run it.
"""
import argparse
import pathlib


def method(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    args = parser.parse_args()
    swap = (args.source / 'src/d3d9/d3d9_swapchain.cpp').read_text()
    presenter = (args.source / 'src/vulkan/vulkan_presenter.cpp').read_text()
    create = method(swap, 'void D3D9SwapChainEx::CreateBackBuffers(')
    allocation = create[create.index('    try {'):create.index('    auto swapImage')]
    test = r'''
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <iostream>
constexpr uint64_t VK_NULL_HANDLE=0;
struct D3D9SwapChainEx;
struct D3D9Surface {
  static int attempt,fail,alive,cleared;
  D3D9Surface(void*,int*,D3D9SwapChainEx*) {
    if(attempt++==fail) throw std::runtime_error("injected image allocation failure");
    ++alive;
  }
  ~D3D9Surface(){--alive;}
  void ClearContainer(){++cleared;}
};
int D3D9Surface::attempt=0,D3D9Surface::fail=-1,D3D9Surface::alive=0,D3D9Surface::cleared=0;
struct Buffer {
  std::shared_ptr<D3D9Surface> value;
  bool operator!=(std::nullptr_t)const{return bool(value);}
  D3D9Surface* operator->(){return value.get();}
  Buffer& operator=(D3D9Surface* p){value.reset(p);return *this;}
};
struct D3D9SwapChainEx {
  std::vector<Buffer> m_backBuffers;
  void* m_parent=nullptr;
  void DestroyBackBuffers();
  void allocate(unsigned count);
};
struct Fn {
  uint64_t device(){return 1;}
  uint64_t instance(){return 1;}
  void vkDestroyImageView(uint64_t,uint64_t,const void*){}
  void vkDestroySemaphore(uint64_t,uint64_t,const void*){}
  void (*vkDestroySwapchainKHR)(uint64_t,uint64_t,const void*)=nullptr;
  void (*vkDestroySurfaceKHR)(uint64_t,uint64_t,const void*)=nullptr;
};
struct Presenter {
  Fn* m_vkd; Fn* m_vki;
  uint64_t m_swapchain=0,m_surface=0;
  struct Image{uint64_t view;}; struct Sync{uint64_t acquire,present;};
  std::vector<Image> m_images; std::vector<Sync> m_semaphores;
  void destroySwapchain(); void destroySurface();
};
'''
    test += method(swap, 'void D3D9SwapChainEx::DestroyBackBuffers()') + '\n'
    test += 'void D3D9SwapChainEx::allocate(unsigned count){ DestroyBackBuffers(); m_backBuffers.resize(count); int desc=0;\n' + allocation + '}\n'
    test += method(presenter, 'void Presenter::destroySwapchain()') + '\n'
    test += method(presenter, 'void Presenter::destroySurface()') + '\n'
    test += r'''
int main(){
  D3D9SwapChainEx chain;
  for(int fail=0;fail<4;++fail){
    D3D9Surface::attempt=0;D3D9Surface::fail=fail;D3D9Surface::cleared=0;
    bool caught=false;try{chain.allocate(4);}catch(const std::runtime_error&){caught=true;}
    assert(caught && chain.m_backBuffers.empty());
    assert(D3D9Surface::alive==0 && D3D9Surface::cleared==fail);
    chain.DestroyBackBuffers();
    D3D9Surface::fail=-1;chain.allocate(4);assert(D3D9Surface::alive==4);
    chain.DestroyBackBuffers();assert(D3D9Surface::alive==0 && D3D9Surface::cleared==fail+4);
  }
  Fn fn;Presenter p{&fn,&fn};
  // Missing extension function pointers must not be called for absent handles.
  p.destroySwapchain();p.destroySurface();
  static int destroys=0;
  auto destroy=[](uint64_t,uint64_t handle,const void*){assert(handle!=0);++destroys;};
  fn.vkDestroySwapchainKHR=destroy;fn.vkDestroySurfaceKHR=destroy;
  p.m_swapchain=10;p.m_surface=20;
  p.destroySwapchain();p.destroySurface();p.destroySwapchain();p.destroySurface();
  assert(destroys==2 && p.m_swapchain==0 && p.m_surface==0);
  std::cout<<"DXVK cleanup passed: allocation failures at every slot, retry, empty teardown, null extension pointers and repeated destruction\n";
}
'''
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(test, encoding='utf-8')


if __name__ == '__main__':
    main()
