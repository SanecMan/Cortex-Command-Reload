#pragma once

#include "WorldStateProtocol.h"

#include "lz4.h"

#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace RTE::WorldStateCompression {

inline constexpr std::uint32_t c_CompressedMagic = 0x5A524343; // "CCRZ".
inline constexpr std::size_t c_HeaderSize = 8;

inline bool EncodeForWire(std::span<const std::uint8_t> packet, std::vector<std::uint8_t>& output, bool& compressed) {
	compressed = false;
	if (packet.empty() || packet.size() > WorldStateProtocol::c_MaxPacketSize || packet.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		return false;
	}

	const int inputSize = static_cast<int>(packet.size());
	const int maximumCompressedSize = LZ4_compressBound(inputSize);
	std::vector<std::uint8_t> candidate(c_HeaderSize + static_cast<std::size_t>(maximumCompressedSize));
	const int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(packet.data()),
	                                                reinterpret_cast<char*>(candidate.data() + c_HeaderSize), inputSize, maximumCompressedSize);
	if (compressedSize > 0 && static_cast<std::size_t>(compressedSize) + c_HeaderSize < packet.size()) {
		for (std::size_t byte = 0; byte < sizeof(c_CompressedMagic); ++byte) {
			candidate[byte] = static_cast<std::uint8_t>((c_CompressedMagic >> (byte * 8)) & 0xFF);
			candidate[sizeof(c_CompressedMagic) + byte] = static_cast<std::uint8_t>((static_cast<std::uint32_t>(packet.size()) >> (byte * 8)) & 0xFF);
		}
		candidate.resize(c_HeaderSize + static_cast<std::size_t>(compressedSize));
		output = std::move(candidate);
		compressed = true;
	} else {
		output.assign(packet.begin(), packet.end());
	}
	return true;
}

inline bool DecodeFromWire(std::span<const std::uint8_t> wirePacket, std::vector<std::uint8_t>& storage, std::span<const std::uint8_t>& packet) {
	if (wirePacket.size() < sizeof(c_CompressedMagic)) {
		return false;
	}
	std::uint32_t magic = 0;
	for (std::size_t byte = 0; byte < sizeof(magic); ++byte) {
		magic |= static_cast<std::uint32_t>(wirePacket[byte]) << (byte * 8);
	}
	if (magic != c_CompressedMagic) {
		packet = wirePacket;
		return true;
	}
	if (wirePacket.size() <= c_HeaderSize) {
		return false;
	}
	std::uint32_t uncompressedSize = 0;
	for (std::size_t byte = 0; byte < sizeof(uncompressedSize); ++byte) {
		uncompressedSize |= static_cast<std::uint32_t>(wirePacket[sizeof(magic) + byte]) << (byte * 8);
	}
	if (uncompressedSize == 0 || uncompressedSize > WorldStateProtocol::c_MaxPacketSize ||
	    uncompressedSize > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
		return false;
	}
	storage.resize(uncompressedSize);
	const int decodedSize = LZ4_decompress_safe(reinterpret_cast<const char*>(wirePacket.data() + c_HeaderSize),
	                                            reinterpret_cast<char*>(storage.data()), static_cast<int>(wirePacket.size() - c_HeaderSize),
	                                            static_cast<int>(storage.size()));
	if (decodedSize < 0 || static_cast<std::size_t>(decodedSize) != storage.size()) {
		storage.clear();
		return false;
	}
	packet = storage;
	return true;
}

} // namespace RTE::WorldStateCompression
