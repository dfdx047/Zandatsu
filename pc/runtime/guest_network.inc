// Winsock 2 (ws2_32.dll, imported by ordinal) on Android BSD sockets, for EA
// Nation online play. Host sockets are always non-blocking; a guest blocking
// call waits in short poll slices with the machine released so other guest
// threads keep running and cancellation stays immediate.
//
// gethostbyname also carries the community-server redirect (what the PC
// "eanationhub.asi" mod does): any *.ea.com name resolves onlineServer instead.
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

std::string onlineServer="eahub.eu";
namespace net {
constexpr uint32_t socketError=0xffffffffu,invalidSocket=0xffffffffu;
constexpr uint32_t wsaEinval=10022,wsaEwouldblock=10035,wsaEnotsock=10038,wsaEfault=10014,wsaEmsgsize=10040,
    wsaEtimedout=10060,wsaEafnosupport=10047,wsaNotinitialised=10093,wsaHostNotFound=11001,wsaNoData=11004;
constexpr char localHostName[]="nfsu2-android";
// icmp: a Windows raw ICMP socket (ping) on an unprivileged Linux ping socket; see icmpReceive.
struct Socket{int fd;bool guestNonBlocking=false,connecting=false,icmp=false;uint16_t icmpId=0;uint32_t recvTimeout=0,sendTimeout=0;std::set<std::string> peersLogged;
    struct Counter{uint32_t sentPackets=0,sentBytes=0,receivedPackets=0,receivedBytes=0;};
    std::map<std::string,Counter> traffic;std::chrono::steady_clock::time_point nextSummary{};};
std::map<uint32_t,Socket> sockets;
uint32_t nextHandle=0x1000,startups=0,hostent=0;

uint32_t wsaError(int error){
    switch(error){
    case EAGAIN:case EINPROGRESS:return wsaEwouldblock;
    case EINTR:return 10004;case EBADF:case ENOTSOCK:return wsaEnotsock;case EACCES:return 10013;case EFAULT:return wsaEfault;
    case EINVAL:return wsaEinval;case EMFILE:case ENFILE:return 10024;case EALREADY:return 10037;case EDESTADDRREQ:return 10039;
    case EMSGSIZE:return wsaEmsgsize;case EPROTOTYPE:return 10041;case ENOPROTOOPT:return 10042;case EPROTONOSUPPORT:return 10043;
    case EOPNOTSUPP:return 10045;case EAFNOSUPPORT:return wsaEafnosupport;case EADDRINUSE:return 10048;case EADDRNOTAVAIL:return 10049;
    case ENETDOWN:return 10050;case ENETUNREACH:return 10051;case ENETRESET:return 10052;case ECONNABORTED:return 10053;
    case ECONNRESET:case EPIPE:return 10054;case ENOBUFS:case ENOMEM:return 10055;case EISCONN:return 10056;case ENOTCONN:return 10057;
    case ESHUTDOWN:return 10058;case ETIMEDOUT:return wsaEtimedout;case ECONNREFUSED:return 10061;case EHOSTDOWN:return 10064;
    case EHOSTUNREACH:return 10065;
    default:
        NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Winsock: host errno %d (%s) reported as WSAENETDOWN",error,std::strerror(error));
        return 10050;
    }
}
// Runs f with the guest machine released (blocking host calls: DNS, poll).
template<class F> auto released(F f){
    if(!machineLease||!machineLease->owns_lock())throw GuestStop("Guest scheduler ownership missing");
    State saved;saved.save();machineLease->unlock();
    auto result=f();int error=errno;
    machineLease->lock();saved.load();errno=error;
    return result;
}
Socket* find(uint32_t handle){auto found=sockets.find(handle);return found==sockets.end()?nullptr:&found->second;}
// Waits until fd reports one of events; timeout in ms, 0 = forever. False on timeout.
bool wait(int fd,short events,uint32_t timeout){
    WaitScope scope(events&POLLOUT?"socket send/connect":"socket recv/accept",uint32_t(fd));
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    for(;;){
        checkGuestProgress();
        int slice=50;
        if(timeout){
            auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
            if(left<=0)return false;
            slice=int(std::min<long long>(left,50));
        }
        pollfd entry{fd,events,0};
        int ready=released([&]{return ::poll(&entry,1,slice);});
        if(ready!=0)return true;  // ready, or a poll error the retried call will report
    }
}
// One socket call with Windows blocking semantics over a non-blocking fd.
template<class Op> ssize_t blocking(Socket& socket,short events,uint32_t timeout,Op op){
    int fd=socket.fd;bool nonBlocking=socket.guestNonBlocking;
    for(;;){
        ssize_t result=op(fd);
        if(result>=0||(errno!=EAGAIN&&errno!=EWOULDBLOCK)||nonBlocking)return result;
        if(!wait(fd,events,timeout)){errno=ETIMEDOUT;return -1;}
    }
}
uint32_t fail(uint32_t error){lastError=error;return socketError;}
uint32_t failErrno(){return fail(wsaError(errno));}
socklen_t guestAddress(uint32_t address,uint32_t length,sockaddr_storage& out){
    if(!address||int32_t(length)<16||length>sizeof out)return 0;
    std::memset(&out,0,sizeof out);std::memcpy(&out,ptr(address,length),length);  // SOCKADDR_IN matches sockaddr_in
    return socklen_t(length);
}
bool storeAddress(uint32_t address,uint32_t lengthPointer,const sockaddr_storage& value,socklen_t length){
    if(!address)return true;
    if(!lengthPointer)return false;
    uint32_t capacity=read32(lengthPointer);
    if(int32_t(capacity)<int32_t(length))return false;
    std::memcpy(ptr(address,length),&value,length);write32(lengthPointer,length);return true;
}
uint32_t newHandle(int fd,bool nonBlocking){
    uint32_t handle=nextHandle;nextHandle+=4;
    sockets[handle]=Socket{fd,nonBlocking};return handle;
}
uint16_t checksum(const uint8_t* data,size_t length){
    uint32_t sum=0;
    for(size_t i=0;i+1<length;i+=2)sum+=uint32_t(data[i])<<8|data[i+1];
    if(length&1)sum+=uint32_t(data[length-1])<<8;
    while(sum>>16)sum=(sum&0xffff)+(sum>>16);
    return uint16_t(~sum);
}
// The kernel's ping socket swaps the echo identifier for its own and returns the
// ICMP message alone; Windows raw sockets return the IPv4 header too. Restore the
// guest's identifier and prepend a header so the guest parses the reply as on PC.
std::vector<uint8_t> icmpPacket(const Socket& socket,std::vector<uint8_t> message,const sockaddr_in& peer){
    if(message.size()>=8&&(message[0]==0||message[0]==8)){
        message[4]=uint8_t(socket.icmpId>>8);message[5]=uint8_t(socket.icmpId);
        message[2]=message[3]=0;uint16_t sum=checksum(message.data(),message.size());message[2]=uint8_t(sum>>8);message[3]=uint8_t(sum);
    }
    std::vector<uint8_t> packet(20+message.size());
    uint16_t total=uint16_t(packet.size());
    packet[0]=0x45;packet[2]=uint8_t(total>>8);packet[3]=uint8_t(total);packet[8]=64;packet[9]=1;
    std::memcpy(&packet[12],&peer.sin_addr,4);
    sockaddr_in local{};socklen_t length=sizeof local;
    if(::getsockname(socket.fd,reinterpret_cast<sockaddr*>(&local),&length)==0)std::memcpy(&packet[16],&local.sin_addr,4);
    uint16_t sum=checksum(packet.data(),20);packet[10]=uint8_t(sum>>8);packet[11]=uint8_t(sum);
    std::memcpy(&packet[20],message.data(),message.size());
    return packet;
}
void noteIcmpSend(Socket& socket,const void* data,uint32_t length){
    auto* bytes=static_cast<const uint8_t*>(data);
    if(socket.icmp&&length>=8)socket.icmpId=uint16_t(bytes[4]<<8|bytes[5]);
}
// Peer-to-peer diagnostics: the first datagrams of each socket and every hard error.
std::string describe(const sockaddr_storage& address){
    if(address.ss_family!=AF_INET)return "?";
    auto* in=reinterpret_cast<const sockaddr_in*>(&address);char text[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&in->sin_addr,text,sizeof text);
    return std::string(text)+":"+std::to_string(ntohs(in->sin_port));
}
// Logs each peer address a socket talks to the first time it appears (up to 32 per socket).
void notePeer(Socket& socket,const char* direction,const std::string& peer,ssize_t bytes){
    if(socket.peersLogged.size()>=32||!socket.peersLogged.insert(std::string(direction)+peer).second)return;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: %s %s (primer paquete, %d bytes)",direction,peer.c_str(),int(bytes));
}
// Per-peer datagram counters, summarized every 5 s while a socket is active.
void count(uint32_t handle,Socket& socket,const std::string& peer,bool sent,ssize_t bytes){
    if(bytes<0)return;
    auto& counter=socket.traffic[peer];
    if(sent){++counter.sentPackets;counter.sentBytes+=uint32_t(bytes);}else{++counter.receivedPackets;counter.receivedBytes+=uint32_t(bytes);}
    auto now=std::chrono::steady_clock::now();
    if(now<socket.nextSummary)return;
    socket.nextSummary=now+std::chrono::seconds(5);
    for(auto& [address,value]:socket.traffic)
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: trafico %08x %s envia %u paq/%u B, recibe %u paq/%u B",handle,address.c_str(),value.sentPackets,value.sentBytes,value.receivedPackets,value.receivedBytes);
    socket.traffic.clear();
}
bool hardError(ssize_t result){return result<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK;}
bool localName(const std::string& name){return name==localHostName||name=="localhost";}
std::vector<in_addr> interfaceAddresses(bool includeLoopback){
    std::vector<in_addr> found;ifaddrs* list=nullptr;
    if(getifaddrs(&list)==0){
        for(auto* entry=list;entry;entry=entry->ifa_next)
            if(entry->ifa_addr&&entry->ifa_addr->sa_family==AF_INET&&(entry->ifa_flags&IFF_UP)&&(includeLoopback||!(entry->ifa_flags&IFF_LOOPBACK)))
                found.push_back(reinterpret_cast<sockaddr_in*>(entry->ifa_addr)->sin_addr);
        freeifaddrs(list);
    }
    if(found.empty()){in_addr loopback{};loopback.s_addr=htonl(INADDR_LOOPBACK);found.push_back(loopback);}
    return found;
}
bool endsWith(const std::string& value,const std::string& suffix){
    return value.size()>=suffix.size()&&value.compare(value.size()-suffix.size(),suffix.size(),suffix)==0;
}
}  // namespace net

// WSAStartup(version, WSADATA*): returns the error directly.
void wsaStartup(){
    uint32_t requested=arg(0)&0xffff,data=arg(1);
    if(!data){ret(net::wsaEfault,2);return;}
    std::memset(ptr(data,400),0,400);
    uint16_t version=uint16_t((requested&0xff)>=2?0x0202:requested),high=0x0202;
    std::memcpy(ptr(data,2),&version,2);std::memcpy(ptr(data+2,2),&high,2);
    std::strcpy(static_cast<char*>(ptr(data+4,32)),"WinSock 2.0 (Android)");
    std::strcpy(static_cast<char*>(ptr(data+261,8)),"Running");
    uint16_t maxSockets=1024,maxDatagram=65467;std::memcpy(ptr(data+390,2),&maxSockets,2);std::memcpy(ptr(data+392,2),&maxDatagram,2);
    ++net::startups;
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: WSAStartup %04x; EA Nation hosts resolve via %s",requested,onlineServer.c_str());
    ret(0,2);
}
void wsaCleanup(){
    if(!net::startups){lastError=net::wsaNotinitialised;ret(net::socketError,0);return;}
    if(!--net::startups){for(auto& entry:net::sockets)::close(entry.second.fd);net::sockets.clear();}
    ret(0,0);
}
void wsaGetLastError(){ret(lastError,0);}
void wsaSetLastError(){lastError=arg(0);ret(0,1);}
void socketCreate(){
    uint32_t family=arg(0),type=arg(1),protocol=arg(2);
    if(family==AF_INET&&type==3&&protocol==1){  // SOCK_RAW + IPPROTO_ICMP: ping
        int fd=::socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,IPPROTO_ICMP);
        if(fd<0){
            NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Winsock: ping ICMP no permitido por Android: %s",std::strerror(errno));
            lastError=10013;ret(net::invalidSocket,3);return;  // WSAEACCES, as for a non-admin raw socket on Windows
        }
        uint32_t handle=net::newHandle(fd,false);net::sockets[handle].icmp=true;
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: socket %08x ICMP (ping)",handle);
        ret(handle,3);return;
    }
    if(family!=AF_INET||(type!=SOCK_STREAM&&type!=SOCK_DGRAM))
        NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Winsock: socket(af=%u,type=%u,protocol=%u) no soportado desde %08x",family,type,protocol,read32(g_esp));
    if(family!=AF_INET){lastError=net::wsaEafnosupport;ret(net::invalidSocket,3);return;}
    if(type!=SOCK_STREAM&&type!=SOCK_DGRAM){lastError=10044;ret(net::invalidSocket,3);return;}  // WSAESOCKTNOSUPPORT
    int fd=::socket(AF_INET,int(type)|SOCK_NONBLOCK|SOCK_CLOEXEC,int(protocol));
    if(fd<0){
        NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Winsock: socket(af=%u,type=%u,protocol=%u) fallo: %s",family,type,protocol,std::strerror(errno));
        lastError=net::wsaError(errno);ret(net::invalidSocket,3);return;
    }
    uint32_t handle=net::newHandle(fd,false);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: socket %08x %s from %08x",handle,type==SOCK_STREAM?"TCP":"UDP",read32(g_esp));
    ret(handle,3);
}
void socketClose(){
    auto found=net::sockets.find(arg(0));
    if(found==net::sockets.end()){ret(net::fail(net::wsaEnotsock),1);return;}
    ::close(found->second.fd);net::sockets.erase(found);ret(0,1);
}
void socketBind(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),3);return;}
    sockaddr_storage address;socklen_t length=net::guestAddress(arg(1),arg(2),address);
    if(!length){ret(net::fail(net::wsaEfault),3);return;}
    // A raw ICMP socket ignores the port; on a ping socket it is the echo id and low ones need privileges.
    if(socket->icmp&&address.ss_family==AF_INET)reinterpret_cast<sockaddr_in*>(&address)->sin_port=0;
    int status=::bind(socket->fd,reinterpret_cast<sockaddr*>(&address),length);int error=errno;
    NFS_RUNTIME_LOG(status?ANDROID_LOG_WARN:ANDROID_LOG_INFO,"NFSU2","Winsock: bind %08x %s%s%s",arg(0),net::describe(address).c_str(),status?" fallo: ":"",status?std::strerror(error):"");
    errno=error;ret(status?net::failErrno():0,3);
}
void socketListen(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),2);return;}
    ret(::listen(socket->fd,int(arg(1)))?net::failErrno():0,2);
}
void socketAccept(){
    uint32_t handle=arg(0),address=arg(1),lengthPointer=arg(2);
    auto* socket=net::find(handle);if(!socket){lastError=net::wsaEnotsock;ret(net::invalidSocket,3);return;}
    sockaddr_storage peer{};socklen_t length=sizeof peer;
    int fd=int(net::blocking(*socket,POLLIN,0,[&](int listener){length=sizeof peer;return ssize_t(::accept4(listener,reinterpret_cast<sockaddr*>(&peer),&length,SOCK_NONBLOCK|SOCK_CLOEXEC));}));
    if(fd<0){lastError=net::wsaError(errno);ret(net::invalidSocket,3);return;}
    if(!net::storeAddress(address,lengthPointer,peer,std::min<socklen_t>(length,16))){::close(fd);lastError=net::wsaEfault;ret(net::invalidSocket,3);return;}
    bool inherit=net::find(handle)?net::find(handle)->guestNonBlocking:false;
    ret(net::newHandle(fd,inherit),3);
}
void socketConnect(){
    uint32_t handle=arg(0);
    auto* socket=net::find(handle);if(!socket){ret(net::fail(net::wsaEnotsock),3);return;}
    sockaddr_storage address;socklen_t length=net::guestAddress(arg(1),arg(2),address);
    if(!length){ret(net::fail(net::wsaEfault),3);return;}
    auto* in=reinterpret_cast<sockaddr_in*>(&address);char text[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&in->sin_addr,text,sizeof text);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: connect %08x to %s:%u",handle,text,ntohs(in->sin_port));
    if(::connect(socket->fd,reinterpret_cast<sockaddr*>(&address),length)==0){ret(0,3);return;}
    if(errno!=EINPROGRESS){ret(net::failErrno(),3);return;}
    if(socket->guestNonBlocking){socket->connecting=true;ret(net::fail(net::wsaEwouldblock),3);return;}
    int fd=socket->fd;net::wait(fd,POLLOUT,0);
    int error=0;socklen_t size=sizeof error;
    if(::getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size))error=errno;
    ret(error?net::fail(net::wsaError(error)):0,3);
}
void socketShutdown(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),2);return;}
    ret(::shutdown(socket->fd,int(arg(1)))?net::failErrno():0,2);
}
void socketSend(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),4);return;}
    uint32_t buffer=arg(1),length=arg(2),flags=arg(3);
    const void* data=length?ptr(buffer,length):nullptr;
    net::noteIcmpSend(*socket,data,length);
    ssize_t sent=net::blocking(*socket,POLLOUT,socket->sendTimeout,[&](int fd){return ::send(fd,data,length,int(flags&7)|MSG_NOSIGNAL);});
    ret(sent<0?net::failErrno():uint32_t(sent),4);
}
void socketSendTo(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),6);return;}
    uint32_t buffer=arg(1),length=arg(2),flags=arg(3);
    sockaddr_storage address;socklen_t addressLength=0;
    if(arg(4)){addressLength=net::guestAddress(arg(4),arg(5),address);if(!addressLength){ret(net::fail(net::wsaEfault),6);return;}}
    const void* data=length?ptr(buffer,length):nullptr;
    net::noteIcmpSend(*socket,data,length);
    ssize_t sent=net::blocking(*socket,POLLOUT,socket->sendTimeout,[&](int fd){
        return ::sendto(fd,data,length,int(flags&7)|MSG_NOSIGNAL,addressLength?reinterpret_cast<sockaddr*>(&address):nullptr,addressLength);});
    socket=net::find(arg(0));  // the call may have waited with the machine released
    if(socket){
        int error=errno;
        if(net::hardError(sent))NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Winsock: sendto %08x -> %s fallo: %s",arg(0),addressLength?net::describe(address).c_str():"(conectado)",std::strerror(error));
        else if(sent>=0&&addressLength){net::notePeer(*socket,"envia a",net::describe(address),sent);net::count(arg(0),*socket,net::describe(address),true,sent);}
        errno=error;
    }
    ret(sent<0?net::failErrno():uint32_t(sent),6);
}
void socketRecvFrom();
void socketRecv(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),4);return;}
    uint32_t buffer=arg(1),length=arg(2),flags=arg(3);
    if(socket->icmp){  // recv == recvfrom without the source address
        uint32_t returnAddress=read32(g_esp),handle=arg(0);
        g_esp-=8;write32(g_esp,returnAddress);write32(g_esp+4,handle);write32(g_esp+8,buffer);write32(g_esp+12,length);write32(g_esp+16,flags);
        write32(g_esp+20,0);write32(g_esp+24,0);socketRecvFrom();return;
    }
    void* data=length?ptr(buffer,length):nullptr;
    ssize_t received=net::blocking(*socket,POLLIN,socket->recvTimeout,[&](int fd){return ::recv(fd,data,length,int(flags&3));});
    ret(received<0?net::failErrno():uint32_t(received),4);
}
void socketRecvFrom(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),6);return;}
    uint32_t buffer=arg(1),length=arg(2),flags=arg(3),address=arg(4),lengthPointer=arg(5);
    void* data=length?ptr(buffer,length):nullptr;
    sockaddr_storage peer{};socklen_t peerLength=sizeof peer;
    if(socket->icmp){
        std::vector<uint8_t> message(65536);
        ssize_t received=net::blocking(*socket,POLLIN,socket->recvTimeout,[&](int fd){
            peerLength=sizeof peer;return ::recvfrom(fd,message.data(),message.size(),int(flags&2),reinterpret_cast<sockaddr*>(&peer),&peerLength);});
        if(received<0){ret(net::failErrno(),6);return;}
        message.resize(size_t(received));
        if(auto* current=net::find(arg(0))){net::notePeer(*current,"ping: respuesta de",net::describe(peer),received);net::count(arg(0),*current,net::describe(peer),false,received);}
        auto packet=net::icmpPacket(*net::find(arg(0)),std::move(message),*reinterpret_cast<sockaddr_in*>(&peer));
        if(peerLength&&!net::storeAddress(address,lengthPointer,peer,std::min<socklen_t>(peerLength,16))){ret(net::fail(net::wsaEfault),6);return;}
        if(length)std::memcpy(data,packet.data(),std::min<size_t>(length,packet.size()));
        if(packet.size()>length){ret(net::fail(net::wsaEmsgsize),6);return;}
        ret(uint32_t(packet.size()),6);return;
    }
    // MSG_TRUNC reports the full datagram size: Windows fails a short buffer with WSAEMSGSIZE.
    ssize_t received=net::blocking(*socket,POLLIN,socket->recvTimeout,[&](int fd){
        peerLength=sizeof peer;return ::recvfrom(fd,data,length,int(flags&3)|MSG_TRUNC,reinterpret_cast<sockaddr*>(&peer),&peerLength);});
    if(net::hardError(received)){int error=errno;NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Winsock: recvfrom %08x fallo: %s",arg(0),std::strerror(error));errno=error;}
    if(received<0){ret(net::failErrno(),6);return;}
    socket=net::find(arg(0));
    if(socket&&peerLength){net::notePeer(*socket,"recibe de",net::describe(peer),received);net::count(arg(0),*socket,net::describe(peer),false,received);}
    if(peerLength&&!net::storeAddress(address,lengthPointer,peer,std::min<socklen_t>(peerLength,16))){ret(net::fail(net::wsaEfault),6);return;}
    if(uint32_t(received)>length){ret(net::fail(net::wsaEmsgsize),6);return;}
    ret(uint32_t(received),6);
}
void socketName(bool peer){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),3);return;}
    sockaddr_storage address{};socklen_t length=sizeof address;
    int status=peer ? ::getpeername(socket->fd,reinterpret_cast<sockaddr*>(&address),&length) : ::getsockname(socket->fd,reinterpret_cast<sockaddr*>(&address),&length);
    if(status){ret(net::failErrno(),3);return;}
    ret(net::storeAddress(arg(1),arg(2),address,std::min<socklen_t>(length,16))?0:net::fail(net::wsaEfault),3);
}
void socketPeerName(){socketName(true);}
void socketLocalName(){socketName(false);}
void socketIoctl(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),3);return;}
    uint32_t command=arg(1),value=arg(2);
    if(!value){ret(net::fail(net::wsaEfault),3);return;}
    switch(command){
    case 0x8004667eu:socket->guestNonBlocking=read32(value)!=0;ret(0,3);return;  // FIONBIO
    case 0x4004667fu:{int pending=0;if(::ioctl(socket->fd,FIONREAD,&pending)){ret(net::failErrno(),3);return;}write32(value,uint32_t(pending));ret(0,3);return;}  // FIONREAD
    default:throw GuestStop("ioctlsocket comando pendiente: "+hex(command)+" desde "+hex(g_cur_func));
    }
}
void socketOption(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),5);return;}
    uint32_t level=arg(1),name=arg(2),value=arg(3),length=arg(4);
    if(!value||int32_t(length)<1){ret(net::fail(net::wsaEfault),5);return;}
    auto integer=[&]{return length>=4?int(read32(value)):int(*static_cast<uint8_t*>(ptr(value,1)));};
    auto set=[&](int hostLevel,int hostName,int option){return ::setsockopt(socket->fd,hostLevel,hostName,&option,sizeof option)?net::failErrno():0u;};
    uint32_t result;
    if(level==0xffffu){  // SOL_SOCKET
        switch(name){
        case 0x0004:result=set(SOL_SOCKET,SO_REUSEADDR,integer());break;
        case 0x0008:result=set(SOL_SOCKET,SO_KEEPALIVE,integer());break;
        case 0x0020:result=set(SOL_SOCKET,SO_BROADCAST,integer());break;
        case 0x0100:result=set(SOL_SOCKET,SO_OOBINLINE,integer());break;
        case 0x1001:result=set(SOL_SOCKET,SO_SNDBUF,integer());break;
        case 0x1002:result=set(SOL_SOCKET,SO_RCVBUF,integer());break;
        case 0x1005:socket->sendTimeout=uint32_t(integer());result=0;break;  // SO_SNDTIMEO (ms)
        case 0x1006:socket->recvTimeout=uint32_t(integer());result=0;break;  // SO_RCVTIMEO (ms)
        case 0x0080:case 0xff7fu:{  // SO_LINGER {u_short onoff, linger} / SO_DONTLINGER
            linger option{};
            if(name==0x0080){if(length<4){result=net::fail(net::wsaEfault);break;}uint16_t parts[2];std::memcpy(parts,ptr(value,4),4);option.l_onoff=parts[0];option.l_linger=parts[1];}
            else option.l_onoff=integer()?0:1;
            result=::setsockopt(socket->fd,SOL_SOCKET,SO_LINGER,&option,sizeof option)?net::failErrno():0;break;
        }
        default:throw GuestStop("setsockopt SOL_SOCKET opcion pendiente: "+hex(name)+" desde "+hex(g_cur_func));
        }
    }else if(level==IPPROTO_TCP&&name==1)result=set(IPPROTO_TCP,TCP_NODELAY,integer());
    else if(level==IPPROTO_IP&&name==4)result=set(IPPROTO_IP,IP_TTL,integer());
    else throw GuestStop("setsockopt nivel "+hex(level)+" opcion "+hex(name)+" pendiente desde "+hex(g_cur_func));
    ret(result,5);
}
// getsockopt for the options a Winsock program reads back.
void socketGetOption(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),5);return;}
    uint32_t level=arg(1),name=arg(2),value=arg(3),lengthPointer=arg(4);
    if(!value||!lengthPointer||int32_t(read32(lengthPointer))<4){ret(net::fail(net::wsaEfault),5);return;}
    int option=0;socklen_t size=sizeof option;int status=0;
    if(level==0xffffu){
        switch(name){
        case 0x1007:status=::getsockopt(socket->fd,SOL_SOCKET,SO_ERROR,&option,&size);if(!status)option=option?int(net::wsaError(option)):0;break;
        case 0x1008:status=::getsockopt(socket->fd,SOL_SOCKET,SO_TYPE,&option,&size);break;  // SOCK_STREAM/DGRAM match
        case 0x1001:status=::getsockopt(socket->fd,SOL_SOCKET,SO_SNDBUF,&option,&size);break;
        case 0x1002:status=::getsockopt(socket->fd,SOL_SOCKET,SO_RCVBUF,&option,&size);break;
        case 0x0004:status=::getsockopt(socket->fd,SOL_SOCKET,SO_REUSEADDR,&option,&size);break;
        case 0x0020:status=::getsockopt(socket->fd,SOL_SOCKET,SO_BROADCAST,&option,&size);break;
        case 0x1005:option=int(socket->sendTimeout);break;
        case 0x1006:option=int(socket->recvTimeout);break;
        default:throw GuestStop("getsockopt SOL_SOCKET opcion pendiente: "+hex(name)+" desde "+hex(g_cur_func));
        }
    }else if(level==IPPROTO_TCP&&name==1)status=::getsockopt(socket->fd,IPPROTO_TCP,TCP_NODELAY,&option,&size);
    else throw GuestStop("getsockopt nivel "+hex(level)+" opcion "+hex(name)+" pendiente desde "+hex(g_cur_func));
    if(status){ret(net::failErrno(),5);return;}
    write32(value,uint32_t(option));write32(lengthPointer,4);ret(0,5);
}
// Byte order helpers (x86 hosts are little-endian, network order is big-endian).
void hostToNetLong(){ret(__builtin_bswap32(arg(0)),1);}
void hostToNetShort(){ret(__builtin_bswap16(uint16_t(arg(0))),1);}
// select over Windows fd_set {u_int count; SOCKET array[64]}.
void socketSelect(){
    uint32_t sets[3]={arg(1),arg(2),arg(3)},timeoutPointer=arg(4);
    static constexpr short wanted[3]={POLLIN,POLLOUT,POLLPRI};
    struct Entry{uint32_t handle;int fd;unsigned set;};
    std::vector<Entry> entries;std::vector<pollfd> polls;
    for(unsigned set=0;set<3;++set){
        if(!sets[set])continue;
        uint32_t count=read32(sets[set]);if(count>64){ret(net::fail(net::wsaEinval),5);return;}
        for(uint32_t i=0;i<count;++i){
            uint32_t handle=read32(sets[set]+4+i*4);auto* socket=net::find(handle);
            if(!socket){ret(net::fail(net::wsaEnotsock),5);return;}
            entries.push_back({handle,socket->fd,set});polls.push_back({socket->fd,wanted[set],0});
        }
    }
    if(entries.empty()){ret(net::fail(net::wsaEinval),5);return;}
    bool forever=!timeoutPointer;
    uint64_t timeout=forever?0:uint64_t(read32(timeoutPointer))*1000u+(read32(timeoutPointer+4)+999u)/1000u;
    int ready=::poll(polls.data(),nfds_t(polls.size()),0);
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    WaitScope scope("select",uint32_t(timeout));
    while(ready==0){
        auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(!forever&&left<=0)break;
        checkGuestProgress();
        int slice=forever?50:int(std::min<long long>(left,50));
        ready=net::released([&]{return ::poll(polls.data(),nfds_t(polls.size()),slice);});
        if(ready<0&&errno==EINTR)ready=0;
    }
    if(ready<0){ret(net::failErrno(),5);return;}
    std::vector<uint32_t> hits[3];
    for(size_t i=0;i<entries.size();++i){
        auto& entry=entries[i];short events=polls[i].revents;auto* socket=net::find(entry.handle);
        bool hit=false;
        if(entry.set==0)hit=events&(POLLIN|POLLHUP|POLLERR);
        else if(socket&&socket->connecting&&(events&(POLLOUT|POLLERR|POLLHUP))){
            // A pending non-blocking connect is writable on success, "except" on failure.
            int error=0;socklen_t size=sizeof error;::getsockopt(entry.fd,SOL_SOCKET,SO_ERROR,&error,&size);
            hit=entry.set==1?!error:error!=0;
            if(hit)socket->connecting=false;
        }
        else if(entry.set==1)hit=events&POLLOUT;
        else hit=events&POLLPRI;
        if(hit&&std::find(hits[entry.set].begin(),hits[entry.set].end(),entry.handle)==hits[entry.set].end())hits[entry.set].push_back(entry.handle);
    }
    uint32_t total=0;
    for(unsigned set=0;set<3;++set){
        if(!sets[set])continue;
        write32(sets[set],uint32_t(hits[set].size()));
        for(size_t i=0;i<hits[set].size();++i)write32(sets[set]+4+uint32_t(i)*4,hits[set][i]);
        total+=uint32_t(hits[set].size());
    }
    ret(total,5);
}
// gethostbyname: one guest HOSTENT {name, aliases, short type, short length, addr_list}.
void hostByName(){
    std::string requested=guestString(arg(0)),name=lower(requested);
    std::vector<in_addr> addresses;
    if(net::localName(name))addresses=net::interfaceAddresses(name=="localhost");
    else{
        std::string lookup=name;
        if(name=="ea.com"||net::endsWith(name,".ea.com"))lookup=onlineServer;
        addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
        addrinfo* result=nullptr;
        int status=net::released([&]{return ::getaddrinfo(lookup.c_str(),nullptr,&hints,&result);});
        if(status){
            NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Winsock: %s -> %s no resuelve (%s)",requested.c_str(),lookup.c_str(),gai_strerror(status));
            lastError=status==EAI_AGAIN?11002:status==EAI_NONAME?net::wsaHostNotFound:net::wsaNoData;ret(0,1);return;
        }
        for(auto* entry=result;entry&&addresses.size()<8;entry=entry->ai_next){
            in_addr value=reinterpret_cast<sockaddr_in*>(entry->ai_addr)->sin_addr;
            if(std::none_of(addresses.begin(),addresses.end(),[&](in_addr a){return a.s_addr==value.s_addr;}))addresses.push_back(value);
        }
        freeaddrinfo(result);
        char text[INET_ADDRSTRLEN]{};if(!addresses.empty())inet_ntop(AF_INET,&addresses[0],text,sizeof text);
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: gethostbyname %s%s%s -> %s",requested.c_str(),lookup!=name?" via ":"",lookup!=name?lookup.c_str():"",text);
    }
    if(addresses.empty()){lastError=net::wsaNoData;ret(0,1);return;}
    if(addresses.size()>8)addresses.resize(8);
    // Layout: hostent(16) aliases[1](4) list[9](36) addrs[8](32) name(256).
    if(!net::hostent)net::hostent=allocate(344);
    uint32_t base=net::hostent,aliases=base+16,list=aliases+4,data=list+36,text=data+32;
    std::memset(ptr(base,344),0,344);
    std::snprintf(static_cast<char*>(ptr(text,256)),256,"%s",requested.c_str());
    write32(base,text);write32(base+4,aliases);
    uint16_t type=AF_INET,length=4;std::memcpy(ptr(base+8,2),&type,2);std::memcpy(ptr(base+10,2),&length,2);
    write32(base+12,list);
    for(size_t i=0;i<addresses.size();++i){std::memcpy(ptr(data+uint32_t(i)*4,4),&addresses[i],4);write32(list+uint32_t(i)*4,data+uint32_t(i)*4);}
    ret(base,1);
}
void hostName(){
    uint32_t buffer=arg(0),length=arg(1);
    if(!buffer||length<sizeof net::localHostName){ret(net::fail(net::wsaEfault),2);return;}
    std::memcpy(ptr(buffer,sizeof net::localHostName),net::localHostName,sizeof net::localHostName);ret(0,2);
}
// WSAIoctl: SIO_GET_INTERFACE_LIST fills INTERFACE_INFO {flags; addr, broadcast, mask as 24-byte sockaddr_gen}.
void wsaIoctl(){
    auto* socket=net::find(arg(0));if(!socket){ret(net::fail(net::wsaEnotsock),9);return;}
    uint32_t code=arg(1),output=arg(4),capacity=arg(5),returned=arg(6);
    if(arg(7)||arg(8))throw GuestStop("WSAIoctl solapado pendiente desde "+hex(g_cur_func));
    if(code==0xc8000014u){
        // SIO_ROUTING_INTERFACE_QUERY: the local address the host routes toward the given destination.
        sockaddr_storage target;socklen_t length=net::guestAddress(arg(2),arg(3),target);
        if(!length||target.ss_family!=AF_INET){ret(net::fail(net::wsaEfault),9);return;}
        if(reinterpret_cast<sockaddr_in*>(&target)->sin_port==0)reinterpret_cast<sockaddr_in*>(&target)->sin_port=htons(9);
        int probe=::socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0);
        if(probe<0){ret(net::failErrno(),9);return;}
        sockaddr_in local{};socklen_t localLength=sizeof local;
        bool ok=::connect(probe,reinterpret_cast<sockaddr*>(&target),length)==0&&::getsockname(probe,reinterpret_cast<sockaddr*>(&local),&localLength)==0;
        int error=errno;::close(probe);
        if(!ok){ret(net::fail(net::wsaError(error)),9);return;}
        local.sin_port=0;
        if(returned)write32(returned,16);
        if(!output||capacity<16){ret(net::fail(net::wsaEfault),9);return;}
        std::memcpy(ptr(output,16),&local,16);
        char text[INET_ADDRSTRLEN]{},to[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&local.sin_addr,text,sizeof text);
        inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in*>(&target)->sin_addr,to,sizeof to);
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Winsock: route toward %s uses local %s",to,text);
        ret(0,9);return;
    }
    if(code!=0x4004747fu)throw GuestStop("WSAIoctl codigo pendiente: "+hex(code)+" desde "+hex(g_cur_func));
    std::vector<std::array<uint8_t,76>> rows;ifaddrs* list=nullptr;
    if(getifaddrs(&list)==0){
        for(auto* entry=list;entry;entry=entry->ifa_next){
            if(!entry->ifa_addr||entry->ifa_addr->sa_family!=AF_INET||!(entry->ifa_flags&IFF_UP))continue;
            std::array<uint8_t,76> row{};
            uint32_t flags=1|(entry->ifa_flags&IFF_BROADCAST?2:0)|(entry->ifa_flags&IFF_LOOPBACK?4:0)|(entry->ifa_flags&IFF_POINTOPOINT?8:0)|(entry->ifa_flags&IFF_MULTICAST?16:0);
            std::memcpy(row.data(),&flags,4);
            auto put=[&](size_t offset,const sockaddr* address){if(address&&address->sa_family==AF_INET)std::memcpy(row.data()+offset,address,16);};
            put(4,entry->ifa_addr);put(28,(entry->ifa_flags&IFF_BROADCAST)?entry->ifa_broadaddr:nullptr);put(52,entry->ifa_netmask);
            rows.push_back(row);
        }
        freeifaddrs(list);
    }
    uint32_t bytes=uint32_t(rows.size()*76);
    if(returned)write32(returned,bytes);
    if(!output||capacity<bytes){ret(net::fail(net::wsaEfault),9);return;}
    for(size_t i=0;i<rows.size();++i)std::memcpy(ptr(output+uint32_t(i)*76,76),rows[i].data(),76);
    ret(0,9);
}
// Netbios(NCB*): only adapter enumeration/status (games read the MAC as a machine id).
// Android hides the real MAC, so the status reports a stable locally-administered one.
void netbios(){
    uint32_t ncb=arg(0);auto* block=static_cast<uint8_t*>(ptr(ncb,64));
    uint8_t command=block[0]&0x7f,status=0;uint32_t buffer=read32(ncb+4);uint16_t length;std::memcpy(&length,block+8,2);
    switch(command){
    case 0x37:  // NCBENUM: LANA_ENUM {length, lana[255]}
        if(!buffer||length<2){status=1;break;}
        static_cast<uint8_t*>(ptr(buffer,2))[0]=1;static_cast<uint8_t*>(ptr(buffer,2))[1]=0;break;
    case 0x32:break;  // NCBRESET
    case 0x33:{  // NCBASTAT: ADAPTER_STATUS starts with the 6-byte address
        if(!buffer||length<6){status=1;break;}
        uint32_t seed=0x811c9dc5u;for(char c:hostGameRoot)seed=(seed^uint8_t(c))*0x01000193u;
        uint8_t mac[6]={0x02,0x4e,uint8_t(seed),uint8_t(seed>>8),uint8_t(seed>>16),uint8_t(seed>>24)};
        std::memset(ptr(buffer,length),0,length);std::memcpy(ptr(buffer,6),mac,6);
        uint16_t written=std::min<uint16_t>(length,60);std::memcpy(block+8,&written,2);break;
    }
    default:throw GuestStop("Netbios comando pendiente: "+hex(command)+" desde "+hex(g_cur_func));
    }
    block[1]=status;block[49]=status;ret(status,1);
}
// TAPI: no modems on Android. lineInitialize fails, so modem play is never offered.
void lineInitialize(){NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","TAPI: lineInitialize -> LINEERR_NODRIVER");ret(0x80000043u,5);}
void lineShutdown(){ret(0x80000034u,1);}  // LINEERR_INVALAPPHANDLE: nothing was initialized
const std::map<std::string,recomp_func_t> networkFunctions={
    {"#1",socketAccept},{"#2",socketBind},{"#3",socketClose},{"#4",socketConnect},{"#5",socketPeerName},{"#6",socketLocalName},
    {"#7",socketGetOption},{"#8",hostToNetLong},{"#9",hostToNetShort},{"#14",hostToNetLong},{"#15",hostToNetShort},
    {"getsockopt",socketGetOption},{"htonl",hostToNetLong},{"htons",hostToNetShort},{"ntohl",hostToNetLong},{"ntohs",hostToNetShort},
    {"#10",socketIoctl},{"#13",socketListen},{"#16",socketRecv},{"#17",socketRecvFrom},{"#18",socketSelect},{"#19",socketSend},
    {"#20",socketSendTo},{"#21",socketOption},{"#22",socketShutdown},{"#23",socketCreate},{"#52",hostByName},{"#57",hostName},
    {"#111",wsaGetLastError},{"#112",wsaSetLastError},{"#115",wsaStartup},{"#116",wsaCleanup},{"WSAIoctl",wsaIoctl},
    {"accept",socketAccept},{"bind",socketBind},{"closesocket",socketClose},{"connect",socketConnect},{"getpeername",socketPeerName},
    {"getsockname",socketLocalName},{"ioctlsocket",socketIoctl},{"listen",socketListen},{"recv",socketRecv},{"recvfrom",socketRecvFrom},
    {"select",socketSelect},{"send",socketSend},{"sendto",socketSendTo},{"setsockopt",socketOption},{"shutdown",socketShutdown},
    {"socket",socketCreate},{"gethostbyname",hostByName},{"gethostname",hostName},{"WSAGetLastError",wsaGetLastError},
    {"WSASetLastError",wsaSetLastError},{"WSAStartup",wsaStartup},{"WSACleanup",wsaCleanup},
};
