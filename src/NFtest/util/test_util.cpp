#include "test_util.hh"
#include <iostream>
#include <stdexcept>
#include "../../NFutil/MTrand/mtrand.h"

using namespace std;
using namespace NFutil;

void NFtest_util::run()
{
	cout << "Running NFutil tests..." << endl;

	const int NUM_ITERATIONS = 100000;

	cout << "  Testing MTRand_int32 initialization error paths..." << endl;

	// Test case: null pointer
	try {
		unsigned long* null_array = nullptr;
		MTRand_int32 rng(null_array, 4);
		throw std::runtime_error("MTRand_int32 did not throw on null array");
	} catch (const std::invalid_argument& e) {
		// Expected
	}

	// Test case: size <= 0
	try {
		unsigned long valid_array[4] = {0x123, 0x234, 0x345, 0x456};
		MTRand_int32 rng(valid_array, 0);
		throw std::runtime_error("MTRand_int32 did not throw on size == 0");
	} catch (const std::invalid_argument& e) {
		// Expected
	}

	try {
		unsigned long valid_array[4] = {0x123, 0x234, 0x345, 0x456};
		MTRand_int32 rng(valid_array, -1);
		throw std::runtime_error("MTRand_int32 did not throw on size == -1");
	} catch (const std::invalid_argument& e) {
		// Expected
	}

	// Test case: valid initialization
	unsigned long valid_array[4] = {0x123, 0x234, 0x345, 0x456};
	MTRand_int32 valid_rng(valid_array, 4); // Should not throw

	cout << "  MTRand_int32 initialization error paths passed!" << endl;

	cout << "  Testing MTRand_int32 seed functionality..." << endl;

	// Note: MTRand_int32 uses static state, so all instances share the same state.
	// We test it by seeding, saving a sequence, reseeding with the same seed,
	// and verifying the sequence is identical.

	MTRand_int32 rng;
	const int SEQ_LENGTH = 100;

	// Test setting seed with 32-bit int
	rng.seed(12345UL);
	std::vector<unsigned long> int_seq;
	for (int i = 0; i < SEQ_LENGTH; ++i) {
		int_seq.push_back(rng());
	}

	rng.seed(12345UL);
	for (int i = 0; i < SEQ_LENGTH; ++i) {
		unsigned long val = rng();
		if (val != int_seq[i]) {
			throw std::runtime_error("MTRand_int32 reseeding with identical int seed produced different outputs");
		}
	}

	// Test resetting seed with 32-bit int (shorter check)
	rng.seed(98765UL);
	unsigned long first_val_1 = rng();
	rng.seed(98765UL);
	unsigned long first_val_2 = rng();
	if (first_val_1 != first_val_2) {
		throw std::runtime_error("MTRand_int32 reseeding with same int seed did not reset sequence");
	}

	// Test setting seed with array
	unsigned long seed_array[4] = {0x111, 0x222, 0x333, 0x444};
	rng.seed(seed_array, 4);
	std::vector<unsigned long> arr_seq;
	for (int i = 0; i < SEQ_LENGTH; ++i) {
		arr_seq.push_back(rng());
	}

	rng.seed(seed_array, 4);
	for (int i = 0; i < SEQ_LENGTH; ++i) {
		unsigned long val = rng();
		if (val != arr_seq[i]) {
			throw std::runtime_error("MTRand_int32 reseeding with identical array seed produced different outputs");
		}
	}

	// Test resetting seed with array
	unsigned long seed_array2[4] = {0x555, 0x666, 0x777, 0x888};
	rng.seed(seed_array2, 4);
	unsigned long first_val_3 = rng();
	rng.seed(seed_array2, 4);
	unsigned long first_val_4 = rng();
	if (first_val_3 != first_val_4) {
		throw std::runtime_error("MTRand_int32 reseeding with same array seed did not reset sequence");
	}

	cout << "  MTRand_int32 seed functionality passed!" << endl;

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

	cout << "  Testing trim..." << endl;

	std::string t1 = "  hello world  ";
	NFutil::trim(t1);
	if (t1 != "hello world") throw std::runtime_error("trim failed on normal case with leading and trailing spaces");

	std::string t2 = "  hello world";
	NFutil::trim(t2);
	if (t2 != "hello world") throw std::runtime_error("trim failed on case with only leading spaces");

	std::string t3 = "hello world  ";
	NFutil::trim(t3);
	if (t3 != "hello world") throw std::runtime_error("trim failed on case with only trailing spaces");

	std::string t4 = "";
	NFutil::trim(t4);
	if (t4 != "") throw std::runtime_error("trim failed on empty string");

	std::string t5 = "     ";
	NFutil::trim(t5);
	if (t5 != "") throw std::runtime_error("trim failed on string with only spaces");

	std::string t6 = "\t\t";
	NFutil::trim(t6);
	if (t6 != "") throw std::runtime_error("trim failed on string with only tabs");

	std::string t7 = " \t \t ";
	NFutil::trim(t7);
	if (t7 != "") throw std::runtime_error("trim failed on mixed spaces and tabs");

	std::string t8 = "hello";
	NFutil::trim(t8);
	if (t8 != "hello") throw std::runtime_error("trim failed on string without spaces");

	std::string t9 = "\t hello \t ";
	NFutil::trim(t9);
	if (t9 != "hello") throw std::runtime_error("trim failed on string with spaces and tabs around");

	cout << "  trim tests passed!" << endl;

	cout << "NFutil tests completed successfully." << endl;
}
