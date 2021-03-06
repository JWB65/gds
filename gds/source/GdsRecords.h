#pragma once

enum gds_record {
	GDS_HEADER = 0x0002,
	GDS_BGNLIB = 0x0102,
	GDS_LIBNAME = 0x0206,
	GDS_UNITS = 0x0305,
	GDS_ENDLIB = 0x0400,
	GDS_BGNSTR = 0x0502,
	GDS_STRNAME = 0x0606,
	GDS_ENDSTR = 0x0700,
	GDS_BOUNDARY = 0x0800,
	GDS_PATH = 0x0900,
	GDS_SREF = 0x0a00,
	GDS_AREF = 0x0b00,
	GDS_TEXT = 0x0c00,
	GDS_LAYER = 0x0d02,
	GDS_DATATYPE = 0x0e02,
	GDS_WIDTH = 0x0f03,
	GDS_XY = 0x1003,
	GDS_ENDEL = 0x1100,
	GDS_SNAME = 0x1206,
	GDS_COLROW = 0x1302,
	GDS_TEXTNODE = 0x1400,
	GDS_NODE = 0x1500,
	GDS_TEXTTYPE = 0x1602,
	GDS_PRESENTATION = 0x1701,
	GDS_STRING = 0x1906,
	GDS_STRANS = 0x1a01,
	GDS_MAG = 0x1b05,
	GDS_ANGLE = 0x1c05,
	GDS_REFLIBS = 0x1f06,
	GDS_FONTS = 0x2006,
	GDS_PATHTYPE = 0x2102,
	GDS_GENERATIONS = 0x2202,
	GDS_ATTRTABLE = 0x2306,
	GDS_ELFLAGS = 0x2601,
	GDS_NODETYPE = 0x2a02,
	GDS_PROPATTR = 0x2b02,
	GDS_PROPVALUE = 0x2c06,
	GDS_BOX = 0x2d00,
	GDS_BOXTYPE = 0x2e02,
	GDS_PLEX = 0x2f03,
	GDS_BGNEXTN = 0x3003,
	GDS_ENDEXTN = 0x3103,
	GDS_FORMAT = 0x3602
};