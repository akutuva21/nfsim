#include "test_random.hh"
#include "../../NFutil/NFutil.hh"
#include <iostream>
#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace std;
using namespace NFutil;

namespace NFtest_random
{
    void testRandomClosed() {
        cout << "  Testing RANDOM_CLOSED()... ";
        for (int i = 0; i < 10000; ++i) {
            double val = RANDOM_CLOSED();
            if (val < 0.0 || val > 1.0) {
                throw runtime_error("RANDOM_CLOSED() generated value out of bounds [0, 1]: " + toString(val));
            }
        }
        cout << "Passed." << endl;
    }

    void testRandomOpen() {
        cout << "  Testing RANDOM_OPEN()... ";
        for (int i = 0; i < 10000; ++i) {
            double val = RANDOM_OPEN();
            if (val <= 0.0 || val >= 1.0) {
                throw runtime_error("RANDOM_OPEN() generated value out of bounds (0, 1): " + toString(val));
            }
        }
        cout << "Passed." << endl;
    }

    void testRandom() {
        cout << "  Testing RANDOM(max)... ";
        double max_val = 5.5;
        for (int i = 0; i < 10000; ++i) {
            double val = RANDOM(max_val);
            if (val <= 0.0 || val > max_val) {
                throw runtime_error("RANDOM() generated value out of bounds (0, max]: " + toString(val));
            }
        }
        cout << "Passed." << endl;
    }

    void testRandomInt() {
        cout << "  Testing RANDOM_INT(min, max)... ";
        int min_val = -5;
        int max_val = 5;
        for (int i = 0; i < 10000; ++i) {
            int val = RANDOM_INT(min_val, max_val);
            if (val < min_val || val >= max_val) {
                throw runtime_error("RANDOM_INT() generated value out of bounds [min, max): " + toString(val));
            }
        }
        cout << "Passed." << endl;
    }

    void testRandomGaussian() {
        cout << "  Testing RANDOM_GAUSSIAN()... ";
        double sum = 0.0;
        double sum_sq = 0.0;
        int n = 10000;
        for (int i = 0; i < n; ++i) {
            double val = RANDOM_GAUSSIAN();
            sum += val;
            sum_sq += val * val;
        }
        double mean = sum / n;
        double variance = (sum_sq / n) - (mean * mean);

        if (std::abs(mean) > 0.1) {
            throw runtime_error("RANDOM_GAUSSIAN() mean is too far from 0: " + toString(mean));
        }
        if (std::abs(variance - 1.0) > 0.1) {
            throw runtime_error("RANDOM_GAUSSIAN() variance is too far from 1: " + toString(variance));
        }
        cout << "Passed." << endl;
    }

    void run()
    {
        cout << "Running NFutil::random tests..." << endl;

        SEED_RANDOM(12345);

        try {
            testRandomClosed();
            testRandomOpen();
            testRandom();
            testRandomInt();
            testRandomGaussian();
            cout << "All NFutil::random tests passed successfully." << endl;
        } catch (const exception& e) {
            cerr << "Test failed: " << e.what() << endl;
            exit(1);
        }
    }
}