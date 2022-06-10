/*
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#pragma once

#include "Polygon.h"

#include <string>
#include <vector>

#define GDS_MAX_STR_NAME (32)

namespace GDS
{
	struct Bndry {
		uint16_t layer = 0xFFFF;
		std::vector<Pair> pairs;
	};

	struct Path {
		uint16_t layer = 0xFFFF;

		std::vector<Pair> pairs;

		uint16_t pathtype;
		uint32_t width;
	};

	struct SRef {
		int32_t x, y;

		wchar_t sname[GDS_MAX_STR_NAME + 1];

		uint16_t strans;
		double mag = 1.0, angle;
	};

	struct Aref {
		int32_t x1, y1, x2, y2, x3, y3;

		wchar_t sname[GDS_MAX_STR_NAME + 1];

		uint16_t col, row;

		uint16_t strans;
		double mag = 1.0, angle;
	};

	struct Cell {
		wchar_t wstrname[GDS_MAX_STR_NAME + 1];

		std::vector<Bndry> boundaries;
		std::vector<Path> paths;
		std::vector<SRef> srefs;
		std::vector<Aref> arefs;
	};
}

namespace GDS
{
	struct Database {
		
		Database(const wchar_t* file); // Construct from a GDS file

		// Collapses cell and write to file and/or a Polygon vector.
		void CollapseCell(const wchar_t* cell, const double* bounds, uint64_t max_polys, const wchar_t* dest, std::vector<Polygon>* pset);

		void AllCells(std::vector<std::wstring>& sset); // Write all the cells to a vector

		void TopCells(std::vector<std::wstring>& sset); // Write the top cells to a vector

		
		double m_uu_per_dbunit = 0.0, m_meter_per_dbunit = 0.0; // Units from the GDS_UNITS record


		std::wstring m_filePath;

		std::vector<Cell> m_cells;

		std::vector<std::wstring> m_libnames;

		uint16_t m_version = 0; // The GDS version (must be 6 or 600)

		// The raw data in the GDS_UNITS record read (so as to easily write back
		// to an output file without conversions.
		uint8_t m_units[16] = { 0 };
	};

	// Static helper function (unrelated to this class).
	bool PointInPoly(Pair* poly, int n, Pair p);
}



