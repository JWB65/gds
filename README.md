# A tool for working with GDSII files

A small C++ class for reading a GDSII file and collapse a cell in it to just simple polygons.

The library defines a class named `GDS::Database` with is contructed gds database objects
from a GDS file.

The member function `Collapse` can be used to collapse (flatten) a cell in the
database object and output it to an output file and/or a std::vector of polygons.

The `Main.cpp` file is an example of its use.
