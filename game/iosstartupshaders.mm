#include "iosstartupshaders.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <TargetConditionals.h>

#include <opengothic_ios_startup_hashes.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using MetalApi = Tempest::MetalApi;
using Profile  = MetalApi::PrecompiledShaderProfile;

constexpr uint32_t Msl24 = 20400;
constexpr uint32_t Msl30 = 30000;
constexpr uint32_t Msl31 = 30100;
constexpr uint32_t MetalLanguage31 = (3u << 16u) | 1u;

const char* versionSuffix(uint32_t version) {
  switch(version) {
    case Msl24: return "20400";
    case Msl30: return "30000";
    case Msl31: return "30100";
    default:    return nullptr;
    }
  }

bool loadResource(std::string_view name, std::string_view extension,
                  std::vector<uint8_t>& data) {
  auto nsName = [NSString stringWithUTF8String:std::string(name).c_str()];
  auto nsExt  = [NSString stringWithUTF8String:std::string(extension).c_str()];
  if(nsName==nil || nsExt==nil)
    return false;
  auto path = [[NSBundle mainBundle] pathForResource:nsName ofType:nsExt];
  if(path==nil)
    return false;
  auto source = [NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:nil];
  if(source==nil || source.length==0 || source.bytes==nullptr)
    return false;
  data.resize(size_t(source.length));
  std::memcpy(data.data(),source.bytes,data.size());
  return true;
  }

bool currentProfile(id<MTLDevice> device, Profile& profile) {
  if(device==nil)
    return false;

  auto compileOptions = [[MTLCompileOptions alloc] init];
  if(compileOptions==nil)
    return false;
  uint32_t language = uint32_t(compileOptions.languageVersion);
  [compileOptions release];
  if(language>MetalLanguage31)
    language = MetalLanguage31;

  const uint32_t major = language >> 16u;
  const uint32_t minor = language & 0xFFFFu;
  profile.mslVersion = major*10000u + minor*100u;
  if(versionSuffix(profile.mslVersion)==nullptr)
    return false;

#if TARGET_OS_SIMULATOR
  profile.platform = MetalApi::PrecompiledPlatform::IOSSimulator;
#else
  profile.platform = MetalApi::PrecompiledPlatform::IOSDevice;
#endif
  profile.entryPoint            = "main0";
  profile.flipVertY             = true;
  profile.bufferSizeBufferIndex = 29;

  bool tier2 = false;
  if(@available(iOS 16.0, *))
    tier2 = [device supportsFamily:MTLGPUFamilyMetal3] &&
            device.argumentBuffersSupport>=MTLArgumentBuffersTier2;
  profile.argumentBuffersTier        = uint8_t(tier2 ? 1 : 0);
  profile.runtimeArrayRichDescriptor = tier2;
  profile.readWriteTextureFences     = profile.mslVersion<20000;
  profile.nativeImageAtomics         = language>=MetalLanguage31;
  if(profile.nativeImageAtomics) {
    profile.r32uiLinearTextureAlignment = 4;
    profile.r32uiAlignmentConstantId    = 65535;
    }
  else {
    const NSUInteger alignment =
        [device minimumLinearTextureAlignmentForPixelFormat:MTLPixelFormatR32Uint];
    if(alignment>std::numeric_limits<uint32_t>::max())
      return false;
    profile.r32uiLinearTextureAlignment = uint32_t(alignment);
    profile.r32uiAlignmentConstantId    = 0;
    }
  return true;
  }

const OpenGothic::IOSStartupShaders::ExpectedMetalLibraryHash*
expectedHash(std::string_view resourceName, uint32_t mslVersion) {
  for(const auto& hash:OpenGothic::IOSStartupShaders::ExpectedMetalLibraryHashes) {
    if(hash.mslVersion==mslVersion && resourceName==hash.resourceName)
      return &hash;
    }
  return nullptr;
  }

void addLibrary(MetalApi::Options& options, std::string_view shaderName,
                MetalApi::PrecompiledShaderStage stage, const Profile& baseProfile) {
  const char* suffix = versionSuffix(baseProfile.mslVersion);
  if(suffix==nullptr)
    return;
  const std::string resourceName =
      std::string("OpenGothic") + std::string(shaderName) + "_" + suffix;
  const auto* expected = expectedHash(resourceName,baseProfile.mslVersion);
  if(expected==nullptr)
    return;

  MetalApi::PrecompiledLibrary library;
  std::vector<uint8_t> canonicalMsl;
  if(!loadResource(resourceName,"metallib",library.data) ||
     !loadResource(resourceName,"mslsrc",canonicalMsl))
    return;
  library.dataHash = expected->sha256;
  if(MetalApi::precompiledLibraryHash(library.data.data(),library.data.size())!=
     library.dataHash)
    return;

  Profile profile = baseProfile;
  profile.stage = stage;
  const std::string_view source(reinterpret_cast<const char*>(canonicalMsl.data()),
                                canonicalMsl.size());
  MetalApi::PrecompiledShader shader;
  shader.profile = std::move(profile);
  shader.key     = MetalApi::precompiledShaderKey(source,shader.profile);
  library.shaders.push_back(std::move(shader));
  options.precompiledLibraries.push_back(std::move(library));
  }

}

void addIOSStartupShaders(Tempest::MetalApi::Options& options) noexcept {
  try {
    @autoreleasepool {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if(device==nil)
        return;
      Profile profile;
      const bool valid = currentProfile(device,profile);
      [device release];
      if(!valid)
        return;
      addLibrary(options,"Triangle", MetalApi::PrecompiledShaderStage::Vertex,  profile);
      addLibrary(options,"Downscale",MetalApi::PrecompiledShaderStage::Fragment,profile);
      }
    }
  catch(...) {
    // Startup libraries are optional; Tempest compiles the original SPIR-V at runtime.
    }
  }
