constexpr uint16_t cp1252High[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
constexpr char16_t cp437High[]=u"ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";
static_assert(sizeof(cp437High)/sizeof(char16_t)==129,"CP437 table size");
uint32_t effectiveCP(uint32_t cp){return cp==0||cp==3?1252:cp==1?437:cp;}
uint32_t decodeByte(uint32_t cp,uint8_t byte){if(cp==1252&&byte>=128&&byte<160)return cp1252High[byte-128];if(cp==437&&byte>=128)return cp437High[byte-128];return byte;}
int encodeByte(uint32_t cp,uint32_t code){for(unsigned i=0;i<256;++i)if(decodeByte(cp,static_cast<uint8_t>(i))==code)return static_cast<int>(i);return -1;}
void wideToMulti() {
    uint32_t cp=effectiveCP(arg(0)),flags=arg(1),source=arg(2),out=arg(4),capacity=arg(5);
    int32_t count=static_cast<int32_t>(arg(3));
    if((cp!=1252&&cp!=437&&cp!=65001)||!source||count==0||count< -1||(capacity&&!out)) {lastError=87;ret(0,8);return;}
    if(cp==65001 && (arg(6)||arg(7))) {lastError=87;ret(0,8);return;}
    std::vector<uint16_t> input;
    if(count==-1){input=wideString(source);input.push_back(0);}else{input.resize(count);std::memcpy(input.data(),ptr(source,uint64_t(count)*2),uint64_t(count)*2);}
    bool used=false;unsigned char fallback=arg(6)?*static_cast<unsigned char*>(ptr(arg(6),1)):'?';
    std::vector<unsigned char> bytes;
    for(size_t i=0;i<input.size();++i) {
        uint32_t code=input[i];
        if(code>=0xd800&&code<=0xdbff&&i+1<input.size()&&input[i+1]>=0xdc00&&input[i+1]<=0xdfff) {code=0x10000+((code-0xd800)<<10)+(input[++i]-0xdc00);}
        else if(code>=0xd800&&code<=0xdfff){if(flags&0x80){lastError=1113;ret(0,8);return;}code=0xfffd;}
        if(cp!=65001){int b=encodeByte(cp,code);bytes.push_back(b<0?fallback:static_cast<unsigned char>(b));used|=b<0;}
        else if(code<0x80)bytes.push_back(static_cast<unsigned char>(code));
        else if(code<0x800){bytes.push_back(static_cast<unsigned char>(0xc0|(code>>6)));bytes.push_back(static_cast<unsigned char>(0x80|(code&63)));}
        else if(code<0x10000){bytes.push_back(static_cast<unsigned char>(0xe0|(code>>12)));bytes.push_back(static_cast<unsigned char>(0x80|((code>>6)&63)));bytes.push_back(static_cast<unsigned char>(0x80|(code&63)));}
        else{bytes.push_back(static_cast<unsigned char>(0xf0|(code>>18)));bytes.push_back(static_cast<unsigned char>(0x80|((code>>12)&63)));bytes.push_back(static_cast<unsigned char>(0x80|((code>>6)&63)));bytes.push_back(static_cast<unsigned char>(0x80|(code&63)));}
    }
    if(arg(7))write32(arg(7),used);
    if(capacity&&capacity<bytes.size()){lastError=122;ret(0,8);return;}
    if(capacity)std::memcpy(ptr(out,bytes.size()),bytes.data(),bytes.size());
    ret(static_cast<uint32_t>(bytes.size()),8);
}
void multiToWide() {
    uint32_t cp=effectiveCP(arg(0)),flags=arg(1),source=arg(2),out=arg(4),capacity=arg(5);
    int32_t count=static_cast<int32_t>(arg(3));
    if((cp!=1252&&cp!=437&&cp!=65001)||!source||count==0||count< -1||(capacity&&!out)){lastError=87;ret(0,6);return;}
    std::string input=count==-1?guestString(source)+std::string(1,0):std::string(static_cast<char*>(ptr(source,count)),count);
    std::vector<uint16_t> chars;
    for(size_t i=0;i<input.size();) {
        uint32_t code=static_cast<unsigned char>(input[i++]);
        if(cp!=65001)code=decodeByte(cp,static_cast<uint8_t>(code));
        else if(code>=128) {
            size_t start=i;uint32_t n=code>=0xf0&&code<=0xf4?3:code>=0xe0&&code<=0xef?2:code>=0xc2&&code<=0xdf?1:0;
            bool invalid=!n||input.size()-i<n;uint32_t minimum=n==1?0x80:n==2?0x800:0x10000;
            code&=n==1?31:n==2?15:7;
            if(!invalid)for(uint32_t k=0;k<n;++k){uint8_t next=static_cast<uint8_t>(input[i++]);if((next&0xc0)!=0x80){invalid=true;break;}code=(code<<6)|(next&63);}
            invalid|=code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff);
            if(invalid){if(flags&8){lastError=1113;ret(0,6);return;}code=0xfffd;i=start;}
        }
        if(code>0xffff){code-=0x10000;chars.push_back(static_cast<uint16_t>(0xd800|(code>>10)));chars.push_back(static_cast<uint16_t>(0xdc00|(code&1023)));}
        else chars.push_back(static_cast<uint16_t>(code));
    }
    if(capacity&&capacity<chars.size()){lastError=122;ret(0,6);return;}
    if(capacity)std::memcpy(ptr(out,chars.size()*2),chars.data(),chars.size()*2);
    ret(static_cast<uint32_t>(chars.size()),6);
}
// Unicode classification uses a fixed table, independent of Android's locale.
#include "guest_ctype_table.h"
#include "guest_case_table.h"
void mapStringW() {
    uint32_t locale=arg(0),flags=arg(1),source=arg(2),out=arg(4),capacity=arg(5);
    int32_t count=static_cast<int32_t>(arg(3));
    if(locale!=0&&locale!=0x409&&locale!=0x400&&locale!=0x800&&locale!=0x7f)
        throw GuestStop("LCMapStringW locale pending: "+hex(locale));
    if(flags!=0x100&&flags!=0x200)throw GuestStop("LCMapStringW flags pending: "+hex(flags));
    if(!source||count==0||count< -1||(capacity&&!out)){lastError=87;ret(0,6);return;}
    if(count==-1){count=0;while(count<32768){uint16_t c;std::memcpy(&c,ptr(source+count*2,2),2);++count;if(!c)break;}}
    if(!capacity){ret(count,6);return;}
    if(capacity<static_cast<uint32_t>(count)){lastError=122;ret(0,6);return;}
    const auto* begin=flags==0x100?std::begin(lowerCase):std::begin(upperCase);
    const auto* end=flags==0x100?std::end(lowerCase):std::end(upperCase);
    std::vector<uint16_t> result(count);
    for(int32_t i=0;i<count;++i){uint16_t c;std::memcpy(&c,ptr(source+i*2,2),2);
        auto mapping=std::lower_bound(begin,end,c,[](const CaseMapping& item,uint16_t value){return item.source<value;});
        result[i]=mapping!=end&&mapping->source==c?mapping->target:c;}
    std::memcpy(ptr(out,count*2),result.data(),count*2);ret(count,6);
}
void stringTypeW() {
    uint32_t type=arg(0),source=arg(1),out=arg(3);
    int32_t count=static_cast<int32_t>(arg(2));
    if(type!=1)throw GuestStop("GetStringTypeW classification level "+std::to_string(type)+" pending");
    if(!source||!out||count==0||count< -1){lastError=87;ret(0,4);return;}
    if(count==-1){count=0;while(count<32768){uint16_t c;std::memcpy(&c,ptr(source+count*2,2),2);++count;if(!c)break;}}
    for(int32_t i=0;i<count;++i){
        uint16_t c;std::memcpy(&c,ptr(source+i*2,2),2);
        auto range=std::lower_bound(std::begin(ctypeRanges),std::end(ctypeRanges),c,
            [](const CTypeRange& item,uint16_t value){return item.end<value;});
        std::memcpy(ptr(out+i*2,2),&range->flags,2);
    }
    ret(1,4);
}
