#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace RTE {

	/// Coalesces terrain pixel changes into bounded tile-aligned regions without
	/// allocating once per changed pixel.
	class TerrainDirtyGrid {
	public:
		struct Rectangle {
			int X = 0;
			int Y = 0;
			int Width = 0;
			int Height = 0;
		};

		bool Configure(int width, int height, int tileSize, bool wrapX) {
			Clear();
			if (width <= 0 || height <= 0 || tileSize <= 0) return false;
			m_Width = width;
			m_Height = height;
			m_TileSize = tileSize;
			m_WrapX = wrapX;
			m_Columns = (width - 1) / tileSize + 1;
			m_Rows = (height - 1) / tileSize + 1;
			if (static_cast<std::size_t>(m_Columns) > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(m_Rows)) {
				Clear();
				return false;
			}
			const std::size_t cellCount = static_cast<std::size_t>(m_Columns) * static_cast<std::size_t>(m_Rows);
			m_DirtyTiles.assign(cellCount, 0);
			return true;
		}

		void Clear() {
			m_Width = 0;
			m_Height = 0;
			m_TileSize = 0;
			m_Columns = 0;
			m_Rows = 0;
			m_WrapX = false;
			m_DirtyTiles.clear();
		}

		bool IsConfigured() const { return !m_DirtyTiles.empty(); }

		void Mark(int x, int y, int width, int height) {
			if (!IsConfigured() || width <= 0 || height <= 0) return;
			const std::int64_t top = std::max<std::int64_t>(0, y);
			const std::int64_t bottom = std::min<std::int64_t>(m_Height, static_cast<std::int64_t>(y) + height);
			if (top >= bottom) return;

			if (m_WrapX) {
				if (width >= m_Width) {
					MarkClippedRange(0, m_Width, static_cast<int>(top), static_cast<int>(bottom));
					return;
				}
				const int startX = PositiveModulo(x, m_Width);
				const int endX = startX + width;
				if (endX <= m_Width) {
					MarkClippedRange(startX, endX, static_cast<int>(top), static_cast<int>(bottom));
				} else {
					MarkClippedRange(startX, m_Width, static_cast<int>(top), static_cast<int>(bottom));
					MarkClippedRange(0, endX - m_Width, static_cast<int>(top), static_cast<int>(bottom));
				}
			} else {
				const std::int64_t left = std::max<std::int64_t>(0, x);
				const std::int64_t right = std::min<std::int64_t>(m_Width, static_cast<std::int64_t>(x) + width);
				if (left < right) MarkClippedRange(static_cast<int>(left), static_cast<int>(right), static_cast<int>(top), static_cast<int>(bottom));
			}
		}

		std::vector<Rectangle> Drain() {
			std::vector<Rectangle> result;
			if (!IsConfigured()) return result;
			using Run = std::pair<int, int>;
			std::map<Run, Rectangle> active;
			for (int row = 0; row < m_Rows; ++row) {
				std::map<Run, Rectangle> next;
				for (int column = 0; column < m_Columns;) {
					if (!IsDirty(column, row)) {
						++column;
						continue;
					}
					const int firstColumn = column;
					while (column < m_Columns && IsDirty(column, row)) {
						SetDirty(column, row, false);
						++column;
					}
					const Run run{firstColumn, column};
					if (auto previous = active.find(run); previous != active.end()) {
						Rectangle rectangle = previous->second;
						rectangle.Height = std::min(m_Height, (row + 1) * m_TileSize) - rectangle.Y;
						next.emplace(run, rectangle);
					} else {
						const int x = firstColumn * m_TileSize;
						const int y = row * m_TileSize;
						Rectangle rectangle{x, y, std::min(m_Width, column * m_TileSize) - x, std::min(m_Height, (row + 1) * m_TileSize) - y};
						next.emplace(run, rectangle);
					}
				}
				for (const auto& [run, rectangle] : active) {
					if (!next.contains(run)) result.push_back(rectangle);
				}
				active = std::move(next);
			}
			for (const auto& [run, rectangle] : active) {
				result.push_back(rectangle);
			}
			return result;
		}

	private:
		static int PositiveModulo(int value, int divisor) {
			const int remainder = value % divisor;
			return remainder < 0 ? remainder + divisor : remainder;
		}

		std::size_t Index(int column, int row) const { return static_cast<std::size_t>(row) * m_Columns + column; }
		bool IsDirty(int column, int row) const { return m_DirtyTiles[Index(column, row)] != 0; }
		void SetDirty(int column, int row, bool dirty) { m_DirtyTiles[Index(column, row)] = dirty ? 1 : 0; }

		void MarkClippedRange(int left, int right, int top, int bottom) {
			if (left >= right || top >= bottom) return;
			const int firstColumn = left / m_TileSize;
			const int lastColumn = (right - 1) / m_TileSize;
			const int firstRow = top / m_TileSize;
			const int lastRow = (bottom - 1) / m_TileSize;
			for (int row = firstRow; row <= lastRow; ++row) {
				for (int column = firstColumn; column <= lastColumn; ++column) SetDirty(column, row, true);
			}
		}

		int m_Width = 0;
		int m_Height = 0;
		int m_TileSize = 0;
		int m_Columns = 0;
		int m_Rows = 0;
		bool m_WrapX = false;
		std::vector<std::uint8_t> m_DirtyTiles;
	};

} // namespace RTE
