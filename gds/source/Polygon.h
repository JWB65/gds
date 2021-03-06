/*
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#pragma once

#include <vector>

namespace GDS {

	struct Pair {
		int32_t x, y;
	};

	struct Polygon {
		Polygon(Pair* p, size_t size, uint16_t layer);
		std::vector<Pair> m_pairs;
		uint16_t m_layer;
	};
}

