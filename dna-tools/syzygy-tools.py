#------------------------------------------------------------------------
# Copyright (c) 2021 Opal Kelly Incorporated
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 
#------------------------------------------------------------------------

from pathlib import Path
import argparse
import json
from typing import List
from intelhex import IntelHex
import subprocess
from datetime import datetime

parser = argparse.ArgumentParser()

requiredArg = parser.add_argument_group('Required Arguments')
requiredArg.add_argument('-d', '--dna', type=str, required=True, help='The DNA JSON file for the target board')

optionalArg = parser.add_argument_group('Additional Optional Arguments')
optionalArg.add_argument('-s', '--serial', type=str, help='Set the boards serial. In barcode mode this sets the serial prefix. Uses a default date format as the prefix if none is supplied.')
optionalArg.add_argument('-f', '--firmware', type=str, help='The SYZYGY firmware hex file for programming. AVRDUDE is used to program the board.')
optionalArg.add_argument('-b', '--barcode_scan_mode', action='store_true', help='Start barcode scan mode for batch programming. Input the board serial prefix (datecode) as the --serial argument. Every time a serial sticker on a board is scanned the serial will be updated and the board will be programmed.')
optionalArg.add_argument('--dna_binary_out', type=str, help='A filename to write out the DNA data')
optionalArg.add_argument('--sequence_binary_out', type=str, help='A filename to write out the Power Sequence data')
optionalArg.add_argument('--firmware_out', type=str, help='A filename to write out the combined firmware hex to, instead of programming a board')

args = parser.parse_args()


# LIBRARY PARAMETERS
# Constraints that apply to this library itself

# datetime's strftime format
DEFAULT_SERIAL_DATE_FORMAT = "%y%U000"
 
# DNA Data Flash LocationS
DNA_FLASH_OFFSET = 0xC00
# Total space available in the EEPROM
FW_BYTES_EEPROM = 256
# EEPROM Space reserved for non-DNA functionality
FW_RESERVED_BYTES = 9

# SmartVIO version implemented and supported by this library
SVIO_IMPL_VER_MAJOR = 1
SVIO_IMPL_VER_MINOR = 1

# CARRIER-SPECIFIC PARAMETERS
# Complete these constant definitions with those appropriate to your carrier.
# Number of ports on the most populous SmartVIO group, including FPGA constraints.

# Total number of SmartVIO groups. This corresponds to the number of unique
# SmartVIO voltages provided by the carrier.
SVIO_NUM_GROUPS = 3

# Maximum number of SYZYGY ports on a single SmartVIO group for the system.
# The FPGA side of a SYZYGY connection counts as a port here.
SVIO_MAX_PORTS = 6

# Total number of SmartVIO ports in the system.
# The FPGA side of a SYZYGY connection counts as a port here.
SVIO_NUM_PORTS = 6

# Maximum number of SmartVIO ranges definable in the DNA.
SZG_ATTR_LVDS = 0x0001
SZG_ATTR_DOUBLEWIDE = 0x0002
SZG_ATTR_TXR4 = 0x0004

# Maximum number of SmartVIO ranges defined in the DNA header.
SZG_MAX_DNA_RANGES = 4

# Maximum length of an I2C read supported by the DNA firmware.
SZG_MAX_DNA_I2C_READ_LENGTH = 32
SZG_DNA_HEADER_LENGTH_V1 = 40

SZG_DNA_PTR_FULL_LENGTH = 0
SZG_DNA_PTR_HEADER_LENGTH = 2
SZG_DNA_PTR_DNA_MAJOR = 4
SZG_DNA_PTR_DNA_MINOR = 5
SZG_DNA_PTR_DNA_REQUIRED_MAJOR = 6
SZG_DNA_PTR_DNA_REQUIRED_MINOR = 7
SZG_DNA_PTR_MAX_5V_LOAD = 8
SZG_DNA_PTR_MAX_33V_LOAD = 10
SZG_DNA_PTR_MAX_VIO_LOAD = 12
SZG_DNA_PTR_ATTRIBUTES = 14
SZG_DNA_MIN_VIO_RANGE0 = 16
SZG_DNA_MAX_VIO_RANGE0 = 18
SZG_DNA_MIN_VIO_RANGE1 = 20
SZG_DNA_MAX_VIO_RANGE1 = 22
SZG_DNA_MIN_VIO_RANGE2 = 24
SZG_DNA_MAX_VIO_RANGE2 = 26
SZG_DNA_MIN_VIO_RANGE3 = 28
SZG_DNA_MAX_VIO_RANGE3 = 30
SZG_DNA_MANUFACTURER_NAME_LENGTH = 32
SZG_DNA_PRODUCT_NAME_LENGTH = 33
SZG_DNA_PRODUCT_MODEL_LENGTH = 34
SZG_DNA_PRODUCT_VERSION_LENGTH = 35
SZG_DNA_SERIAL_NUMBER_LENGTH = 36
SZG_DNA_CRC16_HIGH = 38
SZG_DNA_CRC16_LOW = 39

# Power Sequence Constants
SEQ_DATA_LENGTH = 9

SEQ_THRESHOLD_OFFSET = 0
SEQ_DELAY_OFFSET = 3
SEQ_ENABLE_CONFIG_OFFSET = 6

THRESHOLD_SCALE = (3300.0 / 256.0)

ENABLE_ACTIVE_LOW = (1 << 3)
ENABLE_DISABLED = (1 << 4)




def main():

    dna_file_path = Path(args.dna)
    if not dna_file_path.is_file():
        print("DNA Json file not found")
        exit(1)
    elif not dna_file_path.suffix == ".json":
        print("DNA file must be JSON file")
        exit(1)

    firmware_file_path = None
    if args.firmware:
        firmware_file_path = Path(args.firmware)
        if not firmware_file_path.is_file():
            print("Firmware file not found")
            exit(1)
        elif not firmware_file_path.suffix == ".hex":
            print("Firmware must be a .hex file")
            exit(1)

    dna_binary_out_path = None
    if args.dna_binary_out:
        dna_binary_out_path = Path(args.dna_binary_out)

    sequence_binary_out_path = None
    if args.sequence_binary_out:
        sequence_binary_out_path = Path(args.sequence_binary_out)

    firmware_out_path = None
    if args.firmware_out:
        firmware_out_path = Path(args.firmware_out)

    serial_prefix = None
    if args.serial:
        serial = args.serial
    else:
        # Auto generate the serial prefix when not provided
        serial = datetime.now().strftime(DEFAULT_SERIAL_DATE_FORMAT)
        serial_prefix = serial

    barcode_scan_mode = args.barcode_scan_mode

    if barcode_scan_mode and not firmware_file_path:
        print("Programming requires Firmware hex file argument")
        exit(1)

    # If barcode scan mode was set, wait for input and program a board with every barcode scan
    # Else break at the end of the loop and run through the process only once
    while True:
        if barcode_scan_mode:
            print("\r\nBarcode Serial Scan Program Mode")

        if serial_prefix:
            print(f"DNA: {dna_file_path}")
            print(f"Serial Prefix: {serial_prefix}")
            print("Scan serial label barcode or type serial to complete the full serial value (value is case insensitive. q to exit)")
            label_serial = input(f"SERIAL: {serial_prefix}")
            if label_serial == 'q':
                break

            serial = f"{serial_prefix}{label_serial.upper()}"
        
        print(f"DNA Serial: {serial}")
    
        # Genrate the DNA Data
        dna_data = generate_dna_data(dna_file_path, serial)

        # Print DNA Data
        print("SYZYGY DNA Data:")
        debug_print(dna_data)


        # Optional DNA data file write
        if dna_binary_out_path:
            dna_binary_out_path.write_bytes(bytearray(dna_data))

        
        # Generate Power Sequence Data
        sequence_data = generate_power_sequence_data(dna_file_path)

        if sequence_data:
            # Print Power Sequence Data
            print("Power Sequence Data:")
            debug_print(sequence_data)

            # Optional Power Sequence data file write
            if sequence_binary_out_path:
                sequence_binary_out_path.write_bytes(bytearray(sequence_data))
        else:
            print("No Power Sequence Data Found")


        # Program the applicable flash and EEPROM data
        if firmware_file_path:

            # Generate the binary data
            firmware_hex_file = "fw_temp.hex"
            generate_combined_firmware_file(firmware_file_path, dna_data, firmware_hex_file)

            # Generate the sequence EEPROM data if defined
            eeprom_hex_filename = None
            if sequence_data:
                eeprom_hex_filename = f"eeprom_temp.eep"
                generate_eeprom_file(sequence_data, eeprom_hex_filename)


            if not firmware_out_path:
                # Program the board
                program(firmware_hex_file, eeprom_hex_filename)

            
            # Remove the temp binary data files
            if firmware_hex_file:
                if firmware_out_path:
                    # Rename the generated firmware as the defined output file
                    Path(firmware_hex_file).replace(firmware_out_path)
                else:
                    Path(firmware_hex_file).unlink(missing_ok=True)
            if eeprom_hex_filename:
                Path(eeprom_hex_filename).unlink(missing_ok=True)
            

            
        # Only loop if in barcode scan mode
        if not barcode_scan_mode:
            break
        else:
           print(f"Finished Programming with serial: {serial}")

    
    print("DONE")


def program(firmware_hex_file, eeprom_hex_filename=None):

    # Program the appropriate fuse bits
    program_cmd = "avrdude -pt44 -catmelice_isp -B 20 -Pusb -u -Uefuse:w:0xFE:m -Uhfuse:w:0xDD:m -Ulfuse:w:0xE2:m"
    program_return = subprocess.call(program_cmd, shell=True)

    if program_return != 0:
        print("Error Setting Fuse Bits")
        exit(1)
    else:
        # Load the firmware and DNA
        program_cmd = f"avrdude -pt44 -catmelice_isp -B1 -Pusb -e -Uflash:w:{firmware_hex_file}:i"
        program_return = subprocess.call(program_cmd, shell=True)

        if program_return != 0:
            print("Error Programming Flash")
            exit(1)
        else:

            if eeprom_hex_filename:
                # Load the EEPROM power sequence data
                program_cmd = f"avrdude -pt44 -catmelice_isp -B1 -Pusb -Ueeprom:w:{eeprom_hex_filename}:i"
                program_return = subprocess.call(program_cmd, shell=True)

                if program_return != 0:
                    print("Error Programming EEPROM")
                    exit(1)



def generate_combined_firmware_file(firmware_file_path: Path, dna_data: List[int], filename: str):
    # Read the existing firmware binary
    firmware_hex = IntelHex()
    firmware_hex.fromfile(firmware_file_path, format='hex')

    # Add in the DNA data at the set flash location
    address = DNA_FLASH_OFFSET
    for value in dna_data:
        if firmware_hex[address] != 0xFF:
            print("ERROR: The firmware file already has data at the address for DNA storage. Firmware to large?")
            quit(1)

        firmware_hex[address] = value
        address += 1
    
    # Write the new complete flash file
    firmware_hex.write_hex_file(filename)


def generate_eeprom_file(sequence_data: List[int], filename: str):
    eeprom_hex = IntelHex()

    # Fill the start of the EEPROM file
    for i in range(0, (FW_BYTES_EEPROM - FW_RESERVED_BYTES)):
        eeprom_hex[i] = 0xFF

    # Write the Sequence Data
    address = FW_BYTES_EEPROM - FW_RESERVED_BYTES
    for value in sequence_data:
        eeprom_hex[address] = value
        address += 1

    # Write the EEPROM file
    eeprom_hex.write_hex_file(filename)
    

def generate_dna_data(dna_file_path: Path, serial: str) -> List[int]:
    with dna_file_path.open() as dna_file:
        dna_json = json.load(dna_file)
   
    dna_data = [0] * SZG_DNA_HEADER_LENGTH_V1

    dna_data[SZG_DNA_PTR_HEADER_LENGTH] = SZG_DNA_HEADER_LENGTH_V1 & 0xFF
    dna_data[SZG_DNA_PTR_HEADER_LENGTH + 1] = ((SZG_DNA_HEADER_LENGTH_V1 & 0xFF00) >> 8)

    dna_data[SZG_DNA_PTR_DNA_MAJOR] = SVIO_IMPL_VER_MAJOR
    dna_data[SZG_DNA_PTR_DNA_MINOR] = SVIO_IMPL_VER_MINOR

    # By default the DNA should be compatible with any version of the spec.
    min_ver_major = 0
    min_ver_minor = 0

    # TXR4 peripherals require version > 1.1
    if "is_txr4" in dna_json and dna_json["is_txr4"] == True:
        if min_ver_major < 1:
            min_ver_major = 1
            min_ver_minor = 1
        elif min_ver_major == 1 and min_ver_minor < 1:
            min_ver_minor = 1

    dna_data[SZG_DNA_PTR_DNA_REQUIRED_MAJOR] = min_ver_major
    dna_data[SZG_DNA_PTR_DNA_REQUIRED_MINOR] = min_ver_minor

    dna_data[SZG_DNA_PTR_MAX_5V_LOAD] = dna_json["max_5v_load"] & 0xFF
    dna_data[SZG_DNA_PTR_MAX_5V_LOAD + 1] = dna_json["max_5v_load"] >> 8
    dna_data[SZG_DNA_PTR_MAX_33V_LOAD] = dna_json["max_3v3_load"] & 0xFF
    dna_data[SZG_DNA_PTR_MAX_33V_LOAD + 1] = dna_json["max_3v3_load"] >> 8
    dna_data[SZG_DNA_PTR_MAX_VIO_LOAD] = dna_json["max_vio_load"] & 0xFF
    dna_data[SZG_DNA_PTR_MAX_VIO_LOAD + 1] = dna_json["max_vio_load"] >> 8

    dna_data[SZG_DNA_PTR_ATTRIBUTES] |= SZG_ATTR_LVDS if dna_json["is_lvds"] == True else 0
    dna_data[SZG_DNA_PTR_ATTRIBUTES] |= SZG_ATTR_DOUBLEWIDE if dna_json["is_doublewide"] == True else 0

    if "is_txr4" in dna_json:
        dna_data[SZG_DNA_PTR_ATTRIBUTES] |= SZG_ATTR_TXR4 if dna_json["is_txr4"] == True else 0

    # Run through each range and set the values accordingly
    for i in range(0, SZG_MAX_DNA_RANGES):
        vio = dna_json["vio"][i]["min"]
        dna_data[SZG_DNA_MIN_VIO_RANGE0 + 4*i] = vio & 0xFF
        dna_data[SZG_DNA_MIN_VIO_RANGE0 + (4*i) + 1] = (vio & 0xFF00) >> 8

        vio = dna_json["vio"][i]["max"]
        dna_data[SZG_DNA_MAX_VIO_RANGE0 + 4*i] = vio & 0xFF
        dna_data[SZG_DNA_MAX_VIO_RANGE0 + (4*i) + 1] = (vio & 0xFF00) >> 8

    manuf_len = len(dna_json["manufacturer_name"])
    name_len = len(dna_json["product_name"])
    model_len = len(dna_json["product_model"])
    version_len = len(dna_json["product_version"])
    serial_len = len(serial)

    dna_data[SZG_DNA_MANUFACTURER_NAME_LENGTH] = manuf_len
    dna_data[SZG_DNA_PRODUCT_NAME_LENGTH] = name_len
    dna_data[SZG_DNA_PRODUCT_MODEL_LENGTH] = model_len
    dna_data[SZG_DNA_PRODUCT_VERSION_LENGTH] = version_len
    dna_data[SZG_DNA_SERIAL_NUMBER_LENGTH] = serial_len

    dna_length = SZG_DNA_HEADER_LENGTH_V1 + manuf_len + name_len + model_len + version_len + serial_len

    dna_data[SZG_DNA_PTR_FULL_LENGTH] = dna_length & 0xFF
    dna_data[SZG_DNA_PTR_FULL_LENGTH + 1] = (dna_length & 0xFF00) >> 8

    # Calculate and insert the CRC once all data has been gathered
    crc = szg_compute_crc(dna_data)

    dna_data[SZG_DNA_CRC16_HIGH] = (crc & 0xFF00) >> 8
    dna_data[SZG_DNA_CRC16_LOW] = crc & 0xFF

    dna_data.extend(bytes(dna_json["manufacturer_name"], "utf-8"))
    dna_data.extend(bytes(dna_json["product_name"], "utf-8"))
    dna_data.extend(bytes(dna_json["product_model"], "utf-8"))
    dna_data.extend(bytes(dna_json["product_version"], "utf-8"))
    
    dna_data.extend(bytes(serial, "utf-8"))

    return dna_data


def generate_power_sequence_data(dna_file_path: Path) -> List[int]:
    with dna_file_path.open() as dna_file:
        dna_json = json.load(dna_file)

    if not "sequencer_enable_config" in dna_json:
        return None
   
    sequence_data = [0] * SEQ_DATA_LENGTH

    for i in range(0, 3):
        sequence_data[i + SEQ_THRESHOLD_OFFSET] = min(int(dna_json["sequencer_threshold_mv"][i] / THRESHOLD_SCALE), 0xFF)

        if dna_json["sequencer_enable_config"][i]["enabled"]:

            # Handle Enable output delay times
            delay = int(dna_json["sequencer_enable_config"][i]["delay_ms"] / 10)
            if delay < 0 or delay > 255:
                print("ERROR: Power Sequence Delay Value Out of Range")
                exit(1)

            sequence_data[i + SEQ_DELAY_OFFSET] = delay

            # Handle Enable output configuration
            temp_enable_config = 0
            temp_enable_config |= dna_json["sequencer_enable_config"][i]["input_dependency"]
            temp_enable_config |= 0 if dna_json["sequencer_enable_config"][i]["active_high"] else ENABLE_ACTIVE_LOW
            temp_enable_config |= 0 if dna_json["sequencer_enable_config"][i]["enabled"] else ENABLE_DISABLED
            
            sequence_data[i + SEQ_ENABLE_CONFIG_OFFSET] = temp_enable_config

        else:
            sequence_data[i + SEQ_DELAY_OFFSET] = 0xFF
            sequence_data[i + SEQ_ENABLE_CONFIG_OFFSET] = 0xFF

    return sequence_data


def szg_compute_crc(data):
    crc = 0xffff

    for i in range(0, len(data) - 2):
        x = (crc >> 8) ^ data[i]
        x ^= x>>4
        crc = (crc<<8) ^ (x<<12) ^ (x<<5) ^ (x)
        crc &= 0xffff
    
    return crc


def debug_print(data):
    line = ""
    index = 0
    for data in data:
        line += f"{data:02X} "
        if index == 15:
            print(line)
            line = ""
            index = 0
        else:
            index += 1
    print(line)
    

if __name__ == "__main__":
    main()