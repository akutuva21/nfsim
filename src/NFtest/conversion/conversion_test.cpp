#include "conversion_test.hh"
#include "../../NFutil/NFutil.hh"
#include <iostream>
#include <string>

using namespace std;

void NFtest_conversion::run()
{
	cout << "Running NFutil::toString(int) tests..." << endl;
	int fails = 0;

	// Test case 1: Zero
	if (NFutil::toString(0) != "0") {
		cout << "Test failed: toString(0) returned " << NFutil::toString(0) << " instead of '0'" << endl;
		fails++;
	}

	// Test case 2: Positive integer
	if (NFutil::toString(42) != "42") {
		cout << "Test failed: toString(42) returned " << NFutil::toString(42) << " instead of '42'" << endl;
		fails++;
	}

	// Test case 3: Negative integer
	if (NFutil::toString(-42) != "-42") {
		cout << "Test failed: toString(-42) returned " << NFutil::toString(-42) << " instead of '-42'" << endl;
		fails++;
	}

	// Test case 4: Large positive integer
	if (NFutil::toString(2147483647) != "2147483647") {
		cout << "Test failed: toString(2147483647) returned " << NFutil::toString(2147483647) << " instead of '2147483647'" << endl;
		fails++;
	}

	// Test case 5: Large negative integer
	if (NFutil::toString(-2147483647) != "-2147483647") {
		cout << "Test failed: toString(-2147483647) returned " << NFutil::toString(-2147483647) << " instead of '-2147483647'" << endl;
		fails++;
	}

	if (fails == 0) {
		cout << "All NFutil::toString(int) tests passed!" << endl;
	} else {
		cout << fails << " tests failed!" << endl;
		exit(1);
	}
}
