#pragma once

#include "Constants.h"
#include "SLTerrain.h"

namespace RTE {

	/// Picks a deterministic, solid terrain pixel for the opt-in world-state sync smoke test.
	/// Host and client load the same Scene preset, so they derive the same probe without
	/// changing the network protocol or depending on a test-only packet.
	inline bool FindWorldStateTerrainTestPixel(const SLTerrain& terrain, int& pixelX, int& pixelY) {
		const int width = terrain.GetWidth();
		const int height = terrain.GetHeight();
		if (width <= 0 || height <= 0) {
			return false;
		}

		const int firstX = width / 4;
		const int lastX = width - firstX;
		const int firstY = height / 4;
		for (int y = firstY; y < height; ++y) {
			for (int x = firstX; x < lastX; ++x) {
				if (terrain.GetMaterialPixel(x, y) > g_MaterialCavity) {
					pixelX = x;
					pixelY = y;
					return true;
				}
			}
		}
		return false;
	}

} // namespace RTE
