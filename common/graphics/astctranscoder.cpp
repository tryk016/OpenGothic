#include "astctranscoder.h"

#if defined(HAS_ASTCENC)

#include <Tempest/Log>

#include <astcenc.h>

#include <zenkit/Stream.hh>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#include "gothic.h"
#include "resources.h"

using namespace Tempest;

namespace {

constexpr std::array<uint8_t,8> CacheMagic = {'O','G','A','S','T','C','2',0};
constexpr uint32_t CacheVersion   = 2;
constexpr uint32_t HeaderSize     = 96;
constexpr uint32_t EncoderVersion = 0x00050600; // astcenc 5.6.0, pinned submodule
constexpr uint32_t BlockWidth     = 4;
constexpr uint32_t BlockHeight    = 4;
constexpr uint32_t ProfileLdr     = 1;
constexpr uint32_t PresetFast     = 1;
constexpr uint64_t MaxPayload     = 256ull*1024ull*1024ull;
constexpr uint64_t MaxCacheBytes  = 512ull*1024ull*1024ull;
constexpr uint64_t MaxWorkingSet  = 192ull*1024ull*1024ull;
constexpr uint32_t MaxDimension   = 16384;
constexpr uint64_t FnvOffset      = 14695981039346656037ull;
constexpr uint64_t FnvPrime       = 1099511628211ull;

struct CacheHeader final {
  uint64_t sourceSize = 0;
  uint64_t sourceHash = 0;
  uint64_t nameHash = 0;
  uint64_t payloadSize = 0;
  uint64_t payloadHash = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t mipCount = 0;
  uint32_t sourceFormat = 0;
  };

struct Stats final {
  std::atomic<uint32_t> hits{0};
  std::atomic<uint32_t> encoded{0};
  std::atomic<uint32_t> failed{0};
  std::atomic<uint32_t> invalid{0};
  std::atomic<uint64_t> astcBytes{0};
  std::atomic<uint64_t> rgbaBytes{0};
  std::atomic<uint64_t> texels{0};
  std::atomic<uint64_t> encodeMs{0};
  };

Stats stats;
std::mutex contextSync;
astcenc_context* encoderContext = nullptr;
bool contextInitialized = false;

uint64_t hashBytes(uint64_t hash, const void* ptr, size_t size) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(ptr);
  for(size_t i=0; i<size; ++i) {
    hash ^= bytes[i];
    hash *= FnvPrime;
    }
  return hash;
  }

uint64_t hashString(std::string_view text) {
  return hashBytes(FnvOffset,text.data(),text.size());
  }

void appendU32(std::array<uint8_t,HeaderSize>& dst, size_t& offset, uint32_t value) {
  for(uint32_t i=0; i<4; ++i)
    dst[offset++] = uint8_t(value>>(i*8));
  }

void appendU64(std::array<uint8_t,HeaderSize>& dst, size_t& offset, uint64_t value) {
  for(uint32_t i=0; i<8; ++i)
    dst[offset++] = uint8_t(value>>(i*8));
  }

bool readU32(const std::array<uint8_t,HeaderSize>& src, size_t& offset, uint32_t& value) {
  if(offset+4>src.size())
    return false;
  value = 0;
  for(uint32_t i=0; i<4; ++i)
    value |= uint32_t(src[offset++])<<(i*8);
  return true;
  }

bool readU64(const std::array<uint8_t,HeaderSize>& src, size_t& offset, uint64_t& value) {
  if(offset+8>src.size())
    return false;
  value = 0;
  for(uint32_t i=0; i<8; ++i)
    value |= uint64_t(src[offset++])<<(i*8);
  return true;
  }

std::array<uint8_t,HeaderSize> serialize(const CacheHeader& h) {
  std::array<uint8_t,HeaderSize> bytes = {};
  std::copy(CacheMagic.begin(),CacheMagic.end(),bytes.begin());
  size_t offset = CacheMagic.size();
  appendU32(bytes,offset,CacheVersion);
  appendU32(bytes,offset,HeaderSize);
  appendU64(bytes,offset,h.sourceSize);
  appendU64(bytes,offset,h.sourceHash);
  appendU64(bytes,offset,h.nameHash);
  appendU64(bytes,offset,h.payloadSize);
  appendU64(bytes,offset,h.payloadHash);
  appendU32(bytes,offset,h.width);
  appendU32(bytes,offset,h.height);
  appendU32(bytes,offset,h.mipCount);
  appendU32(bytes,offset,h.sourceFormat);
  appendU32(bytes,offset,BlockWidth);
  appendU32(bytes,offset,BlockHeight);
  appendU32(bytes,offset,ProfileLdr);
  appendU32(bytes,offset,PresetFast);
  appendU32(bytes,offset,EncoderVersion);
  appendU32(bytes,offset,0);
  return bytes;
  }

bool deserialize(const std::array<uint8_t,HeaderSize>& bytes, CacheHeader& h) {
  if(!std::equal(CacheMagic.begin(),CacheMagic.end(),bytes.begin()))
    return false;

  size_t offset = CacheMagic.size();
  uint32_t version = 0, headerSize = 0;
  uint32_t blockWidth = 0, blockHeight = 0, profile = 0, preset = 0;
  uint32_t encoderVersion = 0, reserved = 0;
  return readU32(bytes,offset,version) && version==CacheVersion &&
         readU32(bytes,offset,headerSize) && headerSize==HeaderSize &&
         readU64(bytes,offset,h.sourceSize) &&
         readU64(bytes,offset,h.sourceHash) &&
         readU64(bytes,offset,h.nameHash) &&
         readU64(bytes,offset,h.payloadSize) &&
         readU64(bytes,offset,h.payloadHash) &&
         readU32(bytes,offset,h.width) &&
         readU32(bytes,offset,h.height) &&
         readU32(bytes,offset,h.mipCount) &&
         readU32(bytes,offset,h.sourceFormat) &&
         readU32(bytes,offset,blockWidth) && blockWidth==BlockWidth &&
         readU32(bytes,offset,blockHeight) && blockHeight==BlockHeight &&
         readU32(bytes,offset,profile) && profile==ProfileLdr &&
         readU32(bytes,offset,preset) && preset==PresetFast &&
         readU32(bytes,offset,encoderVersion) && encoderVersion==EncoderVersion &&
         readU32(bytes,offset,reserved) && reserved==0 && offset==bytes.size();
  }

bool addChecked(uint64_t& value, uint64_t amount) {
  if(amount>std::numeric_limits<uint64_t>::max()-value)
    return false;
  value += amount;
  return true;
  }

uint64_t astcLevelSize(uint32_t width, uint32_t height) {
  const uint64_t x = uint64_t(width/4)+(width%4!=0);
  const uint64_t y = uint64_t(height/4)+(height%4!=0);
  if(x==0 || y==0 || x>std::numeric_limits<uint64_t>::max()/y/16u)
    return 0;
  return x*y*16u;
  }

uint32_t validMipCount(uint32_t width, uint32_t height, uint32_t requested) {
  if(width==0 || height==0 || width>MaxDimension || height>MaxDimension)
    return 0;
  uint32_t maximum = 1;
  for(uint32_t size=std::max(width,height); size>1; size/=2)
    ++maximum;
  return std::min(requested,maximum);
  }

uint64_t astcChainSize(uint32_t width, uint32_t height, uint32_t mipCount) {
  uint64_t total = 0;
  for(uint32_t mip=0; mip<mipCount; ++mip) {
    const uint64_t level = astcLevelSize(width,height);
    if(level==0 || !addChecked(total,level))
      return 0;
    width = std::max<uint32_t>(1,width/2);
    height = std::max<uint32_t>(1,height/2);
    }
  return total;
  }

uint64_t rgbaChainSize(uint32_t width, uint32_t height, uint32_t mipCount) {
  uint64_t total = 0;
  for(uint32_t mip=0; mip<mipCount; ++mip) {
    const uint64_t level = uint64_t(width)*uint64_t(height)*4u;
    if(!addChecked(total,level))
      return 0;
    width = std::max<uint32_t>(1,width/2);
    height = std::max<uint32_t>(1,height/2);
    }
  return total;
  }

const std::filesystem::path& cacheDirectory() {
  static const std::filesystem::path path = []() {
    auto configured = Gothic::settingsGetS("INTERNAL","astcCacheDir");
    std::filesystem::path value = configured.empty() ? "astc-v2" : std::string(configured);
    std::error_code error;
    std::filesystem::create_directories(value,error);
    if(error)
      Log::e("[astc] cannot create cache directory: ",error.message());
    return value;
    }();
  return path;
  }

std::filesystem::path cachePath(std::string_view name, AstcTranscoder::SourceFingerprint source) {
  std::array<char,64> file = {};
  std::snprintf(file.data(),file.size(),"%016llx-%016llx.astc2",
                static_cast<unsigned long long>(hashString(name)),
                static_cast<unsigned long long>(source.hash));
  return cacheDirectory()/file.data();
  }

void trimCache() {
  struct Entry final {
    std::filesystem::path path;
    std::filesystem::file_time_type time;
    uint64_t size = 0;
    };

  std::error_code error;
  std::vector<Entry> files;
  uint64_t total = 0;
  for(std::filesystem::directory_iterator it(cacheDirectory(),error), end; !error && it!=end; it.increment(error)) {
    if(!it->is_regular_file(error) || it->path().extension()!=".astc2")
      continue;
    const uint64_t size = it->file_size(error);
    if(error)
      break;
    files.push_back({it->path(),it->last_write_time(error),size});
    if(error || !addChecked(total,size))
      break;
    }
  if(error || total<=MaxCacheBytes)
    return;

  std::sort(files.begin(),files.end(),[](const Entry& a, const Entry& b) {
    return a.time<b.time;
    });
  for(const auto& file:files) {
    if(total<=MaxCacheBytes)
      break;
    std::filesystem::remove(file.path,error);
    if(!error)
      total -= std::min(total,file.size);
    error.clear();
    }
  }

enum class CacheRead : uint8_t { Miss, Hit, Invalid };

CacheRead readCache(const std::filesystem::path& path, std::string_view name,
                    AstcTranscoder::SourceFingerprint source,
                    zenkit::TextureFormat sourceFormat, uint32_t width,
                    uint32_t height, uint32_t mipCount, Pixmap& result) {
  FILE* file = std::fopen(path.string().c_str(),"rb");
  if(file==nullptr)
    return CacheRead::Miss;

  std::array<uint8_t,HeaderSize> bytes = {};
  CacheHeader header;
  bool valid = std::fread(bytes.data(),1,bytes.size(),file)==bytes.size() &&
               deserialize(bytes,header);
  const uint64_t expectedName = hashString(name);
  valid = valid && header.sourceSize==source.size && header.sourceHash==source.hash &&
          header.nameHash==expectedName &&
          header.sourceFormat==uint32_t(sourceFormat) &&
          header.width==width && header.height==height &&
          header.mipCount==mipCount &&
          header.payloadSize>0 && header.payloadSize<=MaxPayload &&
          header.payloadSize==astcChainSize(header.width,header.height,header.mipCount);

  if(valid) {
    if(std::fseek(file,0,SEEK_END)!=0)
      valid = false;
    const long fileSize = valid ? std::ftell(file) : -1;
    valid = valid && fileSize>=0 && uint64_t(fileSize)==HeaderSize+header.payloadSize &&
            std::fseek(file,long(HeaderSize),SEEK_SET)==0;
    }

  std::vector<uint8_t> payload;
  if(valid) {
    try {
      payload.resize(size_t(header.payloadSize));
      }
    catch(...) {
      std::fclose(file);
      return CacheRead::Miss;
      }
    valid = std::fread(payload.data(),1,payload.size(),file)==payload.size() &&
            hashBytes(FnvOffset,payload.data(),payload.size())==header.payloadHash;
    }
  std::fclose(file);
  if(!valid)
    return CacheRead::Invalid;

  try {
    result = Pixmap(payload.data(),payload.size(),header.width,header.height,
                    header.mipCount,TextureFormat::ASTC4x4);
    }
  catch(...) {
    return CacheRead::Invalid;
    }

  std::error_code error;
  std::filesystem::last_write_time(path,std::filesystem::file_time_type::clock::now(),error);
  return CacheRead::Hit;
  }

void writeCache(const std::filesystem::path& path, const CacheHeader& header,
                const std::vector<uint8_t>& payload) {
  const auto bytes = serialize(header);
  const std::filesystem::path temporary = path.string()+".tmp";
  std::error_code error;
  std::filesystem::remove(temporary,error);

  FILE* file = std::fopen(temporary.string().c_str(),"wb");
  if(file==nullptr)
    return;
  const bool ok = std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size() &&
                  std::fwrite(payload.data(),1,payload.size(),file)==payload.size() &&
                  std::fflush(file)==0;
  const bool closed = std::fclose(file)==0;
  if(!ok || !closed) {
    std::filesystem::remove(temporary,error);
    return;
    }

  std::filesystem::rename(temporary,path,error);
  if(error)
    std::filesystem::remove(temporary,error);
  }

astcenc_context* context() {
  if(contextInitialized)
    return encoderContext;
  contextInitialized = true;

  astcenc_config config = {};
  auto result = astcenc_config_init(ASTCENC_PRF_LDR,BlockWidth,BlockHeight,1,
                                    ASTCENC_PRE_FAST,ASTCENC_FLG_SELF_DECOMPRESS_ONLY,&config);
  if(result!=ASTCENC_SUCCESS) {
    Log::e("[astc] config: ",astcenc_get_error_string(result));
    return nullptr;
    }
  result = astcenc_context_alloc(&config,1,&encoderContext,nullptr);
  if(result!=ASTCENC_SUCCESS) {
    Log::e("[astc] context: ",astcenc_get_error_string(result));
    encoderContext = nullptr;
    }
  return encoderContext;
  }

TextureFormat nativeFormat(zenkit::TextureFormat format) {
  switch(format) {
    case zenkit::TextureFormat::DXT1: return TextureFormat::DXT1;
    case zenkit::TextureFormat::DXT3: return TextureFormat::DXT3;
    case zenkit::TextureFormat::DXT5: return TextureFormat::DXT5;
    default:                          return TextureFormat::Undefined;
    }
  }

} // namespace

AstcTranscoder::SourceFingerprint AstcTranscoder::fingerprint(zenkit::Read& input) {
  SourceFingerprint result;
  input.seek(0,zenkit::Whence::END);
  result.size = input.tell();
  input.seek(0,zenkit::Whence::BEG);

  std::array<uint8_t,64*1024> buffer = {};
  uint64_t remaining = result.size;
  uint64_t hash = FnvOffset;
  while(remaining>0) {
    const size_t request = size_t(std::min<uint64_t>(remaining,buffer.size()));
    const size_t read = input.read(buffer.data(),request);
    if(read==0 || read>request)
      break;
    hash = hashBytes(hash,buffer.data(),read);
    remaining -= read;
    }
  input.seek(0,zenkit::Whence::BEG);
  result.hash = hash;
  result.valid = remaining==0;
  return result;
  }

bool AstcTranscoder::enabled(zenkit::TextureFormat sourceFormat) {
  const TextureFormat bc = nativeFormat(sourceFormat);
  if(bc==TextureFormat::Undefined)
    return false;

  auto& properties = Resources::device().properties();
  const bool nativeBc = properties.hasSamplerFormat(bc);
  const bool astc = properties.hasSamplerFormat(TextureFormat::ASTC4x4);
  static std::atomic<bool> logged{false};
  if(!logged.exchange(true))
    Log::i("[astc] caps: source BC=",int(nativeBc)," ASTC4x4=",int(astc));
  return !nativeBc && astc;
  }

Pixmap AstcTranscoder::transcode(std::string_view name, const zenkit::Texture& texture,
                                 SourceFingerprint source) {
  if(!source.valid || source.size==0 || !enabled(texture.format()))
    return Pixmap();

  const uint32_t width = texture.width();
  const uint32_t height = texture.height();
  const uint32_t mipCount = validMipCount(width,height,texture.mipmaps());
  if(mipCount==0)
    return Pixmap();

  const uint64_t total = astcChainSize(width,height,mipCount);
  uint64_t workingSet = source.size;
  const uint64_t baseRgba = uint64_t(width)*uint64_t(height)*4u;
  if(total==0 || total>MaxPayload ||
     !addChecked(workingSet,total) || !addChecked(workingSet,total) ||
     !addChecked(workingSet,baseRgba) || workingSet>MaxWorkingSet) {
    stats.failed.fetch_add(1);
    return Pixmap();
    }

  static std::once_flag cleanup;
  std::call_once(cleanup,trimCache);
  const auto path = cachePath(name,source);
  Pixmap cached;
  const auto cache = readCache(path,name,source,texture.format(),width,height,
                               mipCount,cached);
  if(cache==CacheRead::Hit) {
    stats.hits.fetch_add(1);
    stats.astcBytes.fetch_add(cached.dataSize());
    stats.rgbaBytes.fetch_add(rgbaChainSize(width,height,mipCount));
    return cached;
    }
  if(cache==CacheRead::Invalid) {
    stats.invalid.fetch_add(1);
    std::error_code error;
    std::filesystem::remove(path,error);
    }

  std::lock_guard<std::mutex> guard(contextSync);
  astcenc_context* encoder = context();
  if(encoder==nullptr) {
    stats.failed.fetch_add(1);
    return Pixmap();
    }

  std::vector<uint8_t> output;
  try {
    output.resize(size_t(total));
    }
  catch(...) {
    stats.failed.fetch_add(1);
    return Pixmap();
    }
  const astcenc_swizzle swizzle = {ASTCENC_SWZ_R,ASTCENC_SWZ_G,ASTCENC_SWZ_B,ASTCENC_SWZ_A};

  const auto started = std::chrono::steady_clock::now();
  size_t offset = 0;
  uint64_t texels = 0;
  for(uint32_t mip=0; mip<mipCount; ++mip) {
    const uint32_t mipWidth = std::max<uint32_t>(1,texture.mipmap_width(mip));
    const uint32_t mipHeight = std::max<uint32_t>(1,texture.mipmap_height(mip));
    std::vector<uint8_t> rgba;
    try {
      rgba = texture.as_rgba8(mip);
      }
    catch(...) {
      stats.failed.fetch_add(1);
      return Pixmap();
      }
    const uint64_t rgbaSize = uint64_t(mipWidth)*uint64_t(mipHeight)*4u;
    if(rgbaSize>std::numeric_limits<size_t>::max() || rgba.size()!=size_t(rgbaSize)) {
      stats.failed.fetch_add(1);
      return Pixmap();
      }

    astcenc_image image = {};
    image.dim_x = mipWidth;
    image.dim_y = mipHeight;
    image.dim_z = 1;
    image.data_type = ASTCENC_TYPE_U8;
    void* slices[1] = {rgba.data()};
    image.data = slices;

    const size_t levelSize = size_t(astcLevelSize(mipWidth,mipHeight));
    const auto result = astcenc_compress_image(encoder,&image,&swizzle,
                                               output.data()+offset,levelSize,0);
    astcenc_compress_reset(encoder);
    if(result!=ASTCENC_SUCCESS) {
      Log::e("[astc] encode ",name,": ",astcenc_get_error_string(result));
      stats.failed.fetch_add(1);
      return Pixmap();
      }
    offset += levelSize;
    texels += uint64_t(mipWidth)*uint64_t(mipHeight);
    }

  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now()-started).count();
  CacheHeader header;
  header.sourceSize = source.size;
  header.sourceHash = source.hash;
  header.nameHash = hashString(name);
  header.payloadSize = output.size();
  header.payloadHash = hashBytes(FnvOffset,output.data(),output.size());
  header.width = width;
  header.height = height;
  header.mipCount = mipCount;
  header.sourceFormat = uint32_t(texture.format());
  writeCache(path,header,output);

  stats.encoded.fetch_add(1);
  stats.astcBytes.fetch_add(output.size());
  stats.rgbaBytes.fetch_add(rgbaChainSize(width,height,mipCount));
  stats.texels.fetch_add(texels);
  stats.encodeMs.fetch_add(uint64_t(elapsed));
  try {
    return Pixmap(output.data(),output.size(),width,height,mipCount,TextureFormat::ASTC4x4);
    }
  catch(...) {
    stats.failed.fetch_add(1);
    return Pixmap();
    }
  }

void AstcTranscoder::logStats() {
  const uint32_t hits = stats.hits.load();
  const uint32_t encoded = stats.encoded.load();
  const uint32_t failed = stats.failed.load();
  const uint32_t invalid = stats.invalid.load();
  if(hits==0 && encoded==0 && failed==0 && invalid==0)
    return;

  const uint64_t astc = stats.astcBytes.load();
  const uint64_t rgba = stats.rgbaBytes.load();
  Log::i("[astc] textures: encoded=",encoded," hits=",hits,
         " invalid=",invalid," failed=",failed);
  Log::i("[astc] resident bytes: astc=",int(astc>>20),"MiB rgba8=",int(rgba>>20),
         "MiB ratio=",(astc>0 ? double(rgba)/double(astc) : 0.0));
  Log::i("[astc] encoded ",double(stats.texels.load())/1000000.0," Mpx in ",
         double(stats.encodeMs.load())/1000.0," s");
  trimCache();
  }

#endif
