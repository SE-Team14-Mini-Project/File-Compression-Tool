// Test-only codec substitutes inject late failures. Production codec is not
// linked into this executable; controller/archive/file I/O/validation are real.
#include "interfaces.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace team14;
namespace {
enum class Fault { None, CodesThrow, EncodeError, EncodeThrow, EncodeWriteFailure, DecodeError, DecodeThrow, DecodeWriteFailure, DecodeShort, DecodeWrongCrc };
Fault fault = Fault::None;
fs::path destination;
bool visibilityViolation = false;
unsigned callbackVisits = 0, checks = 0;
void check(bool ok, const char* label) { ++checks; if (!ok) throw std::runtime_error(label); }
std::string load(const fs::path& path) { std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}}; }
void save(const fs::path& path,const std::string& bytes) {
    std::ofstream out(path,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();check(!out.fail(),"save fixture");
}
void observePublication() {
    ++callbackVisits;
    if (fs::exists(destination)) visibilityViolation=true;
    bool temporaryPresent=false;
    for(const auto& e:fs::directory_iterator(destination.parent_path())) {
        if(e.path().filename().string().find(".team14-tmp-")==0) temporaryPresent=true;
    }
    if(!temporaryPresent) visibilityViolation=true;
}
void noTemporaryFiles(const fs::path& parent) {
    for(const auto& e:fs::directory_iterator(parent)) {
        const auto name=e.path().filename().string();
        check(name.find(".team14-tmp-")!=0 && name.find(".team14-io-check-")!=0,"no leaked temporary output or access probe");
    }
}
std::string fixture(bool crc=true) {
    ArchiveMetadata m; m.originalBytes=m.encodedBits=10;m.frequencies['A']=10;
    if(crc){m.flags=kFlagCrc32Present;m.crc32=0x478ED0CFU;} // CRC32/IEEE of ten A bytes
    std::istringstream payload(std::string(2,'\0'),std::ios::binary);std::ostringstream out(std::ios::binary);
    check(writeArchive(out,m,payload).ok(),"build archive");return out.str();
}
}
namespace team14 {
FrequencyTable buildFrequencyTable(std::istream& in) { FrequencyTable f{};char c;while(in.get(c))++f[static_cast<unsigned char>(c)];return f; }
HuffmanTree buildTree(const FrequencyTable& f) { auto n=std::make_shared<HuffmanNode>();n->frequency=f['A'];n->symbol='A';n->minSymbol='A';return n; }
CodeTable generateCodes(const HuffmanTree&) {
    if(fault==Fault::CodesThrow) throw std::runtime_error("PRIVATE_EXCEPTION_CONTENT");
    CodeTable codes{};
    codes['A'] = {false};
    return codes;
}
Result encode(std::istream& in,const CodeTable&,std::ostream& out,EncodedStats& stats) {
    observePublication();char c;std::uint64_t count=0;while(in.get(c))++count;
    out.put('\0');
    if(fault==Fault::EncodeThrow)throw std::runtime_error("PRIVATE_EXCEPTION_CONTENT");
    if(fault==Fault::EncodeError)return {ErrorCode::IoError,"Injected encode failure."};
    for(std::uint64_t n=1;n<count/8+(count%8!=0);++n)out.put('\0');
    stats.meaningfulBits=count;stats.packedBytes=count/8+(count%8!=0);
    if(fault==Fault::EncodeWriteFailure) out.setstate(std::ios::badbit);
    return {};
}
Result decode(std::istream& in,const HuffmanTree&,std::uint64_t bits,std::uint64_t count,std::ostream& out) {
    observePublication();
    for(std::uint64_t n=0;n<bits/8+(bits%8!=0);++n)if(in.get()==std::char_traits<char>::eof())return {ErrorCode::InvalidArchive,"Truncated injected fixture."};
    if(fault==Fault::DecodeThrow){out<<"AAA";throw std::runtime_error("PRIVATE_EXCEPTION_CONTENT");}
    if(fault==Fault::DecodeError){out<<"AAA";return {ErrorCode::InvalidArchive,"Injected decode failure."};}
    if(fault==Fault::DecodeShort)--count;
    for(std::uint64_t n=0;n<count;++n)out.put(fault==Fault::DecodeWrongCrc?'B':'A');
    if(fault==Fault::DecodeWriteFailure) out.setstate(std::ios::badbit);
    return {};
}
}
int main() {
    const fs::path root=fs::current_path()/"publication-fault-fixtures";
    struct Cleanup {fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{root};
    try {
        fs::create_directory(root);const auto source=root/"source.bin";save(source,std::string(10,'A'));
        destination=root/"out.huf";
        for(auto mode:{Fault::CodesThrow,Fault::EncodeError,Fault::EncodeThrow,Fault::EncodeWriteFailure,Fault::None}) {
            fault=mode;visibilityViolation=false;callbackVisits=0;OperationStats stats{999,888};
            Result r;
            try {r=compressFile(source,destination,stats);} catch(...) {check(false,"compression exceptions must be converted to Result");}
            check(load(source)==std::string(10,'A'),"source preserved");check(!visibilityViolation,"archive stays unpublished during encoding");
            check(r.message.find("PRIVATE_EXCEPTION_CONTENT")==std::string::npos,"exception details not disclosed");
            if(mode==Fault::None){check(r.ok() && fs::exists(destination),"successful archive published");check(callbackVisits==1,"encoding callback observed");fs::remove(destination);}
            else {check(!r.ok() && !fs::exists(destination),"failed compression has no final output");check(stats.originalBytes==0 && stats.archiveBytes==0,"failed compression stats reset");}
            noTemporaryFiles(root);
        }
        const auto archive=root/"input.huf";const auto golden=fixture();save(archive,golden);destination=root/"restored.bin";
        for(auto mode:{Fault::DecodeError,Fault::DecodeThrow,Fault::DecodeWriteFailure,Fault::DecodeShort,Fault::DecodeWrongCrc,Fault::None}) {
            fault=mode;visibilityViolation=false;callbackVisits=0;OperationStats stats{999,888};Result r;
            try {r=decompressFile(archive,destination,stats);} catch(...) {check(false,"decompression exceptions must be converted to Result");}
            check(callbackVisits==1 && !visibilityViolation,"restored output stays unpublished during decoding");
            check(load(archive)==golden,"archive input preserved");check(r.message.find("PRIVATE_EXCEPTION_CONTENT")==std::string::npos,"decode exception details not disclosed");
            if(mode==Fault::None){check(r.ok() && load(destination)==std::string(10,'A'),"verified restored output published");fs::remove(destination);}
            else {check(!r.ok() && !fs::exists(destination),"failed decoding/size/CRC verification never publishes");check(stats.originalBytes==0 && stats.archiveBytes==0,"failed decode stats reset");}
            if(mode==Fault::DecodeShort || mode==Fault::DecodeWrongCrc) {
                check(r.code==ErrorCode::IntegrityFailure,"real size/CRC verifier rejects invalid restored bytes");
            }
            noTemporaryFiles(root);
        }
        fault=Fault::None;
        for(auto malformed:{golden+"extra",golden.substr(0,golden.size()-1)}) {
            save(archive,malformed);OperationStats stats;check(!decompressFile(archive,destination,stats).ok() && !fs::exists(destination),"truncated/trailing archives never publish");noTemporaryFiles(root);
        }
        std::cout<<"PASS: "<<checks<<" output-publication fault checks\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
