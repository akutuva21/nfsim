#include "test_random.hh"
#include <cmath>

void NFtest_random::run()
{
	cout << "Running the random gaussian test" << endl;

	const int numSamples = 1000000;
	double sum = 0;
	double sumSq = 0;

	// Reset seed for reproducibility and isolation
	NFutil::SEED_RANDOM(12345);

	for(int i=0; i<numSamples; i++) {
		double val = NFutil::RANDOM_GAUSSIAN();
		sum += val;
		sumSq += val * val;
	}

	double mean = sum / numSamples;
	double variance = (sumSq / numSamples) - (mean * mean);

	cout << "Mean: " << mean << endl;
	cout << "Variance: " << variance << endl;

	if (std::abs(mean) > 0.05) {
		throw std::runtime_error("Mean is not close to 0");
	}
	if (std::abs(variance - 1.0) > 0.05) {
		throw std::runtime_error("Variance is not close to 1");
	}

	// Test determinism
	NFutil::SEED_RANDOM(54321);
	double val1 = NFutil::RANDOM_GAUSSIAN();
	double val2 = NFutil::RANDOM_GAUSSIAN();

	NFutil::SEED_RANDOM(54321);
	double val3 = NFutil::RANDOM_GAUSSIAN();
	double val4 = NFutil::RANDOM_GAUSSIAN();

	if (val1 != val3 || val2 != val4) {
		throw std::runtime_error("RANDOM_GAUSSIAN is not deterministic with SEED_RANDOM");
	}

	cout << "Random gaussian test passed successfully!" << endl;
}