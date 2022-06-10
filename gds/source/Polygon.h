/*
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#pragma once

#include <cstdint>
#include <vector>

namespace GDS {

	struct Pair {
		int32_t x, y;
	};

	class Polygon {
	public:
		Polygon(Pair* p, size_t size, uint16_t layer);
		~Polygon();

	private:
		Pair* m_pairs;
		size_t m_size;
		uint16_t m_layer;
	};
}

