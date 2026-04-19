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

	const string callerName = "test_util";

	// Helper to create temp files
	auto createTempFile = [](const string& filename, const string& content) {
		std::ofstream out(filename);
		out << content;
		out.close();
	};

	// Test 1: Valid monotonic increasing data
	string validIncFile = "temp_valid_inc.txt";
	createTempFile(validIncFile, "0.0 1.0\n1.0 2.0\n2.0 3.0\n");
	try {
		TimeSeries ts = NFutil::loadTimeSeries(validIncFile, callerName);
		if (ts.time.size() != 3 || ts.time[0] != 0.0 || ts.time[2] != 2.0) {
			throw std::runtime_error("loadTimeSeries failed to parse valid increasing data correctly.");
		}
	} catch (const std::exception& e) {
		throw std::runtime_error(std::string("loadTimeSeries threw unexpected exception for valid increasing data: ") + e.what());
	}
	remove(validIncFile.c_str());

	// Test 2: Valid monotonic decreasing data
	string validDecFile = "temp_valid_dec.txt";
	createTempFile(validDecFile, "3.0 1.0\n2.0 2.0\n1.0 3.0\n");
	try {
		TimeSeries ts = NFutil::loadTimeSeries(validDecFile, callerName);
		if (ts.time.size() != 3 || ts.time[0] != 3.0 || ts.time[2] != 1.0) {
			throw std::runtime_error("loadTimeSeries failed to parse valid decreasing data correctly.");
		}
	} catch (const std::exception& e) {
		throw std::runtime_error(std::string("loadTimeSeries threw unexpected exception for valid decreasing data: ") + e.what());
	}
	remove(validDecFile.c_str());

	// Test 3: Missing file
	bool threw = false;
	try {
		NFutil::loadTimeSeries("non_existent_file_xyz.txt", callerName);
	} catch (const std::runtime_error& e) {
		threw = true;
	}
	if (!threw) throw std::runtime_error("loadTimeSeries did not throw for missing file.");

	// Test 4: Empty file
	string emptyFile = "temp_empty.txt";
	createTempFile(emptyFile, "");
	threw = false;
	try {
		NFutil::loadTimeSeries(emptyFile, callerName);
	} catch (const std::runtime_error& e) {
		threw = true;
	}
	remove(emptyFile.c_str());
	if (!threw) throw std::runtime_error("loadTimeSeries did not throw for empty file.");

	// Test 5: Duplicate time
	string dupFile = "temp_dup.txt";
	createTempFile(dupFile, "0.0 1.0\n1.0 2.0\n1.0 3.0\n");
	threw = false;
	try {
		NFutil::loadTimeSeries(dupFile, callerName);
	} catch (const std::runtime_error& e) {
		threw = true;
	}
	remove(dupFile.c_str());
	if (!threw) throw std::runtime_error("loadTimeSeries did not throw for duplicate time.");

	// Test 6: Non-monotonic time
	string nonMonoFile = "temp_non_mono.txt";
	createTempFile(nonMonoFile, "0.0 1.0\n1.0 2.0\n0.5 3.0\n");
	threw = false;
	try {
		NFutil::loadTimeSeries(nonMonoFile, callerName);
	} catch (const std::runtime_error& e) {
		threw = true;
	}
	remove(nonMonoFile.c_str());
	if (!threw) throw std::runtime_error("loadTimeSeries did not throw for non-monotonic time.");

	// Test 7: Invalid format
	string invalidFormatFile = "temp_invalid.txt";
	createTempFile(invalidFormatFile, "0.0 abc\n");
	threw = false;
	try {
		NFutil::loadTimeSeries(invalidFormatFile, callerName);
	} catch (const std::runtime_error& e) {
		threw = true;
	}
	remove(invalidFormatFile.c_str());
	if (!threw) throw std::runtime_error("loadTimeSeries did not throw for invalid number format.");

	cout << "  loadTimeSeries tests passed!" << endl;

	cout << "NFutil tests completed successfully." << endl;
}
