#include "test_input.hh"
#include "../../NFinput/NFinput.hh"
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

// Expose the internal functions from walk.cpp
int getInput(int min, int max);
double getInput(double min);

using namespace std;

void NFtest_input::run() {
    cout << "Testing getInput..." << endl;

    // Save original cin/cout buffers
    streambuf *orig_cin = cin.rdbuf();
    streambuf *orig_cout = cout.rdbuf();

    ostringstream out;
    cout.rdbuf(out.rdbuf());

    // Test valid int input
    {
        istringstream in("5\n");
        cin.rdbuf(in.rdbuf());
        int res = getInput(0, 10);
        if (res != 5) throw runtime_error("Expected 5");
    }

    // Test invalid int input (out of bounds)
    {
        istringstream in("11\n5\n");
        cin.rdbuf(in.rdbuf());
        int res = getInput(0, 10);
        if (res != 5) throw runtime_error("Expected 5");
    }

    // Test invalid int input (runtime_error on parsing)
    {
        istringstream in("abc\n5\n");
        cin.rdbuf(in.rdbuf());
        int res = getInput(0, 10);
        if (res != 5) throw runtime_error("Expected 5");
    }

    // Test valid double input
    {
        istringstream in("5.5\n");
        cin.rdbuf(in.rdbuf());
        double res = getInput(0.0);
        if (res != 5.5) throw runtime_error("Expected 5.5");
    }

    // Test invalid double input (out of bounds)
    {
        istringstream in("-1.0\n5.5\n");
        cin.rdbuf(in.rdbuf());
        double res = getInput(0.0);
        if (res != 5.5) throw runtime_error("Expected 5.5");
    }

    // Test invalid double input (runtime_error on parsing)
    {
        istringstream in("abc\n5.5\n");
        cin.rdbuf(in.rdbuf());
        double res = getInput(0.0);
        if (res != 5.5) throw runtime_error("Expected 5.5");
    }

    // Restore buffers
    cin.rdbuf(orig_cin);
    cout.rdbuf(orig_cout);
    cout << "getInput tests passed!" << endl;
}
