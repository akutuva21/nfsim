#include "random.hh"
#include "../../NFutil/NFutil.hh"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace std;
using namespace NFutil;

namespace NFtest_random
{
	void run()
	{
		cout << "Running NFutil random tests..." << endl;

		int numIterations = 100000;

		// Test RANDOM(max): (0, max]
		double max_val = 5.0;
		for (int i = 0; i < numIterations; ++i) {
			double val = RANDOM(max_val);
			if (val <= 0.0 || val > max_val) {
				cout << "RANDOM(" << max_val << ") failed bounds: " << val << endl;
				exit(1);
			}
		}
		cout << "  RANDOM(max) bounds check passed." << endl;

		// Test RANDOM_CLOSED(): [0, 1]
		for (int i = 0; i < numIterations; ++i) {
			double val = RANDOM_CLOSED();
			if (val < 0.0 || val > 1.0) {
				cout << "RANDOM_CLOSED() failed bounds: " << val << endl;
				exit(1);
			}
		}
		cout << "  RANDOM_CLOSED() bounds check passed." << endl;

		// Test RANDOM_OPEN(): (0, 1)
		for (int i = 0; i < numIterations; ++i) {
			double val = RANDOM_OPEN();
			if (val <= 0.0 || val >= 1.0) {
				cout << "RANDOM_OPEN() failed bounds: " << val << endl;
				exit(1);
			}
		}
		cout << "  RANDOM_OPEN() bounds check passed." << endl;

		// Test RANDOM_INT(min, max): [min, max)
		unsigned long min_int = 5;
		unsigned long max_int = 15;
		for (int i = 0; i < numIterations; ++i) {
			int val = RANDOM_INT(min_int, max_int);
			if (val < min_int || val >= max_int) {
				cout << "RANDOM_INT(" << min_int << ", " << max_int << ") failed bounds: " << val << endl;
				exit(1);
			}
		}
		cout << "  RANDOM_INT(min, max) bounds check passed." << endl;

		// Test RANDOM_GAUSSIAN()
		double sum = 0;
		double sq_sum = 0;
		for (int i = 0; i < numIterations; ++i) {
			double val = RANDOM_GAUSSIAN();
			sum += val;
			sq_sum += val * val;
		}
		double mean = sum / numIterations;
		double variance = (sq_sum / numIterations) - (mean * mean);

		if (abs(mean) > 0.05) {
			cout << "RANDOM_GAUSSIAN() mean check failed: " << mean << endl;
			exit(1);
		}
		if (abs(variance - 1.0) > 0.05) {
			cout << "RANDOM_GAUSSIAN() variance check failed: " << variance << endl;
			exit(1);
		}
		cout << "  RANDOM_GAUSSIAN() statistical check passed." << endl;

		cout << "All NFutil random tests passed!" << endl;
	}
}
