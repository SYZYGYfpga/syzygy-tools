# SYZYGY Tools

## Overview

The SYZYGY Tools package provides a suite of applications to assist developers
of [SYZYGY](http://syzygyfpga.io/) compatible carriers and peripherals.

These tools include:

- syzygy.(h,c) - A library that can be used to parse SYZYGY DNA data and
determine an appropriate VIO voltage for a set of peripherals.

- smartvio-test.cpp - A test suite for the syzygy.(h,c) library.

- dna-writer - An application for generating SYZYGY DNA blobs that can be
written to peripheral MCUs.

### SmartVIO Library

The SmartVIO library consists of a C header file (`syzygy.h`) along with
C source code (`syzygy.c`). The library includes a set of useful definitions
and structures for working with DNA data. It also includes functions for
parsing DNA data, solving for a VIO voltage from a group of pods, and
calculating the CRC-16 used in the DNA.

Note that some modification will be required to integrate the library with a
SmartVIO controller, as the set number of ports and VIO groups will change
based on the requirements of a particular carrier.

A test application `smartvio-test` is provided as a test suite for the
SmartVIO solver algorithm. This test is based on the
[Catch](https://github.com/catchorg/Catch2) library.

### DNA Writer Application

The `dna-writer` application can be used to generate a binary DNA blob from
a JSON-format input. This application uses the
[JSON for Modern C++](https://github.com/nlohmann/json) library. The DNA
writer conforms to the SYZYGY DNA Specification v1.0. Please see the
[SYZYGY](http://syzygyfpga.io/) website for more information.

Please see the examples in the `pod-dna` folder for peripheral DNA examples.

## Building

A Makefile is provided to assist with building this design. Simply run the
"make" command on a system with the necessary development tools installed to
build all provided applications. Note that the dna-writer and smartvio-test
applications require C++ 11 support. The JSON library used by dna-writer
requires GCC 4.9 or later.

Individual applications can be built by running `make <application>`.

This build has been tested on a machine running Ubuntu 16.04 LTS with
GCC 5.4.0.
