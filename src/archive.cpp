#include "interfaces.h"

#include <algorithm>
#include <array>
#include <istream>
#include <limits>
#include <ostream>

namespace team14 {
namespace {
Result malformed(const char* message) { return {ErrorCode::InvalidArchive, message}; }

// Structural invariants only. Huffman consistency, output limits and CRC
// verification remain the validation/codec/controller modules' responsibility.
Result checkStructure(const ArchiveMetadata& metadata) {
    if (metadata.version != kArchiveVersion) {
        return {ErrorCode::UnsupportedVersion, "Unsupported archive version."};
    }
    if ((metadata.flags & ~kFlagCrc32Present) != 0) {
        return malformed("Archive contains unknown flags.");
    }
    if (!(metadata.flags & kFlagCrc32Present) && metadata.crc32 != 0) {
        return malformed("CRC32 must be zero when its flag is absent.");
    }
    std::uint64_t total = 0;
    unsigned symbols = 0;
    for (const auto frequency : metadata.frequencies) {
        if (!frequency) continue;
        ++symbols;
        if (frequency > std::numeric_limits<std::uint64_t>::max() - total) {
            return malformed("Archive frequency sum overflows.");
        }
        total += frequency;
    }
    if (total != metadata.originalBytes) {
        return malformed("Archive frequencies do not match original size.");
    }
    if (!symbols && (metadata.originalBytes != 0 || metadata.encodedBits != 0 || metadata.crc32 != 0)) {
        return malformed("Empty archive metadata is inconsistent.");
    }
    if (symbols && metadata.encodedBits < metadata.originalBytes) {
        return malformed("Archive bit count is smaller than its original byte count.");
    }
    if (symbols == 1 && metadata.encodedBits != metadata.originalBytes) {
        return malformed("Single-symbol archive must use one bit per byte.");
    }
    return {};
}

// Explicit byte arrays avoid host endianness, struct padding and signed-char
// conversions. All calls use sizes 1, 2, 4 or 8.
bool readUnsigned(std::istream& input, unsigned size, std::uint64_t& value) {
    std::array<unsigned char, 8> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!input) return false;
    value = 0;
    for (unsigned n = 0; n < size; ++n) value |= std::uint64_t(bytes[n]) << (8 * n);
    return true;
}
void writeUnsigned(std::ostream& output, std::uint64_t value, unsigned size) {
    std::array<unsigned char, 8> bytes{};
    for (unsigned n = 0; n < size; ++n) bytes[n] = static_cast<unsigned char>(value >> (8 * n));
    output.write(reinterpret_cast<const char*>(bytes.data()), size);
}
Result readError(const std::istream& input) {
    return input.bad() ? Result{ErrorCode::IoError, "Could not read archive stream."}
                       : malformed("Archive header is truncated or unreadable.");
}
} // namespace

Result readArchiveHeader(std::istream& archive, ArchiveMetadata& metadata) {
    metadata = ArchiveMetadata{};
    try {
        std::array<char, 4> magic{};
        archive.read(magic.data(), magic.size());
        if (!archive) return readError(archive);
        if (magic != std::array<char, 4>{'H', 'U', 'F', '1'}) {
            return malformed("Invalid archive magic; expected HUF1.");
        }
        ArchiveMetadata parsed{};
        std::uint64_t value = 0;
        if (!readUnsigned(archive, 1, value)) return readError(archive);
        parsed.version = static_cast<std::uint8_t>(value);
        if (parsed.version != kArchiveVersion) {
            return {ErrorCode::UnsupportedVersion, "Unsupported archive version."};
        }
        if (!readUnsigned(archive, 1, value)) return readError(archive);
        parsed.flags = static_cast<std::uint8_t>(value);
        if (!readUnsigned(archive, 2, value)) return readError(archive);
        if (value != 0) return malformed("Archive reserved bytes must be zero.");
        if (!readUnsigned(archive, 8, parsed.originalBytes) ||
            !readUnsigned(archive, 8, parsed.encodedBits) ||
            !readUnsigned(archive, 4, value)) return readError(archive);
        parsed.crc32 = static_cast<std::uint32_t>(value);
        if (!readUnsigned(archive, 2, value)) return readError(archive);
        if (value > 256) return malformed("Archive symbol count exceeds 256.");
        const auto symbols = static_cast<unsigned>(value);
        int previous = -1;
        for (unsigned n = 0; n < symbols; ++n) {
            if (!readUnsigned(archive, 1, value)) return readError(archive);
            const auto symbol = static_cast<unsigned>(value);
            if (static_cast<int>(symbol) <= previous) {
                return malformed("Archive symbols must be strictly ascending and unique.");
            }
            previous = static_cast<int>(symbol);
            if (!readUnsigned(archive, 8, value)) return readError(archive);
            if (value == 0) return malformed("Stored archive frequencies must be nonzero.");
            parsed.frequencies[symbol] = value;
        }
        const Result result = checkStructure(parsed);
        if (!result.ok()) return result;
        // Do not seek, peek at or consume payload: non-seekable streams work too.
        metadata = parsed;
        return {};
    } catch (const std::ios_base::failure&) {
        return readError(archive);
    }
}

Result writeArchive(std::ostream& archive, const ArchiveMetadata& metadata,
                    std::istream& packedPayload) {
    const auto validation = checkStructure(metadata);
    if (!validation.ok()) return validation;
    try {
        if (!archive || !packedPayload) {
            return {ErrorCode::IoError, "Archive or payload stream is not ready."};
        }
        unsigned symbols = 0;
        for (auto frequency : metadata.frequencies) if (frequency) ++symbols;
        archive.write("HUF1", 4);
        writeUnsigned(archive, metadata.version, 1);
        writeUnsigned(archive, metadata.flags, 1);
        writeUnsigned(archive, 0, 2);
        writeUnsigned(archive, metadata.originalBytes, 8);
        writeUnsigned(archive, metadata.encodedBits, 8);
        writeUnsigned(archive, metadata.crc32, 4);
        writeUnsigned(archive, symbols, 2);
        for (unsigned symbol = 0; symbol < 256; ++symbol) {
            if (metadata.frequencies[symbol]) {
                writeUnsigned(archive, symbol, 1);
                writeUnsigned(archive, metadata.frequencies[symbol], 8);
            }
        }
        if (!archive) return {ErrorCode::IoError, "Could not write archive header."};
        // ceil(bits/8), without overflowing bits+7. Memory use stays bounded.
        std::uint64_t remaining = metadata.encodedBits / 8 + (metadata.encodedBits % 8 != 0);
        std::array<char, 65536> buffer{};
        while (remaining) {
            const auto size = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, buffer.size()));
            packedPayload.read(buffer.data(), size);
            if (!packedPayload) {
                return packedPayload.bad()
                    ? Result{ErrorCode::IoError, "Could not read packed payload."}
                    : malformed("Packed payload is shorter than the declared bit count.");
            }
            remaining -= static_cast<std::uint64_t>(size);
            if (!remaining && metadata.encodedBits % 8) {
                const auto mask = (1U << (8 - metadata.encodedBits % 8)) - 1U;
                if ((static_cast<unsigned char>(buffer[static_cast<std::size_t>(size) - 1]) & mask) != 0) {
                    return malformed("Packed payload contains nonzero padding bits.");
                }
            }
            archive.write(buffer.data(), size);
            if (!archive) return {ErrorCode::IoError, "Could not write archive payload."};
        }
        // Check for extra spool bytes. EOF is expected, even for streams that
        // enable failbit exceptions. Do not clear or replace the caller's mask.
        int extra = std::char_traits<char>::eof();
        try {
            extra = packedPayload.peek();
        } catch (const std::ios_base::failure&) {
            if (packedPayload.bad() || !packedPayload.eof()) throw;
        }
        if (packedPayload.bad() || (packedPayload.fail() && !packedPayload.eof())) {
            return {ErrorCode::IoError, "Could not check packed payload boundary."};
        }
        if (extra != std::char_traits<char>::eof()) {
            return malformed("Packed payload has trailing bytes.");
        }
        // Surface buffered output failures; the controller owns close, rollback
        // and publication. A failed call can leave bytes in the supplied stream.
        archive.flush();
        if (!archive) return {ErrorCode::IoError, "Could not flush archive output."};
        return {};
    } catch (const std::ios_base::failure&) {
        if (archive.bad() || archive.fail() || packedPayload.bad()) {
            return {ErrorCode::IoError, "Archive stream I/O failed."};
        }
        return malformed("Packed payload is truncated or unreadable.");
    }
}
} // namespace team14
