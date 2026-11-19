void winFormatA(){
    uint32_t output=arg(0),arguments=arg(2);auto format=guestString(arg(1));
    std::string result;
    for(size_t position=0;position<format.size();){
        if(format[position]!='%'){result+=format[position++];continue;}
        ++position;if(position<format.size()&&format[position]=='%'){result+='%';++position;continue;}
        bool left=false,zero=false,alternate=false;
        while(position<format.size()){
            char c=format[position];if(c=='-')left=true;else if(c=='0')zero=true;else if(c=='#')alternate=true;else break;++position;
        }
        unsigned width=0;int precision=-1;
        while(position<format.size()&&std::isdigit(static_cast<unsigned char>(format[position]))){width=width*10+unsigned(format[position++]-'0');if(width>1023)throw GuestStop("wvsprintf width exceeds Windows buffer");}
        if(position<format.size()&&format[position]=='.'){
            ++position;precision=0;while(position<format.size()&&std::isdigit(static_cast<unsigned char>(format[position]))){precision=precision*10+(format[position++]-'0');if(precision>1023)throw GuestStop("wvsprintf precision exceeds Windows buffer");}
        }
        char length=0;if(position<format.size()&&(format[position]=='l'||format[position]=='h'))length=format[position++];
        if(position==format.size())throw GuestStop("Incomplete wvsprintf conversion");
        char conversion=format[position++];uint32_t value=read32(arguments);arguments+=4;std::string piece;
        if(conversion=='s'){
            if(length=='l')throw GuestStop("wvsprintf wide string pending");piece=value?guestString(value):"(null)";
            if(precision>=0&&piece.size()>size_t(precision))piece.resize(precision);
        }else if(conversion=='c')piece+=char(value);
        else if(conversion=='d'||conversion=='i'||conversion=='u'||conversion=='x'||conversion=='X'||conversion=='p'){
            std::string spec="%";if(left)spec+='-';if(zero)spec+='0';if(alternate)spec+='#';
            if(conversion=='p'){spec+="08X";}else{if(width)spec+=std::to_string(width);if(precision>=0)spec+='.'+std::to_string(precision);spec+=conversion;}
            char buffer[1024];int count=(conversion=='d'||conversion=='i')?std::snprintf(buffer,sizeof buffer,spec.c_str(),int32_t(value)):std::snprintf(buffer,sizeof buffer,spec.c_str(),value);
            if(count<0||count>=1024)throw GuestStop("wvsprintf result exceeds Windows buffer");piece.assign(buffer,count);
        }else throw GuestStop("wvsprintf conversion pending: "+std::string(1,conversion));
        if(width>piece.size()){size_t padding=width-piece.size();if(left)piece.append(padding,' ');else piece.insert(0,padding,' ');}
        result+=piece;if(result.size()>1023)throw GuestStop("wvsprintf result exceeds Windows buffer");
    }
    if(result.size()>1023)throw GuestStop("wvsprintf result exceeds Windows buffer");
    std::memcpy(ptr(output,result.size()+1),result.c_str(),result.size()+1);ret(uint32_t(result.size()),3);
}
