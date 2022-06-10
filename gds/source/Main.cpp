/**
* Copyright(c) 2022, Jan Willem Bos - janwillembos@yahoo.com
* All rights reserved.
*
* This source code is licensed under the BSD - style license found in the
* LICENSE file in the root directory of this source tree.
*/

#include "Gds.h"

#include <cwchar>
#include <stdexcept>
#include <iostream>

int wmain()
{
	/*
	// Example use of the Database class to collapse a cell in a GDS file and
	// write to an newly created output GDS file and/or a set of Polygons.
	*/ 


	try
	{
		// Create a gds database from the example GDS file "nand.gds".
		GDS::Database gds = GDS::Database(L"nand.gds");

		/*
		// Write all the top cells to a vector of strings
		*/
		std::vector<std::wstring> cellNames;
		gds.TopCells(cellNames);
		std::wcout << L"Top cells in GDS file " <<  gds.filePath << L":" << std::endl;
		for (std::wstring it : cellNames)
		{
			std::wcout << it << std::endl;
		}
		std::wcout << std::endl;

		gds.AllCells(cellNames);
		std::wcout << L"All cells in GDS file " << gds.filePath << L":" << std::endl;
		for (std::wstring it : cellNames)
		{
			std::wcout << it << std::endl;
		}
		std::wcout << std::endl;

		/*
		// Collapse cell "NAND" and write the resulting polygons to a output file
		// and a polygon vector.
		*/

		// Declare and initialize the cell to collapse.
		std::wstring cell(L"NAND");

		// Enter the bounding box of the part of the GDS cell to output
		// bottom/left x = 28.7 and y = 45.2 followed by top/right x = 50.0 and y = 85.5
		// in GDS user units (usually micron). Or enter a null pointer if no bounding
		// is to be used. This could for a typical GDS cell create many polygons so
		// do with care.
		double bounds[4] = { 28.7, 45.2, 50.0, 85.5 };

		// Set an upper limit to the number of polys to output.
		uint64_t maxPolys = (uint64_t)10E+6;

		// Declare and initialize the output file name. You can also pass a null pointer
		// in which case no output file will be created.
		std::wstring outFile(L"out.gds");

		// Declare and initialize the vector of polygon pointers to write to. You can
		// also pass a null pointer.
		std::vector<GDS::Polygon*> pset;

		// Finally, collapse the cell.
		gds.CollapseCell(cell.c_str(), bounds, maxPolys, outFile.c_str(), &pset);

		std::wcout << L"A total of " << std::to_wstring(pset.size()) << L" polygons were written to GDS file " <<
			outFile << L" and the polygon set" << std::endl;

		// Deallocate the memory of the Polygon pointers.
		for (auto it = pset.begin(); it != pset.end(); ++it)
		{
			delete* it;
		}
	}
	catch (const std::runtime_error& e)
	{
		std::cout << e.what() << std::endl;
	}

	return 0;
}
