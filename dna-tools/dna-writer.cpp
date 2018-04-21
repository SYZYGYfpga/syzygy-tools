// DNA Generator v1.0
//
// An application that can be used to create binary SYZYGY DNA blobs that
// can be programmed to SYZYGY peripherals. A JSON-formatted input file
// provides the DNA content. Some examples are distributed with this
// software.
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

#include <fstream>
#include <iostream>
#include <vector>
#include <stdint.h>
#include "json.hpp"

extern "C" {
#include "syzygy.h"
}

// Target version of the SYZYGY DNA specification for this application
#define DNA_SPEC_MAJOR 1
#define DNA_SPEC_MINOR 0

using json = nlohmann::json;

int main(int argc, char *argv[])
{
	std::vector<uint8_t> dna_header(SZG_DNA_HEADER_LENGTH_V1);

	// Print usage if there are not enough or too many arguments
	if ((argc < 3) || (argc > 4)) {
		std::cout << "USAGE: " << argv[0];
		std::cout << " in_file out_file [serial]\n";
		std::cout << "  If no serial is provided it will be ";
		std::cout << "extracted from the JSON input file\n";
		return 0;
	}

	std::ifstream input_file(argv[1]);
	std::ofstream output_file(argv[2]);

	json dna_json;

	// Parse the input data
	input_file >> dna_json;

	dna_header[SZG_DNA_PTR_HEADER_LENGTH] = SZG_DNA_HEADER_LENGTH_V1 & 0xFF;
	dna_header[SZG_DNA_PTR_HEADER_LENGTH + 1] =
	    (SZG_DNA_HEADER_LENGTH_V1 & 0xFF00) >> 8;

	dna_header[SZG_DNA_PTR_DNA_MAJOR] = DNA_SPEC_MAJOR;
	dna_header[SZG_DNA_PTR_DNA_MINOR] = DNA_SPEC_MINOR;

	// As of now there are no incompatibilities, so any dna parser should
	// work.
	dna_header[SZG_DNA_PTR_DNA_REQUIRED_MAJOR] = 0;
	dna_header[SZG_DNA_PTR_DNA_REQUIRED_MINOR] = 0;

	dna_header[SZG_DNA_PTR_MAX_5V_LOAD] = dna_json["max_5v_load"].get<uint16_t>() & 0xFF;
	dna_header[SZG_DNA_PTR_MAX_5V_LOAD + 1] = dna_json["max_5v_load"].get<uint16_t>() >> 8;
	dna_header[SZG_DNA_PTR_MAX_33V_LOAD] = dna_json["max_3v3_load"].get<uint16_t>() & 0xFF;
	dna_header[SZG_DNA_PTR_MAX_33V_LOAD + 1] = dna_json["max_3v3_load"].get<uint16_t>() >> 8;
	dna_header[SZG_DNA_PTR_MAX_VIO_LOAD] = dna_json["max_vio_load"].get<uint16_t>() & 0xFF;
	dna_header[SZG_DNA_PTR_MAX_VIO_LOAD + 1] = dna_json["max_vio_load"].get<uint16_t>() >> 8;

	dna_header[SZG_DNA_PTR_ATTRIBUTES] |= dna_json["is_lvds"].get<bool>() ?
	    SZG_ATTR_LVDS : 0;
	dna_header[SZG_DNA_PTR_ATTRIBUTES] |= dna_json["is_doublewide"].get<bool>() ?
	    SZG_ATTR_DOUBLEWIDE : 0;

	// Run through each range and set the values accordingly
	for (int i = 0; i < SZG_MAX_DNA_RANGES - 1; i++) {
		uint16_t vio;
		vio = dna_json["vio"][i]["min"].get<uint16_t>();
		dna_header[SZG_DNA_MIN_VIO_RANGE0 + 4*i] = vio & 0xFF;
		dna_header[SZG_DNA_MIN_VIO_RANGE0 + (4*i) + 1] = (vio & 0xFF00) >> 8;

		vio = dna_json["vio"][i]["max"].get<uint16_t>();
		dna_header[SZG_DNA_MAX_VIO_RANGE0 + 4*i] = vio & 0xFF;
		dna_header[SZG_DNA_MAX_VIO_RANGE0 + (4*i) + 1] = (vio & 0xFF00) >> 8;
	}

	uint8_t manuf_len =
	    dna_json["manufacturer_name"].get<std::string>().length();

	uint8_t name_len =
	    dna_json["product_name"].get<std::string>().length();

	uint8_t model_len =
	    dna_json["product_model"].get<std::string>().length();

	uint8_t version_len =
	    dna_json["product_version"].get<std::string>().length();

	uint8_t serial_len = 0;

	if (argc == 4) {
		serial_len = strlen(argv[3]);
	} else {
		serial_len = dna_json["serial"].get<std::string>().length();
	}

	dna_header[SZG_DNA_MANUFACTURER_NAME_LENGTH] = manuf_len;

	dna_header[SZG_DNA_PRODUCT_NAME_LENGTH] = name_len;

	dna_header[SZG_DNA_PRODUCT_MODEL_LENGTH] = model_len;

	dna_header[SZG_DNA_PRODUCT_VERSION_LENGTH] = version_len;

	dna_header[SZG_DNA_SERIAL_NUMBER_LENGTH] = serial_len;

	uint16_t dna_length = SZG_DNA_HEADER_LENGTH_V1 + manuf_len + name_len +
	                      model_len + version_len + serial_len;

	dna_header[SZG_DNA_PTR_FULL_LENGTH] = dna_length & 0xFF;
	dna_header[SZG_DNA_PTR_FULL_LENGTH + 1] = (dna_length & 0xFF00) >> 8;

	// Calculate and insert the CRC once all data has been gathered
	uint16_t crc = szgComputeCRC(&dna_header[0], SZG_DNA_HEADER_LENGTH_V1 - 2);

	dna_header[SZG_DNA_CRC16_HIGH] = (crc & 0xFF00) >> 8;
	dna_header[SZG_DNA_CRC16_LOW] = crc & 0xFF;

	for (int i = 0; i < SZG_DNA_HEADER_LENGTH_V1; i++) {
		output_file << dna_header[i];
	}

	output_file << dna_json["manufacturer_name"].get<std::string>();
	output_file << dna_json["product_name"].get<std::string>();
	output_file << dna_json["product_model"].get<std::string>();
	output_file << dna_json["product_version"].get<std::string>();

	if (argc == 4) {
		output_file << argv[3];
	} else {
		output_file << dna_json["serial"].get<std::string>();
	}

	return 0;
}

