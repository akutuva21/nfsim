#include "conversion.hh"
#include "../../NFutil/NFutil.hh"
#include <iostream>
#include <string>
#include <stdexcept>

using namespace std;
using namespace NFutil;

void NFtest_conversion::run()
{
    cout << "Testing NFutil::convertToDouble and NFutil::convertToInt..." << endl;

    // Test valid conversions
    try {
        double d1 = convertToDouble("1.23");
        if (d1 != 1.23) cout << "FAILED: convertToDouble(\"1.23\")" << endl;

        int i1 = convertToInt("42");
        if (i1 != 42) cout << "FAILED: convertToInt(\"42\")" << endl;

        cout << "Passed valid conversions" << endl;
    } catch(std::exception& e) {
        cout << "FAILED: unexpected exception during valid conversions" << endl;
    }

    // Test invalid conversions (leftover chars)
    try {
        convertToDouble("1.23a");
        cout << "FAILED: convertToDouble didn't throw on leftover chars" << endl;
    } catch(std::runtime_error& e) {
        cout << "Passed convertToDouble invalid conversion (leftover chars)" << endl;
    }

    // Test invalid conversions (empty/invalid)
    try {
        convertToDouble("abc");
        cout << "FAILED: convertToDouble didn't throw on invalid string" << endl;
    } catch(std::runtime_error& e) {
        cout << "Passed convertToDouble invalid conversion (invalid string)" << endl;
    }

    // Test convertToInt
    try {
        convertToInt("42a");
        cout << "FAILED: convertToInt didn't throw on leftover chars" << endl;
    } catch(std::runtime_error& e) {
        cout << "Passed convertToInt invalid conversion (leftover chars)" << endl;
    }

    cout << "Finished testing conversion." << endl;
}
