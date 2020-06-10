// Sequencer Generator v1.0
//
// Tool used to generate a binary file containing a set of register settings
// used to configure the power supply sequencing feature of the official
// SYZYGY AVR firmware. A JSON file is taken as input, this file can contain
// other data fields and be combined with the JSON file used to generate the
// primary DNA blob. Please refer to the szg-sensor.json file for an example
// of the available fields.
//
//------------------------------------------------------------------------
// Copyright (c) 2020 Opal Kelly Incorporated
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

using json = nlohmann::json;

#define SEQ_THRESHOLD_OFFSET 0
#define SEQ_DELAY_OFFSET 3
#define SEQ_ENABLE_CONFIG_OFFSET 6

#define THRESHOLD_SCALE (3.3 / 256)

#define ENABLE_ACTIVE_LOW (1 << 3)
#define ENABLE_DISABLED (1 << 4)

int main(int argc, char *argv[])
{
	// The SYZYGY AVR Firmware sequencer configuration data is stored in 9
	// bytes in the AVR EEPROM.
	std::vector<uint8_t> sequencer_data(9);

	uint8_t temp_enable_config;

	// Print usage if there are not enough or too many arguments
	if ((argc < 3) || (argc > 3)) {
		std::cout << "USAGE: " << argv[0];
		std::cout << " in_file out_file\n";
		return 0;
	}

	std::ifstream input_file(argv[1]);
	std::ofstream output_file(argv[2]);

	json sequencer_json;

	// Parse the input data
	input_file >> sequencer_json;

	for (int i = 0; i < 3; i++) {
		// Handle Sequencer Threshold values
		sequencer_data[i + SEQ_THRESHOLD_OFFSET] =
			(uint8_t) (sequencer_json["sequencer_threshold"][i].get<float>() / THRESHOLD_SCALE);

		// Handle Enable output delay times
		// TODO: Check bounds and error...
		sequencer_data[i + SEQ_DELAY_OFFSET] =
			sequencer_json["sequencer_enable_config"][i]["delay"].get<uint8_t>();

		// Handle Enable output configuration
		temp_enable_config = 0;
		temp_enable_config |=
			sequencer_json["sequencer_enable_config"][i]["input_dependency"].get<uint8_t>();

		temp_enable_config |=
			sequencer_json["sequencer_enable_config"][i]["active_high"].get<bool>() ? ENABLE_ACTIVE_LOW : 0;

		temp_enable_config |=
			sequencer_json["sequencer_enable_config"][i]["enabled"].get<bool>() ? 0 : ENABLE_DISABLED;
		
		sequencer_data[i + SEQ_ENABLE_CONFIG_OFFSET] = temp_enable_config;
	}

	for (int i = 0; i < 9; i++) {
		output_file << sequencer_data[i];
	}

	return 0;
}

