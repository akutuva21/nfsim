#include "test_rng.hh"
#include <iostream>
#include <stdexcept>
#include "../../NFutil/nfsim_rng.h"

using namespace std;
using namespace NFcore;

void NFtest_rng::run()
{
	cout << "Running NFutil RNG tests..." << endl;

	const unsigned long test_seed = 123456789UL;
	const unsigned long test_seed_2 = 987654321UL;
	const int num_iterations = 100;

	// Test case 1: Determinism with same seed
	NfsimRNG rng1(test_seed);
	NfsimRNG rng2(test_seed);

	for (int i = 0; i < num_iterations; ++i) {
		unsigned long val1 = rng1.rand_int32();
		unsigned long val2 = rng2.rand_int32();
		if (val1 != val2) {
			throw std::runtime_error("RNG instances with same seed produced different sequences!");
		}
	}

	// Test case 2: Different sequences with different seeds
	NfsimRNG rng3(test_seed);
	NfsimRNG rng4(test_seed_2);

	bool all_same = true;
	for (int i = 0; i < num_iterations; ++i) {
		unsigned long val3 = rng3.rand_int32();
		unsigned long val4 = rng4.rand_int32();
		if (val3 != val4) {
			all_same = false;
			break;
		}
	}

	if (all_same) {
		throw std::runtime_error("RNG instances with different seeds produced identical sequences!");
	}

	// Test case 3: Re-seeding an existing instance
	NfsimRNG rng5;
	rng5.seed(test_seed);
	NfsimRNG rng6(test_seed);

	for (int i = 0; i < num_iterations; ++i) {
		unsigned long val5 = rng5.rand_int32();
		unsigned long val6 = rng6.rand_int32();
		if (val5 != val6) {
			throw std::runtime_error("Re-seeded RNG instance produced different sequence from freshly seeded instance!");
		}
	}

	cout << "  NfsimRNG tests passed!" << endl;
	cout << "NFutil RNG tests completed successfully." << endl;
}
