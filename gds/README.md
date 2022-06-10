# gdscollapse

A small C library for reading a GDSII file and collapse a cell in it to just simple polygons.

The library defines a stucture `struct gds_database` which can be created from a GDS file by the function `gds_db_new(...)`.

The function `gds_collapse(...)` can be used to collapse (flatten) a cell in the created `struct gds_database` and write it to an
output GDS file with only polygons, or in memory to a polygon set.

A polygon set is a array of pointers consisting of elements:

```
struct gds_poly {
	struct gds_ipair* pairs;
	uint16_t size;
	uint16_t layer;
};
```
The `size` element is the number on `int` pairs in the polygon and the `layer' the layer in the GDS database the polygon belong.

Details are provide in the header file gds.h and main.c contains an example.
