/*
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#pragma warning( disable : 6011 4711 5045 4710)

#include "Polygon.h"

namespace GDS {
	Polygon::Polygon(Pair* p, size_t size, uint16_t layer)
	{
		m_pairs = new Pair[size];
		m_size = size;
		m_layer = layer;

		for (size_t i = 0; i < size; ++i) {
			m_pairs[i].x = p[i].x;
			m_pairs[i].y = p[i].y;
		}
	}

	Polygon::~Polygon()
	{
		delete[] m_pairs;
	}
}