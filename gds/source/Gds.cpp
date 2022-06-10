/*
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

//#pragma warning( disable : 6011 4711 5045 4710)

#include "Gds.h"
#include "GdsRecords.h"
#include "StringConverter.h"
#include <stdexcept>

const double M_PI = 3.14159265358979323846;

using namespace GDS;

struct gds_line {
	// Standard form of a line.

	double a, b, c;
};

struct gds_trans {
	// GDS reference cell rransformation parameters.

	int32_t x = 0, y = 0;
	double mag = 1.0, angle = 0.0;
	uint16_t mirror = 0;
};

struct Recdata {
	Database* gds;

	bool usebbox;

	uint64_t scount, pcount, max_polys;

	Pair bbox[5];

	FILE* poutfile;
	std::vector<Polygon*>* pset;
};


static double BufReadFloat(uint8_t* p)
{
	int i, sign, exp;
	double fraction;

	// The binary representation is with 7 bits in the exponent and 56 bits in
	// the fraction:
	//
	// SEEEEEEE MMMMMMMM MMMMMMMM MMMMMMMM
	// MMMMMMMM MMMMMMMM MMMMMMMM MMMMMMMM

	// Denominators for the 7 fraction bytes
	uint64_t div[7] = {
		0x0100,
		0x010000,
		0x01000000,
		0x0100000000,
		0x010000000000,
		0x01000000000000,
		0x0100000000000000
	};

	sign = 1;
	exp = 0;
	if (p[0] > 127) {
		sign = -1;
		exp = p[0] - 192;
	} else {
		exp = p[0] - 64;
	}

	fraction = 0.0;
	for (i = 0; i < 7; ++i) {
		fraction += p[i + 1] / (double)div[i];
	}

	return (pow(16, exp) * sign * fraction);
}

static Pair SumPairs(Pair one, Pair two)
{
	Pair out;

	out.x = one.x + two.x;
	out.y = one.y + two.y;

	return out;
}

//
// Functions to add data to the output file
//

static void FileAppendBytes(FILE* file, uint16_t record, uint8_t* data, size_t len)
{
	uint8_t buf[4];

	buf[0] = uint8_t(((len + 4) >> 8) & 0xFF);
	buf[1] = uint8_t((len + 4) & 0xFF);
	buf[2] = uint8_t((record >> 8) & 0xFF);
	buf[3] = uint8_t(record & 0xFF);

	fwrite(buf, 4, 1, file);
	fwrite(data, len, 1, file);
}

static void FileAppendRecord(FILE* file, uint16_t record)
{
	uint8_t buf[4];

	buf[0] = 0;
	buf[1] = 4;
	buf[2] = uint8_t((record >> 8) & 0xFF);
	buf[3] = uint8_t(record & 0xFF);

	fwrite(buf, 4, 1, file);
}

static void FileAppendShort(FILE* file, uint16_t record, uint16_t data)
{
	uint8_t buf[6];

	buf[0] = 0;
	buf[1] = 6;
	buf[2] = uint8_t((record >> 8) & 0xFF);
	buf[3] = uint8_t(record & 0xFF);

	buf[4] = uint8_t((data >> 8) & 0xFF);
	buf[5] = uint8_t(data & 0xFF);

	fwrite(buf, 6, 1, file);
}

static void FileAppendString(FILE* file, uint16_t record, const char* string)
{
	uint8_t buf[4];
	size_t text_len, record_len;

	text_len = strlen(string);

	record_len = text_len;
	if (text_len % 2) record_len++;

	buf[0] = uint8_t(((record_len + 4) >> 8) & 0xFF);
	buf[1] = uint8_t((record_len + 4) & 0xFF);
	buf[2] = uint8_t((record >> 8) & 0xFF);
	buf[3] = uint8_t(record & 0xFF);

	fwrite(buf, 4, 1, file);
	fwrite(string, text_len, 1, file);
	if (text_len % 2) putc(0, file);
}


static void TransformPoly(Pair* pout, Pair* pin, size_t size, gds_trans tra)
{
	unsigned int i;
	double sign = 1.0;
	double angle_rad = M_PI * tra.angle / 180.0;

	// Reflect with respect to x axis first before rotation
	if (tra.mirror)
		sign = -1.0;

	for (i = 0; i < size; i++) {
		pout[i].x = (int)(tra.x + tra.mag * (pin[i].x * cos(angle_rad) - sign * pin[i].y * sin(angle_rad)));
		pout[i].y = (int)(tra.y + tra.mag * (pin[i].x * sin(angle_rad) +
			sign * pin[i].y * cos(angle_rad)));
	}
}

//
// Functions to add a polygon to a file and polygon set
//

static void BufWriteInt(uint8_t* p, size_t offset, int32_t n) {
	p[offset + 0] = uint8_t((n >> 24) & 0xFF);
	p[offset + 1] = uint8_t((n >> 16) & 0xFF);
	p[offset + 2] = uint8_t((n >> 8) & 0xFF);
	p[offset + 3] = uint8_t(n & 0xFF);
}

static void FileAppendPoly(FILE* pfile, Pair* p, size_t size, uint16_t layer)
{
	// Store polygon in buffer in the format of the gds standard.

	FileAppendRecord(pfile, GDS_BOUNDARY);
	FileAppendShort(pfile, GDS_LAYER, layer);
	FileAppendShort(pfile, GDS_DATATYPE, 0);

	uint8_t* buf = new uint8_t[8 * size];
	for (unsigned int i = 0; i < size; i++) {
		BufWriteInt(buf, 8 * i, p[i].x);
		BufWriteInt(buf, 8 * i + 4, p[i].y);
	}

	FileAppendBytes(pfile, GDS_XY, buf, 8 * size);
	FileAppendRecord(pfile, GDS_ENDEL);

	delete[] buf;
}

static bool TestPolyOverlap(Pair* p, size_t size, Pair* bbox)
{
	unsigned int i;
	int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;

	if (!bbox)
		return true;

	/* the final (closing) point is not needed for this evaluation */
	for (i = 0; i < size - 1; i++) {
		if (p[i].x > maxx)  maxx = p[i].x;
		if (p[i].y > maxy)  maxy = p[i].y;
		if (p[i].x < minx)  minx = p[i].x;
		if (p[i].y < miny)  miny = p[i].y;
	}

	return (miny <= bbox[2].y && maxy >= bbox[0].y && minx <= bbox[2].x &&
		maxx >= bbox[0].x);
}

static void AddPoly(Pair* pairs, size_t size, uint16_t layer, Recdata& data)
{
	data.scount++;

	// Add polygon to file or polygon set
	if (!data.usebbox || TestPolyOverlap(pairs, size, data.bbox)) {
		if (data.poutfile) {
			FileAppendPoly(data.poutfile, pairs, size, layer);
		}
		if (data.pset) {
			// Polygon on the heap to add to polyset
			//Pair* p = new Pair[size];
			//if (p)
				//memcpy(p, &pairs[0], size * sizeof(Pair));

			// https://stackoverflow.com/questions/9331561/why-does-my-classs-destructor-get-called-when-i-add-instances-to-a-vector
			// https://stackoverflow.com/questions/10488348/add-to-vector-without-copying-struct-data

			Polygon* p = new Polygon(pairs, size, layer);

			//Polygon p(pairs, size, layer);
			data.pset->push_back(p);
			//data.pset->emplace_back(p);
			//delete p;
		}
		data.pcount++;
	}

	if (data.pcount >= data.max_polys)
		return;

}

//
// Functions to expand a GDS PATH element
//

static Pair ExtendVector(Pair tail, Pair head, double length)
{
	int32_t segx, segy;
	double norm;
	Pair out = { tail.x, tail.y };

	segx = tail.x - head.x;
	segy = tail.y - head.y;
	norm = sqrt(double(segx) * segx + double(segy) * segy);

	if (norm != 0.0) {
		out.x = tail.x + (int)((length / norm) * segx);
		out.y = tail.y + (int)((length / norm) * segy);
	}

	return out;
}

static Pair IntersectLines(gds_line* one, gds_line* two)
{
	double xh, yh, wh;

	// line-line intersection (homogenous coordinates):
	//		(B1C2 - B2C1, A2C1 - A1C2, A1B2 - A2B1)
	xh = one->b * two->c - two->b * one->c;
	yh = two->a * one->c - one->a * two->c;
	wh = one->a * two->b - two->a * one->b;

	Pair out = { (int)(xh / wh), (int)(yh / wh) };

	return out;
}

static Pair ProjectLine(Pair p, gds_line* line)
{
	gds_line normal;

	// normal to Ax+By+C = 0 through (x1, y1)
	// A'x+B'y+C' = 0 with
	// A' = B  B' = -A  C' = Ay1 - Bx1

	normal.a = line->b;
	normal.b = -line->a;
	normal.c = line->a * p.y - line->b * p.x;

	// intersect the normal through (p.x, p.y) with line
	return IntersectLines(line, &normal);
}

static void ExpandPath(Pair* pout, Pair* pin, size_t size, uint32_t width, int pathtype)
{
	size_t i;
	double hwidth = width / 2.0;
	gds_line* mline, * pline;
	Pair end_point;

	if (size < 2) return;

	// parallel line segments on both sides
	mline = new gds_line[size - 1];
	pline = new gds_line[size - 1];

	if (!mline || !pline) return;

	for (i = 0; i < size - 1; i++) {
		double a, b, c, c_trans;

		// line thr. (x1, y1) & (x2, y2): Ax + By + C = 0  A = y2 - y1
		//		B = -(x2 - x1)  C = -By1 - Ax1
		a = double(pin[i + 1].y) - pin[i].y;
		b = -(double(pin[i + 1].x) - pin[i].x);
		c = -b * pin[i].y - a * pin[i].x;

		// line parallel to Ax + By + C = 0 at distance d:   Ax + By +
		//		(C +/- d * sqrt(A^2 + B^2))
		c_trans = hwidth * sqrt(a * a + b * b);

		pline[i].a = mline[i].a = a;
		pline[i].b = mline[i].b = b;

		// c ooefficient in standard form of parallel segments
		pline[i].c = c + c_trans;
		mline[i].c = c - c_trans;
	}

	// head points
	if (pathtype == 2) {
		end_point = ExtendVector(pin[0], pin[1], hwidth);
	} else {
		end_point = pin[0];
	}
	pout[0] = ProjectLine(end_point, &pline[0]);
	pout[2 * size - 1] = ProjectLine(end_point, &mline[0]);
	pout[2 * size] = pout[0];

	// middle points
	for (i = 1; i < size - 1; i++) {
		pout[i] = IntersectLines(&pline[i - 1], &pline[i]);
		pout[2 * size - 1 - i] = IntersectLines(&mline[i - 1],
			&mline[i]);
	}

	// tail points
	if (pathtype == 2) {
		end_point = ExtendVector(pin[size - 1], pin[size - 2], hwidth);
	} else {
		end_point = pin[size - 1];
	}
	pout[size - 1] = ProjectLine(end_point, &pline[size - 2]);
	pout[size] = ProjectLine(end_point, &mline[size - 2]);

	delete[] pline;
	delete[] mline;
}

//
// Functions to recurse through the hierarchy of the cell
//

static Cell* FindCell(Database* gds, const wchar_t* name)
{
	for (auto it = std::begin(gds->m_cells); it != std::end(gds->m_cells); ++it) {
		if (wcscmp(it->wstrname, name) == 0)
			return &*it;
	}

	return nullptr;
}

static bool Recurse(Cell& top, gds_trans tra, Recdata& data)
{
	// Return false if the recursion needs to stop because of an error or the
	// max allowed output polygons is reached.

	Pair out[400];
	Pair tmp[400];

	// BOUNDARY elements
	for (auto it = std::begin(top.boundaries); it != std::end(top.boundaries); ++it)
	{
		TransformPoly(out, it->pairs.get(), it->size, tra);

		AddPoly(out, it->size, it->layer, data);

		if (data.pcount >= data.max_polys)
			return false;
	}

	// PATH elements
	for (auto it = std::begin(top.paths); it != std::end(top.paths); ++it)
	{
		// The size of the expanded polygon
		size_t out_size = 2 * it->size + 1U;

		// Expand and transform
		ExpandPath(tmp, it->pairs.get(), it->size, it->width, it->pathtype);
		TransformPoly(out, tmp, out_size, tra);

		AddPoly(out, out_size, it->layer, data);

		if (data.pcount >= data.max_polys)
			return false;
	}

	// SREF elements
	for (auto it = std::begin(top.srefs); it != std::end(top.srefs); ++it)
	{
		Cell* str;
		gds_trans acc_tra;

		str = FindCell(data.gds, it->sname);

		// This should not happen in a correct GDS file
		if (!str) {
			throw std::runtime_error("SREF cell not found");
		}

		// Accumulate the transformations

		acc_tra.x = tra.x + it->x;
		acc_tra.y = tra.y + it->y;
		acc_tra.mag = tra.mag * it->mag;
		acc_tra.angle = tra.angle + it->angle;
		acc_tra.mirror = static_cast<uint16_t> (tra.mirror ^ (it->strans & 0x8000));


		// Down a level
		if (!Recurse(*str, acc_tra, data))
			return false;
	}

	// Expand AREF elements
	for (auto it = std::begin(top.arefs); it != std::end(top.arefs); ++it)
	{
		Cell* str;
		Aref* p = &*it;

		// Find the structure name being referenced
		str = FindCell(data.gds, it->sname);

		if (!str) {
			throw std::runtime_error("AREF cell not found");
			return false;
		}

		// (v_col_x, v_col_y) vector in column direction
		double v_col_x = (double(p->x2) - p->x1) / p->col;
		double v_col_y = (double(p->y2) - p->y1) / p->col;

		// (v_row_x, v_row_y) vector in row direction
		double v_row_x = (double(p->x3) - p->x1) / p->row;
		double v_row_y = (double(p->y3) - p->y1) / p->row;

		// loop through the reference points of the array
		for (int col = 0; col < p->col; col++) {
			for (int row = 0; row < p->row; row++) {

				int x_ref, y_ref, x_ref_t, y_ref_t;
				double angle_radians, sign;
				gds_trans acc_tra;

				// Origin of the cell being referenced in the reference
				// frame of the aref
				x_ref = (int)(p->x1 + col * v_col_x + row * v_row_x);
				y_ref = (int)(p->y1 + col * v_col_y + row * v_row_y);


				// Origin of the cell being referenced in the reference
				// frame of the top cell (so after transformations)
				angle_radians = M_PI * tra.angle / 180.0;
				sign = 1.0;
				if (tra.mirror) {
					sign = -1;
				}
				x_ref_t = tra.x + (int)(tra.mag * (x_ref * cos(angle_radians) - sign * y_ref * sin(angle_radians)));
				y_ref_t = tra.y + (int)(tra.mag * (x_ref * sin(angle_radians) + sign * y_ref * cos(angle_radians)));

				// Cell referenced still needs to be transformed

				acc_tra.x = x_ref_t,
					acc_tra.y = y_ref_t;
				acc_tra.mag = tra.mag * p->mag;
				acc_tra.angle = tra.angle + p->angle;
				acc_tra.mirror = static_cast<uint16_t> (tra.mirror ^ (p->strans & 0x8000));


				// Down a level
				if (!Recurse(*str, acc_tra, data))
					return false;
			}
		}
	}

	return true;
}


Database::Database(const wchar_t* file)
{
	FILE* p_file;

	filePath = std::wstring(file);

	enum STR_TYPE
	{
		NONE,
		BRY,
		PATH,
		SREF,
		AREF
	} curElem = NONE;

	// Current GDS structure being read
	Cell curCell = {};

	// The different elements
	Bndry curBndry = {};
	Path curPath = {};
	SRef curSRef = {};
	Aref curARef = {};

	p_file = nullptr;
	_wfopen_s(&p_file, file, L"rb");

	if (!p_file)
	{
		throw std::runtime_error("Could not find or open GDS file");
	}

	// Number of bytes and cells read
	uint64_t bytes_read = 0;
	uint64_t cell_count = 0;
	bool readEndlib = false;

	while (true)
	{
		uint8_t rheader[4], * buf;
		uint16_t buf_size, record_len, record_type;

		fread(rheader, 1, 4, p_file);
		bytes_read += 4;

		// First 2 bytes: record length; second 2 bytes record type and data type
		record_len = uint16_t((rheader[0] << 8) | rheader[1]);
		record_type = uint16_t(rheader[2] << 8 | rheader[3]);

		// Minimum record length is 4
		if (record_len < 4) {
			throw std::runtime_error("Invalid GDS record (size < 4) found");
		}

		// The size of the additional data in bytes
		buf_size = record_len - 4U;

		buf = new uint8_t[buf_size];

		if (!buf) {
			throw std::runtime_error("Error reading data record");
		}
		bytes_read += fread(buf, 1, buf_size, p_file);

		// Handle the GDS records
		switch (record_type) {
		case GDS_HEADER:
			if (bytes_read != 6)
			{
				// GDH_HEADER should be the first record of a GDS file
				throw std::runtime_error("GDS file should start with HEADER record");
			}
			if (buf && buf_size == 2)
			{
				m_version = uint16_t(buf[0] << 8 | buf[1]);
				if (m_version != 600 && m_version != 6)
				{
					throw std::runtime_error("Unsupported GDS version");
				}
			}
			break;
		case GDS_BGNLIB:
			break;
		case GDS_ENDLIB:
			readEndlib = true;
			break;
		case GDS_LIBNAME:
			{
				// add to libnames after converting to wide

				std::string s((char*)buf, buf_size);

				std::wstring ws = to_wstring(s);



				m_libnames.push_back(ws);
			}
			break;
		case GDS_BGNSTR:
			cell_count++;
			break;
		case GDS_ENDSTR:
			m_cells.push_back(curCell);
			curCell = {};
			break;
		case GDS_UNITS:
			{
				m_uu_per_dbunit = BufReadFloat(buf);
				m_meter_per_dbunit = BufReadFloat(buf + 8);

				if (buf_size == 16)
					memcpy(m_units, buf, 16); // also store the buffer raw data

				break;
			}
		case GDS_STRNAME:
			{
				std::string s((char*)buf, buf_size);



				std::wstring ws = to_wstring(s);

				wcscpy_s(curCell.wstrname, GDS_MAX_STR_NAME, ws.c_str());

				break;
			}
		case GDS_BOUNDARY:
			curElem = BRY;
			break;
		case GDS_PATH:
			curElem = PATH;
			break;
		case GDS_SREF:
			curElem = SREF;
			break;
		case GDS_AREF:
			curElem = AREF;
			break;
		case GDS_TEXT:
			break;
		case GDS_NODE:
			break;
		case GDS_BOX:
			break;
		case GDS_ENDEL:
			// add element to the current structure

			switch (curElem) {
			case BRY:
				curCell.boundaries.push_back(curBndry);
				curBndry = {};
				curElem = NONE;
				break;
			case PATH:
				curCell.paths.push_back(curPath);
				curPath = {};
				curElem = NONE;
				break;
			case SREF:
				curCell.srefs.push_back(curSRef);
				curSRef = {};
				curElem = NONE;
				break;
			case AREF:
				curCell.arefs.push_back(curARef);
				curARef = {};
				curElem = NONE;
				break;
			case NONE:
				break;
			}
			break;
		case GDS_SNAME: // SREF, AREF
			switch (curElem) {
			case SREF:
				{
					std::string s((char*)buf, buf_size);
					std::wstring ws = to_wstring(s);
					wcscpy_s(curSRef.sname, GDS_MAX_STR_NAME, ws.c_str());;
				}
				break;
			case AREF:
				{
					std::string s((char*)buf, buf_size);
					std::wstring ws = to_wstring(s);
					wcscpy_s(curARef.sname, GDS_MAX_STR_NAME, ws.c_str());;
				}
				break;
			case BRY:
			case PATH:
				throw std::runtime_error("Invalid SNAME record");
			case NONE:
				break;
			}
			break;
		case GDS_COLROW: // AREF
			if (curElem == AREF) {
				curARef.col = uint16_t(buf[0] << 8 | buf[1]);
				curARef.row = uint16_t(buf[2] << 8 | buf[3]);
			}
			break;
		case GDS_PATHTYPE:

			if (curElem == PATH) {
				curPath.pathtype = uint16_t(buf[0] << 8 | buf[1]);
			}

			break;
		case GDS_STRANS: // SREF, AREF, TEXT
			switch (curElem) {
			case SREF:
				curSRef.strans = uint16_t(buf[0] << 8 | buf[1]);
				break;
			case AREF:
				curARef.strans = uint16_t(buf[0] << 8 | buf[1]);
				break;
			case BRY:
			case PATH:
				throw std::runtime_error("Invalid STRANS record");
			case NONE:
				break;
			}
			break;
		case GDS_ANGLE: // SREF, AREF, TEXT
			switch (curElem) {
			case SREF:
				curSRef.angle = BufReadFloat(buf);
				break;
			case AREF:
				curARef.angle = BufReadFloat(buf);
				break;
			case BRY:
			case PATH:
				throw std::runtime_error("Invalid ANGLE record");
			case NONE:
				break;
			}
			break;
		case GDS_MAG: // SREF, AREF, TEXT
			switch (curElem) {
			case SREF:
				curSRef.mag = BufReadFloat(buf);
				break;
			case AREF:
				curARef.mag = BufReadFloat(buf);
				break;
			case BRY:
			case PATH:
				throw std::runtime_error("Invalid MAG record");
			case NONE:
				break;
			}
			break;
		case GDS_XY:
			switch (curElem) {
			case BRY:
				{
					// number of pairs
					size_t count = buf_size / 8U;

					if (count >= 8191)
						throw std::runtime_error("Invalid XY record data for BOUNDARY");

					curBndry.size = uint8_t(count);

					// This pointer could be left dangling if there is and error
					// further downs in the GDS file. So we make it a unique pointer
					// and move it later when the element is final when ENDEL is
					// received.
					curBndry.pairs = std::unique_ptr<Pair[]> (new Pair[count]);

					for (unsigned int n = 0; n < count; n++) {
						unsigned int i = 8 * n;
						int x = buf[i] << 24 | buf[i + 1] << 16 | buf[i + 2] << 8 | buf[i + 3];
						int y = buf[i + 4] << 24 | buf[i + 5] << 16 | buf[i + 6] << 8 | buf[i + 7];

						curBndry.pairs[n].x = x;
						curBndry.pairs[n].y = y;
					}
					break;
				}
			case SREF:
				curSRef.x = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
				curSRef.y = buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7];
				break;
			case AREF:
				curARef.x1 = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
				curARef.y1 = buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7];
				curARef.x2 = buf[8] << 24 | buf[9] << 16 | buf[10] << 8 | buf[11];
				curARef.y2 = buf[12] << 24 | buf[13] << 16 | buf[14] << 8 | buf[15];
				curARef.x3 = buf[16] << 24 | buf[17] << 16 | buf[18] << 8 | buf[19];
				curARef.y3 = buf[20] << 24 | buf[21] << 16 | buf[22] << 8 | buf[23];
				break;
			case PATH:
				{
					// number of pairs
					size_t count = buf_size / 8U;

					if (count >= 8191)
						throw std::runtime_error("Invalid XY record data for PATH");

					curPath.size = uint8_t(count);

					//** NEW **//
					curPath.pairs = std::unique_ptr<Pair[]> (new Pair[count]);

					for (unsigned int n = 0; n < count; n++) {
						unsigned int i = 8U * n;
						int x = buf[i] << 24 | buf[i + 1] << 16 | buf[i + 2] << 8 | buf[i + 3];
						int y = buf[i + 4] << 24 | buf[i + 5] << 16 | buf[i + 6] << 8 | buf[i + 7];

						curPath.pairs[n].x = x;
						curPath.pairs[n].y = y;
					}
				}
				break;
			case NONE:
				break;
			}
			break;
		case GDS_LAYER: // BOUNDARY, PATH, TEXT, NODE, BOX
			switch (curElem) {
			case BRY:
				curBndry.layer = uint16_t(buf[0] << 8 | buf[1]);
				break;
			case PATH:
				curPath.layer = uint16_t(buf[0] << 8 | buf[1]);
				break;
			case SREF:
			case AREF:
				throw std::runtime_error("Invalid LAYER record");
			case NONE:
				break;
			}
			break;
		case GDS_WIDTH: // PATH, TEXT

			if (curElem == PATH) {
				curPath.width = uint32_t(buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3]);
			}

			break;
		case GDS_DATATYPE:
			break;
		case GDS_TEXTNODE:
			break;
		case GDS_TEXTTYPE:
			break;
		case GDS_PRESENTATION:
			break;
		case GDS_STRING:
			break;
		case GDS_REFLIBS:
			break;
		case GDS_FONTS:
			break;
		case GDS_ATTRTABLE:
			break;
		case GDS_ELFLAGS:
			break;
		case GDS_PROPATTR:
			break;
		case GDS_PROPVALUE:
			break;
		case GDS_BOXTYPE:
			break;
		case GDS_PLEX:
			break;
		case GDS_BGNEXTN:
			break;
		case GDS_ENDEXTN:
			break;
		case GDS_FORMAT:
			break;
		default:
			throw std::runtime_error("Unknown GDS record type");
		}

		delete[] buf;

		if (readEndlib)
			break;
	}

	if (p_file)
		fclose(p_file);
}

#include <iostream>
Database::~Database() {
	std::cout << "~Database\n";
}

void Database::AllCells(std::vector<std::wstring>& sset)
{
	for (auto it : m_cells)
	{
		sset.push_back(std::wstring(it.wstrname));
	}
}

void Database::CollapseCell(const wchar_t* cell, const double* bounds, uint64_t max_polys, const wchar_t* dest, std::vector<Polygon*>* pset)
{
	Recdata rdata{};
	gds_trans trans{};

	rdata.pset = pset;
	rdata.max_polys = max_polys;
	rdata.gds = this;

	if (!cell)
		throw std::runtime_error("No input cell provided");

	if (dest)
	{
		// 24 bytes needed for GDS_BGNLIB.
		uint8_t access[24] = { 0 };

		_wfopen_s(&rdata.poutfile, dest, L"wb");
		if (!rdata.poutfile)
			throw std::runtime_error("Failure creating file for writing");

		// Write starting records to output GDS file.
		FileAppendShort(rdata.poutfile, GDS_HEADER, 600);
		FileAppendBytes(rdata.poutfile, GDS_BGNLIB, access, 24);
		FileAppendString(rdata.poutfile, GDS_LIBNAME, "");
		FileAppendBytes(rdata.poutfile, GDS_UNITS, (uint8_t*)m_units, 16);
		FileAppendBytes(rdata.poutfile, GDS_BGNSTR, access, 24);
		FileAppendString(rdata.poutfile, GDS_STRNAME, "TOP");
	}

	// Create the bounding box
	if (bounds)
	{
		rdata.usebbox = true;
		if (bounds[2] <= bounds[0] || bounds[3] <= bounds[1])
		{
			throw std::runtime_error("Incorrect bounding box");
		}

		rdata.bbox[0] = { (int)(bounds[0] / m_uu_per_dbunit), (int)(bounds[1] / m_uu_per_dbunit) };
		rdata.bbox[1] = { (int)(bounds[0] / m_uu_per_dbunit), (int)(bounds[3] / m_uu_per_dbunit) };
		rdata.bbox[2] = { (int)(bounds[2] / m_uu_per_dbunit), (int)(bounds[3] / m_uu_per_dbunit) };
		rdata.bbox[3] = { (int)(bounds[2] / m_uu_per_dbunit), (int)(bounds[1] / m_uu_per_dbunit) };
		rdata.bbox[4] = rdata.bbox[0];
	}

	// start the recursion

	Cell* top = FindCell(rdata.gds, cell);

	if (!top)
		throw std::runtime_error("Cell not found");

	Recurse(*top, trans, rdata);

	// write the tail headers to the outfile
	if (dest)
	{
		FileAppendRecord(rdata.poutfile, GDS_ENDSTR);
		FileAppendRecord(rdata.poutfile, GDS_ENDLIB);
	}

	if (rdata.poutfile)
	{
		fclose(rdata.poutfile);
	}
}

void Database::TopCells(std::vector<std::wstring>& sset)
{
	for (auto it = m_cells.begin(); it != m_cells.end(); ++it)
	{
		int is_referenced = 0;
		for (auto it2 = m_cells.begin(); it2 != m_cells.end(); ++it2) {

			for (auto it3 = it2->arefs.begin(); it3 != it2->arefs.end(); ++it3) {
				if (wcscmp(it3->sname, it->wstrname) == 0) {
					is_referenced = 1;
					break;
				}
			}
			for (auto it3 = it2->srefs.begin(); it3 != it2->srefs.end(); ++it3) {
				if (wcscmp(it3->sname, it->wstrname) == 0) {
					is_referenced = 1;
					break;
				}
			}

			// break if the cell is referenced
			if (is_referenced) break;
		}

		if (!is_referenced) {
			sset.push_back(it->wstrname);
		}
	}
}


bool PointInPoly(Pair* poly, int n, Pair p)
{
	// Evaluate if test point P is inside the polygon @poly.
	// Returns true if yes and false if no

	int count, i;

	// counts the segment crossed by a vertical line downwards from test point P
	count = 0;
	for (i = 0; i < n - 1; i++) {

		// Check if segment i will cross a vertical line through the test point
		if (((poly[i].x <= p.x) && (poly[i + 1].x > p.x)) || ((poly[i].x > p.x) && (poly[i + 1].x <= p.x))) {

			// add count if it crosses
			if (p.y < poly[i].y + ((p.x - poly[i].x) * (poly[i + 1].y - poly[i].y)) / (poly[i + 1].x - poly[i].x)) {
				count++;
			}
		}
	}

	// if the coint is off the point is outside the polygon
	return (bool(count % 2));
}