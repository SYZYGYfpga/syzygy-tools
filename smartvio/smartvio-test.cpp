// SmartVIO Tester
//
// This is a quick test application to ensure that the SmartVIO solver
// functions correctly for a series of different test ranges.
//
//------------------------------------------------------------------------
// Copyright (c) 2018 Opal Kelly Incorporated
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 
//------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fstream>

#define CATCH_CONFIG_MAIN // Have Catch handle our main()
#include "catch.hpp"

extern "C" {
#include "syzygy.h"
}

#define TEST_BLOB_DIR "test-dna-blobs/"

// Fictional test board setup:
//   Group 1:
//     Port 1: 0x30
//     Potential for double-wide with Group2/Port2
//   Group 2:
//     Port 2: 0x31
//     Potential for double-wide with Group1/Port1
//   Group 3:
//     Port 3: 0x32

// Unfortunately we can't used designated initializers in c++...
szgSmartVIOConfig default_svio = {
	// Ports,       Groups,         Results, Group Masks
	SVIO_NUM_PORTS, SVIO_NUM_GROUPS, {0,0}, {0x1, 0x2, 0x4}, {
		{
			// Group 1
			0x00, // i2c_addr
			1,    // present
			0,    // group
			0x00, // attr
			0,    // doublewide_mate
			1,    // range_count
			{ {120, 330}, {0,0}, {0,0}, {0,0} } // ranges
		}, {
			0x30, // i2c_addr
			0,    // present
			0,    // group
			0x00, // attr
			1,    // doublewide_mate
			0,    // range_count
			{ {0,0}, {0,0}, {0,0}, {0,0} } // ranges
		}, {
			// Group 2
			0x00, // i2c_addr
			1,    // present
			1,    // group
			0x00, // attr
			1,    // doublewide_mate
			1,    // range_count
			{ {120, 330}, {0,0}, {0,0}, {0,0} } // ranges
		}, {
			0x31, // i2c_addr
			0,    // present
			1,    // group
			0x00, // attr
			0,    // doublewide_mate
			0,    // range_count
			{ {0,0}, {0,0}, {0,0}, {0,0} } // ranges
		}, {
			// Group 3
			0x00, // i2c_addr
			1,    // present
			2,    // group
			0x00, // attr
			2,    // doublewide_mate
			1,    // range_count
			{ {120, 330}, {0,0}, {0,0}, {0,0} } // ranges
		}, {
			0x32, // i2c_addr
			0,    // present
			2,    // group
			0x00, // attr
			2,    // doublewide_mate
			0,    // range_count
			{ {0,0}, {0,0}, {0,0}, {0,0} } // ranges
		}
	}
};

// Simple test case to check that an unpopulated group solves correctly
TEST_CASE( "No Peripherals Inserted", "[smartvio]" ) {
	szgSmartVIOConfig svio = default_svio;

	for (int i = 0; i < SVIO_NUM_GROUPS; i++) {
		REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[i]) == 120);
	}
}

// Basic test, read in a DNA blob for a POD-PMOD on port 1 and solve
TEST_CASE( "Basic SmartVIO Test", "[smartvio]" ) {
	// Read in POD-PMOD DNA Blob
	std::ifstream dna_blob(std::string(TEST_BLOB_DIR) + std::string("szg-pmod.bin"), std::ifstream::in);
	unsigned char dna_header[SZG_DNA_HEADER_LENGTH_V1 + 1];
	szgSmartVIOConfig svio = default_svio;

	dna_blob.read((char *)dna_header, SZG_DNA_HEADER_LENGTH_V1);

	REQUIRE(szgParsePortDNA(1, &svio, dna_header, SZG_DNA_HEADER_LENGTH_V1) == 0);

	// POD-PMOD requires VIO == 3.3V on group 1
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[0]) == 330);
	// Other groups should remain at the default 1.2V
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[1]) == 120);
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[2]) == 120);
}

// Doublewide test, read in a DNA blob for a doublewide test pod on port 1
//  and solve.
TEST_CASE( "Doublewide Group Test", "[smartvio]" ) {
	// Read in doublewide test DNA Blob
	std::ifstream dna_blob(std::string(TEST_BLOB_DIR) + std::string("szg-tst-doublewide.bin"), std::ifstream::in);
	unsigned char dna_header[SZG_DNA_HEADER_LENGTH_V1 + 1];
	szgSmartVIOConfig svio = default_svio;

	dna_blob.read((char *)dna_header, SZG_DNA_HEADER_LENGTH_V1);

	REQUIRE(szgParsePortDNA(1, &svio, dna_header, SZG_DNA_HEADER_LENGTH_V1) == 0);

	// Groups 1 and 2 should be at 1.8V for the doublewide test pod
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[0]) == 180);
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[1]) == 180);
	// Group 3 should remain at the default 1.2V
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[2]) == 120);
}

// Failing doublewide test, read in a DNA blob for a doublewide test pod on
//  port 1 and solve. The FPGA-side of port 2 is modified to cause a failure
TEST_CASE( "Failing Doublewide Group Test", "[smartvio]" ) {
	// Read in doublewide test DNA Blob
	std::ifstream dna_blob(std::string(TEST_BLOB_DIR) + std::string("szg-tst-doublewide.bin"), std::ifstream::in);
	unsigned char dna_header[SZG_DNA_HEADER_LENGTH_V1 + 1];
	szgSmartVIOConfig svio = default_svio;

	dna_blob.read((char *)dna_header, SZG_DNA_HEADER_LENGTH_V1);

	// Modify FPGA-side restrictions on port 2 to cause a failure
	svio.ports[2].ranges[0].min = 120;
	svio.ports[2].ranges[0].max = 120;

	REQUIRE(szgParsePortDNA(1, &svio, dna_header, SZG_DNA_HEADER_LENGTH_V1) == 0);

	// Groups 1 and 2 should be at 1.8V for the doublewide test pod
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[0]) == -1);
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[1]) == -1);
	// Group 3 should remain at the default 1.2V
	REQUIRE(szgSolveSmartVIOGroup(svio.ports, svio.group_masks[2]) == 120);
}

// Test to ensure that a solution is found even with a complex set of ranges
//  with minimal overlap of the target VIO. Note that this does not test
//  parsing of DNA, just the solver.
TEST_CASE( "Advanced SmartVIO Test", "[smartvio]" ) {
	szgSmartVIOConfig svio = default_svio;

	// More advanced test, multiple ranges, less overlap
	// - FPGA  = 090 - 100, 150 - 180, 250 - 330
	// - Port1 = 110 - 130, 180 - 200, 250 - 330
	svio.ports[0].ranges[0].min = 90;
	svio.ports[0].ranges[0].max = 100;
	svio.ports[0].ranges[1].min = 150;
	svio.ports[0].ranges[1].max = 180;
	svio.ports[0].ranges[2].min = 250;
	svio.ports[0].ranges[2].max = 330;
	svio.ports[0].range_count = 3;
	svio.ports[1].ranges[0].min = 110;
	svio.ports[1].ranges[0].max = 130;
	svio.ports[1].ranges[1].min = 180;
	svio.ports[1].ranges[1].max = 200;
	svio.ports[1].ranges[2].min = 250;
	svio.ports[1].ranges[2].max = 330;
	svio.ports[1].range_count = 3;
	svio.ports[1].present = 1;

	REQUIRE(szgSolveSmartVIOGroup(svio.ports, 0x1) == 180);
}

// Ensure that no solution is found for two different failing smartVIO
//  setups. Note that this does not test parsing of DNA, just the solver.
TEST_CASE( "Failing SmartVIO Tests", "[smartvio]" ) {
	szgSmartVIOConfig svio = default_svio;

	// Failing - First Range Lower
	// - FPGA  = 120 - 180
	// - Port1 = 250 - 330
	svio.ports[0].ranges[0].min = 120;
	svio.ports[0].ranges[0].max = 180;
	svio.ports[1].ranges[0].min = 250;
	svio.ports[1].ranges[0].max = 330;
	svio.ports[1].present = 1;

	REQUIRE(szgSolveSmartVIOGroup(svio.ports, 0x1) == -1);

	// Failing - First Range Higher
	// - FPGA  = 250 - 330
	// - Port1 = 120 - 180
	svio.ports[0].ranges[0].min = 250;
	svio.ports[0].ranges[0].max = 330;
	svio.ports[1].ranges[0].min = 120;
	svio.ports[1].ranges[0].max = 180;
	svio.ports[1].present = 1;

	REQUIRE(szgSolveSmartVIOGroup(svio.ports, 0x1) == -1);
}

