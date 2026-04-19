#include "test_util.hh"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cstdio>

using namespace std;
using namespace NFutil;

void NFtest_util::run()
{
	cout << "Running NFutil tests..." << endl;

	const int NUM_ITERATIONS = 100000;

	cout << "  Testing RANDOM_INT..." << endl;

	// Test case 1: range [0, 10)
	unsigned long min1 = 0;
	unsigned long max1 = 10;
	bool hit_min1 = false;
	bool hit_max_minus_1 = false;
	for (int i = 0; i < NUM_ITERATIONS; ++i) {
		int result = NFutil::RANDOM_INT(min1, max1);
		if (result < (int)min1 || result >= (int)max1) {
			throw std::runtime_error("RANDOM_INT generated a number out of range: " + std::to_string(result));
		}
		if (result == (int)min1) hit_min1 = true;
		if (result == (int)max1 - 1) hit_max_minus_1 = true;
	}
	if (!hit_min1) throw std::runtime_error("RANDOM_INT did not hit the minimum value");
	if (!hit_max_minus_1) throw std::runtime_error("RANDOM_INT did not hit the maximum - 1 value");

	// Test case 2: range [5, 20)
	unsigned long min2 = 5;
	unsigned long max2 = 20;
	bool hit_min2 = false;
	bool hit_max_minus_2 = false;
	for (int i = 0; i < NUM_ITERATIONS; ++i) {
		int result = NFutil::RANDOM_INT(min2, max2);
		if (result < (int)min2 || result >= (int)max2) {
			throw std::runtime_error("RANDOM_INT generated a number out of range: " + std::to_string(result));
		}
		if (result == (int)min2) hit_min2 = true;
		if (result == (int)max2 - 1) hit_max_minus_2 = true;
	}
	if (!hit_min2) throw std::runtime_error("RANDOM_INT did not hit the minimum value");
	if (!hit_max_minus_2) throw std::runtime_error("RANDOM_INT did not hit the maximum - 1 value");

	// Test case 3: very tight range [5, 6)
	unsigned long min3 = 5;
	unsigned long max3 = 6;
	for (int i = 0; i < NUM_ITERATIONS; ++i) {
		int result = NFutil::RANDOM_INT(min3, max3);
		if (result != 5) {
			throw std::runtime_error("RANDOM_INT generated a number out of range for tight bound: " + std::to_string(result));
		}
	}

	cout << "  RANDOM_INT tests passed!" << endl;

	cout << "  Testing loadTimeSeries..." << endl;
	auto create_temp_file = [](const std::string& filename, const std::string& content) {
		std::ofstream ofs(filename);
		ofs << content;
		ofs.close();
	};

	// Test case 1: Valid increasing time series
	create_temp_file("test_ts_valid_inc.txt", "0.0 10.0\n1.0 20.0\n2.0 30.0\n");
	NFutil::TimeSeries ts1 = NFutil::loadTimeSeries("test_ts_valid_inc.txt", "testValidInc");
	if (ts1.time.size() != 3 || ts1.values.size() != 3 || ts1.time[0] != 0.0 || ts1.values[2] != 30.0) {
		throw std::runtime_error("loadTimeSeries failed to parse valid increasing series.");
	}
	remove("test_ts_valid_inc.txt");

	// Test case 2: Valid decreasing time series
	create_temp_file("test_ts_valid_dec.txt", "2.0 30.0\n1.0 20.0\n0.0 10.0\n");
	NFutil::TimeSeries ts2 = NFutil::loadTimeSeries("test_ts_valid_dec.txt", "testValidDec");
	if (ts2.time.size() != 3 || ts2.time[1] != 1.0) {
		throw std::runtime_error("loadTimeSeries failed to parse valid decreasing series.");
	}
	remove("test_ts_valid_dec.txt");

	// Test case 3: Missing/non-existent file
	bool threwMissing = false;
	try {
		NFutil::loadTimeSeries("test_ts_nonexistent.txt", "testMissing");
	} catch (const std::runtime_error& e) {
		threwMissing = true;
		if (std::string(e.what()).find("doesn't look like it exists") == std::string::npos) {
			throw std::runtime_error("loadTimeSeries threw incorrect error for missing file.");
		}
	}
	if (!threwMissing) {
		throw std::runtime_error("loadTimeSeries did not throw on missing file.");
	}

	// Test case 4: Empty file
	create_temp_file("test_ts_empty.txt", "");
	bool threwEmpty = false;
	try {
		NFutil::loadTimeSeries("test_ts_empty.txt", "testEmpty");
	} catch (const std::runtime_error& e) {
		threwEmpty = true;
		if (std::string(e.what()).find("empty or invalid format") == std::string::npos) {
			throw std::runtime_error("loadTimeSeries threw incorrect error for empty file.");
		}
	}
	if (!threwEmpty) {
		throw std::runtime_error("loadTimeSeries did not throw on empty file.");
	}
	remove("test_ts_empty.txt");

	// Test case 5: Duplicate time values
	create_temp_file("test_ts_dup.txt", "0.0 10.0\n1.0 20.0\n1.0 30.0\n");
	bool threwDup = false;
	try {
		NFutil::loadTimeSeries("test_ts_dup.txt", "testDup");
	} catch (const std::runtime_error& e) {
		threwDup = true;
		if (std::string(e.what()).find("Found duplicate time") == std::string::npos) {
			throw std::runtime_error("loadTimeSeries threw incorrect error for duplicate time values.");
		}
	}
	if (!threwDup) {
		throw std::runtime_error("loadTimeSeries did not throw on duplicate time values.");
	}
	remove("test_ts_dup.txt");

	// Test case 6: Non-monotonic time values (increasing then decreasing)
	create_temp_file("test_ts_nonmono.txt", "0.0 10.0\n1.0 20.0\n0.5 30.0\n");
	bool threwNonMono = false;
	try {
		NFutil::loadTimeSeries("test_ts_nonmono.txt", "testNonMono");
	} catch (const std::runtime_error& e) {
		threwNonMono = true;
		if (std::string(e.what()).find("strictly monotonic") == std::string::npos) {
			throw std::runtime_error("loadTimeSeries threw incorrect error for non-monotonic time values.");
		}
	}
	if (!threwNonMono) {
		throw std::runtime_error("loadTimeSeries did not throw on non-monotonic time values.");
	}
	remove("test_ts_nonmono.txt");

	// Test case 7: Invalid number formatting
	create_temp_file("test_ts_invalid_fmt.txt", "0.0 10.0\nabc 20.0\n");
	bool threwInvalidFmt = false;
	try {
		NFutil::loadTimeSeries("test_ts_invalid_fmt.txt", "testInvalidFmt");
	} catch (const std::runtime_error& e) {
		threwInvalidFmt = true;
		if (std::string(e.what()).find("invalid number format") == std::string::npos && std::string(e.what()).find("error in NFutil::convertToDouble") == std::string::npos) {
			throw std::runtime_error("loadTimeSeries threw incorrect error for invalid number formatting: " + std::string(e.what()));
		}
	}
	if (!threwInvalidFmt) {
		throw std::runtime_error("loadTimeSeries did not throw on invalid number formatting.");
	}
	remove("test_ts_invalid_fmt.txt");

	cout << "  loadTimeSeries tests passed!" << endl;

	cout << "NFutil tests completed successfully." << endl;
}
