#include "iosstartupshaders.h"

#import <Foundation/Foundation.h>

#include <opengothic_ios_startup_hashes.h>

#include <cstring>
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
      Profile profile;
      if(!MetalApi::currentPrecompiledShaderProfile(
             MetalApi::PrecompiledShaderStage::Vertex,profile) ||
         versionSuffix(profile.mslVersion)==nullptr)
        return;
      addLibrary(options,"Triangle", MetalApi::PrecompiledShaderStage::Vertex,  profile);
      addLibrary(options,"Downscale",MetalApi::PrecompiledShaderStage::Fragment,profile);
      }
    }
  catch(...) {
    // Startup libraries are optional; Tempest compiles the original SPIR-V at runtime.
    }
  }
