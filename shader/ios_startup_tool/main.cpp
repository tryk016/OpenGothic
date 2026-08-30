#include <spirv_msl.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Arguments final {
  std::string input;
  std::string output;
  std::string stage;
  uint32_t    mslVersion              = 0;
  uint32_t    argumentBuffersTier     = 0;
  bool        richDescriptor          = false;
  uint32_t    r32uiAlignment          = 0;
  uint32_t    r32uiAlignmentConstantId= 0;
};

uint32_t parseUint(std::string_view text, std::string_view option) {
  if(text.empty())
    throw std::runtime_error(std::string(option) + " requires an unsigned integer");
  size_t consumed = 0;
  unsigned long value = 0;
  try {
    value = std::stoul(std::string(text),&consumed,10);
    }
  catch(...) {
    throw std::runtime_error(std::string(option) + " requires an unsigned integer");
    }
  if(consumed!=text.size() || value>std::numeric_limits<uint32_t>::max())
    throw std::runtime_error(std::string(option) + " is out of range");
  return uint32_t(value);
}

Arguments parseArguments(int argc, char** argv) {
  Arguments args;
  for(int i=1; i<argc; ++i) {
    const std::string_view option = argv[i];
    if(i+1>=argc)
      throw std::runtime_error(std::string(option) + " requires a value");
    const std::string_view value = argv[++i];
    if(option=="--input")
      args.input = value;
    else if(option=="--output")
      args.output = value;
    else if(option=="--stage")
      args.stage = value;
    else if(option=="--msl-version")
      args.mslVersion = parseUint(value,option);
    else if(option=="--argument-buffers-tier")
      args.argumentBuffersTier = parseUint(value,option);
    else if(option=="--runtime-array-rich-descriptor") {
      const uint32_t enabled = parseUint(value,option);
      if(enabled>1)
        throw std::runtime_error(std::string(option) + " must be 0 or 1");
      args.richDescriptor = enabled!=0;
      }
    else if(option=="--r32ui-linear-texture-alignment")
      args.r32uiAlignment = parseUint(value,option);
    else if(option=="--r32ui-alignment-constant-id")
      args.r32uiAlignmentConstantId = parseUint(value,option);
    else
      throw std::runtime_error("Unknown option: " + std::string(option));
    }

  if(args.input.empty() || args.output.empty() || args.stage.empty() ||
     args.mslVersion==0 || args.r32uiAlignment==0)
    throw std::runtime_error("Missing a required startup MSL generator option");
  if(args.argumentBuffersTier>1)
    throw std::runtime_error("--argument-buffers-tier must be 0 or 1");
  if(args.stage!="vertex" && args.stage!="fragment")
    throw std::runtime_error("--stage must be vertex or fragment");
  return args;
}

std::vector<uint32_t> readSpirv(const std::string& path) {
  std::ifstream input(path,std::ios::binary | std::ios::ate);
  if(!input)
    throw std::runtime_error("Cannot open SPIR-V input: " + path);
  const auto end = input.tellg();
  if(end<=0 || uint64_t(end)%sizeof(uint32_t)!=0)
    throw std::runtime_error("SPIR-V input has an invalid byte size: " + path);
  if(uint64_t(end)>std::numeric_limits<size_t>::max())
    throw std::runtime_error("SPIR-V input is too large: " + path);
  std::vector<uint32_t> words(size_t(end)/sizeof(uint32_t));
  input.seekg(0);
  input.read(reinterpret_cast<char*>(words.data()),std::streamsize(end));
  if(!input)
    throw std::runtime_error("Cannot read SPIR-V input: " + path);
  return words;
}

void writeMsl(const std::string& path, const std::string& msl) {
  std::ofstream output(path,std::ios::binary | std::ios::trunc);
  if(!output)
    throw std::runtime_error("Cannot open MSL output: " + path);
  output.write(msl.data(),std::streamsize(msl.size()));
  if(!output)
    throw std::runtime_error("Cannot write MSL output: " + path);
}

size_t countToken(const std::string& source, std::string_view token) {
  size_t count = 0;
  for(size_t at=0; (at=source.find(token,at))!=std::string::npos; at+=token.size())
    ++count;
  return count;
}

void validateEntryPoint(const spirv_cross::CompilerMSL& compiler,
                        const Arguments& args, const std::string& msl) {
  const auto entryPoints = compiler.get_entry_points_and_stages();
  const spv::ExecutionModel expected = args.stage=="vertex"
      ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
  if(entryPoints.size()!=1 || entryPoints.front().execution_model!=expected)
    throw std::runtime_error("Startup SPIR-V must contain exactly one entry point of the requested stage");

  const std::string stagePrefix = "\n" + args.stage + " ";
  if(countToken(msl,stagePrefix)!=1 || countToken(msl," main0(")!=1 ||
     countToken(msl,"\nvertex ") + countToken(msl,"\nfragment ") +
       countToken(msl,"\nkernel ")!=1)
    throw std::runtime_error("Generated MSL must contain exactly one entry point named main0");
}

int run(int argc, char** argv) {
  const Arguments args = parseArguments(argc,argv);
  const std::vector<uint32_t> spirv = readSpirv(args.input);

  spirv_cross::CompilerMSL compiler(spirv);
  spirv_cross::CompilerMSL::Options mslOptions;
  mslOptions.platform = spirv_cross::CompilerMSL::Options::iOS;
  mslOptions.msl_version = args.mslVersion;
  mslOptions.buffer_size_buffer_index = 29;
  mslOptions.argument_buffers_tier =
      static_cast<spirv_cross::CompilerMSL::Options::ArgumentBuffersTier>(
          args.argumentBuffersTier);
  mslOptions.runtime_array_rich_descriptor = args.richDescriptor;
  mslOptions.readwrite_texture_fences = false;
  mslOptions.r32ui_linear_texture_alignment = args.r32uiAlignment;
  mslOptions.r32ui_alignment_constant_id = args.r32uiAlignmentConstantId;

  spirv_cross::CompilerGLSL::Options commonOptions;
  commonOptions.vertex.flip_vert_y = true;
  compiler.set_msl_options(mslOptions);
  compiler.set_common_options(commonOptions);

  const std::string msl = compiler.compile();
  validateEntryPoint(compiler,args,msl);
  writeMsl(args.output,msl);
  return 0;
}

}

int main(int argc, char** argv) {
  try {
    return run(argc,argv);
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
    }
}
