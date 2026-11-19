#pragma once
#include "dxt_decode.h"
#include <memory>
#include <algorithm>
struct CompressedTexture {
    uint32_t format,width,height,levels,faces;
    std::vector<std::vector<uint8_t>> data;
    CompressedTexture(uint32_t format_,uint32_t width_,uint32_t height_,uint32_t levels_,uint32_t faces_)
        :format(format_),width(width_),height(height_),levels(levels_),faces(faces_),data(levels_*faces_){
        if(!levels||levels>32||!faces||faces>6)throw std::invalid_argument("Invalid compressed texture layout");
        for(uint32_t face=0;face<faces;++face)for(uint32_t level=0;level<levels;++level)
            bytes(face,level).resize(size_t(pitch(level))*((mipHeight(level)+3)/4));
    }
    uint32_t mode()const{return format==0x31545844?1:format==0x32545844||format==0x33545844?3:5;}
    uint32_t blockBytes()const{return mode()==1?8:16;}
    uint32_t mipWidth(uint32_t level)const{return std::max(1u,width>>level);}
    uint32_t mipHeight(uint32_t level)const{return std::max(1u,height>>level);}
    uint32_t pitch(uint32_t level)const{return ((mipWidth(level)+3)/4)*blockBytes();}
    std::vector<uint8_t>& bytes(uint32_t face,uint32_t level){return data.at(face*levels+level);}
};
struct CompressedView {std::shared_ptr<CompressedTexture> texture;uint32_t level=0,face=0;};
