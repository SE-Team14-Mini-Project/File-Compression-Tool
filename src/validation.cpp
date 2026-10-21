#include "interfaces.h"

namespace team14 {

// FR-DEC-01 / SEC-REQ-01 (TC-08, ST-08): reject unsupported archive formats.
//
// Magic bytes are checked while the header is parsed (readArchiveHeader), because
// the magic is not stored in ArchiveMetadata. The format version IS stored there,
// so it is checked again here. That keeps this function safe to call on its own
// and means the controller does not trust the parser as the only gatekeeper.
//
// Scope of this change: format version only. The declared-output-size limit
// (maxOutputBytes), CRC32 and path checks belong to other requirements.
Result validateArchiveMetadata(const ArchiveMetadata& metadata,
                               std::uint64_t /*maxOutputBytes*/) {
    if (metadata.version != kArchiveVersion) {
        return {ErrorCode::UnsupportedVersion, "Unsupported archive version."};
    }
    return {};
}

} // namespace team14