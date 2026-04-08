#include "test_random.hh"
#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include <cassert>
#include <cstdlib>

using namespace std;
using namespace NFutil;

void NFtest_random::run()
{
    cout << "Running the random gaussian test..." << endl;

    // Seed the random number generator for determinism
    SEED_RANDOM(12345);

    int n = 100000;
    vector<double> samples(n);

    double sum = 0;
    for (int i = 0; i < n; ++i) {
        samples[i] = RANDOM_GAUSSIAN();
        sum += samples[i];
    }

    double mean = sum / n;

    double variance_sum = 0;
    for (int i = 0; i < n; ++i) {
        variance_sum += (samples[i] - mean) * (samples[i] - mean);
    }
    double variance = variance_sum / n;

    cout << "Mean: " << mean << " (expected ~0.0)" << endl;
    cout << "Variance: " << variance << " (expected ~1.0)" << endl;

    // Check if mean is close to 0
    if (abs(mean) >= 0.05) {
        cerr << "Error: Mean is not close to 0.0" << endl;
        exit(1);
    }

    // Check if variance is close to 1
    if (abs(variance - 1.0) >= 0.05) {
        cerr << "Error: Variance is not close to 1.0" << endl;
        exit(1);
    }

    cout << "test_random_gaussian passed!" << endl;
}
