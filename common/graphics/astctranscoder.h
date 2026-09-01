#pragma once

#if defined(HAS_ASTCENC)

#include <Tempest/Pixmap>

#include <zenkit/Texture.hh>

#include <cstdint>
#include <string_view>

namespace zenkit {
  class Read;
  }

namespace AstcTranscoder {
  struct SourceFingerprint final {
    uint64_t size = 0;
    uint64_t hash = 0;
    bool     valid = false;
    };

  // Hashes the complete source entry and rewinds it to byte zero.
  SourceFingerprint fingerprint(zenkit::Read& input);

  // True only when this source BC format is unsupported and ASTC 4x4 is
  // sampleable. Capability checks, rather than platform checks, select it.
  bool enabled(zenkit::TextureFormat sourceFormat);

  // Returns an ASTC 4x4 mip chain, or an empty pixmap on a safe fallback.
  Tempest::Pixmap transcode(std::string_view name, const zenkit::Texture& texture,
                            SourceFingerprint source);

  void logStats();
  }

#endif
