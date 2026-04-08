#include "test_random.hh"
#include "../../NFutil/NFutil.hh"
#include <iostream>
#include <cmath>
#include <vector>

using namespace std;

void NFtest_random::run() {
    cout << "Running the random test suite" << endl;

    int num_samples = 100000;
    double sum = 0.0;
    double sq_sum = 0.0;

    int within_1_std = 0;
    int within_2_std = 0;
    int within_3_std = 0;

    NFutil::SEED_RANDOM(12345);

    vector<double> samples;
    samples.reserve(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        double val = NFutil::RANDOM_GAUSSIAN();
        samples.push_back(val);
        sum += val;
        sq_sum += val * val;

        if (std::abs(val) <= 1.0) within_1_std++;
        if (std::abs(val) <= 2.0) within_2_std++;
        if (std::abs(val) <= 3.0) within_3_std++;
    }

    double mean = sum / num_samples;
    double variance = (sq_sum / num_samples) - (mean * mean);

    double pct_1 = (double)within_1_std / num_samples;
    double pct_2 = (double)within_2_std / num_samples;
    double pct_3 = (double)within_3_std / num_samples;

    cout << "Samples generated: " << num_samples << endl;
    cout << "Mean: " << mean << " (Expected ~0.0)" << endl;
    cout << "Variance: " << variance << " (Expected ~1.0)" << endl;
    cout << "Within 1 std (expected ~0.6827): " << pct_1 << endl;
    cout << "Within 2 std (expected ~0.9545): " << pct_2 << endl;
    cout << "Within 3 std (expected ~0.9973): " << pct_3 << endl;

    bool passed = true;
    if (std::abs(mean) > 0.05) {
        cout << "FAIL: Mean is too far from 0." << endl;
        passed = false;
    }
    if (std::abs(variance - 1.0) > 0.05) {
        cout << "FAIL: Variance is too far from 1." << endl;
        passed = false;
    }
    if (std::abs(pct_1 - 0.6827) > 0.05) {
        cout << "FAIL: 1 std dev proportion is too far from expected." << endl;
        passed = false;
    }
    if (std::abs(pct_2 - 0.9545) > 0.05) {
        cout << "FAIL: 2 std dev proportion is too far from expected." << endl;
        passed = false;
    }
    if (std::abs(pct_3 - 0.9973) > 0.01) {
        cout << "FAIL: 3 std dev proportion is too far from expected." << endl;
        passed = false;
    }

    if (passed) {
        cout << "Random test passed!" << endl;
    } else {
        cout << "Random test failed!" << endl;
        exit(1);
    }
}
