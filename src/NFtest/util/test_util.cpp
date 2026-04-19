#include "test_util.hh"
#include <iostream>
#include <stdexcept>
#include "../../NFcore/NFcore.hh"

using namespace std;
using namespace NFutil;
using namespace NFcore;

void testTemplateMoleculeConstraint() {
    cout << "  Testing TemplateMolecule constraints..." << endl;

    System *s = new System("test");

    vector <string> compName;
    vector <string> defaultCompState;
    vector < vector <string> > possibleCompStates;

    compName.push_back("p");
    defaultCompState.push_back("0");
    vector <string> pStates;
    pStates.push_back("0");
    pStates.push_back("1");
    possibleCompStates.push_back(pStates);

    MoleculeType *molX = new MoleculeType("X", compName, defaultCompState, possibleCompStates, s);

    TemplateMolecule *xTemp = new TemplateMolecule(molX);
    xTemp->addComponentConstraint("p", "0");

    // Adding the same constraint should be fine (idempotent)
    xTemp->addComponentConstraint("p", "0");

    bool threw_exception = false;
    try {
        xTemp->addComponentConstraint("p", "1");
    } catch (const std::runtime_error& e) {
        // Expected
        cout << "  Caught expected runtime_error: " << e.what() << endl;
        threw_exception = true;
    } catch (...) {
        throw std::logic_error("TemplateMolecule::addComponentConstraint threw unexpected exception type!");
    }

    if (!threw_exception) {
        throw std::logic_error("TemplateMolecule::addComponentConstraint should have thrown an exception for mutually exclusive constraints!");
    }

    // NOTE: delete s deletes the MoleculeType, which deletes all its templates, including xTemp
    // delete xTemp;
    delete s;
    cout << "  TemplateMolecule constraints tests passed!" << endl;
}

void NFtest_util::run()
{
	cout << "Running NFutil tests..." << endl;

	const int NUM_ITERATIONS = 100000;

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

	testTemplateMoleculeConstraint();

	cout << "NFutil tests completed successfully." << endl;
}
