#include "test_compartment.hh"
#include "../../NFcore/compartment.hh"
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace NFcore;

void NFtest_compartment::run()
{
	cout << "Running NFcore::Compartment tests..." << endl;

	// Test 1: Basic constructor and getters
	cout << "  Testing constructor and getters..." << endl;
	Compartment* c1 = new Compartment("c1", 3, 10.5);
	if (c1->getId() != "c1") {
		throw runtime_error("getId failed for c1");
	}
	if (c1->getSpatialDimensions() != 3) {
		throw runtime_error("getSpatialDimensions failed for c1");
	}
	if (c1->getSize() != 10.5) {
		throw runtime_error("getSize failed for c1");
	}
	if (c1->getVolume() != 10.5) {
		throw runtime_error("getVolume failed for c1");
	}
	if (c1->getParent() != 0) {
		throw runtime_error("getParent should return null for c1");
	}

	// Test 2: Nested compartment and isInside
	cout << "  Testing setParent and isInside..." << endl;
	Compartment* c2 = new Compartment("c2", 2, 5.0, c1);
	if (c2->getParent() != c1) {
		throw runtime_error("getParent failed for c2");
	}
	if (!c2->isInside(c1)) {
		throw runtime_error("isInside failed for c2 inside c1");
	}
	if (c1->isInside(c2)) {
		throw runtime_error("isInside returned false positive for c1 inside c2");
	}

	// Test 3: Transitive isInside
	cout << "  Testing transitive isInside..." << endl;
	Compartment* c3 = new Compartment("c3", 3, 1.0);
	c3->setParent(c2);
	if (c3->getParent() != c2) {
		throw runtime_error("setParent failed for c3");
	}
	if (!c3->isInside(c1)) {
		throw runtime_error("transitive isInside failed for c3 inside c1");
	}
	if (!c3->isInside(c2)) {
		throw runtime_error("isInside failed for c3 inside c2");
	}

	// Test 4: isInside with null and self
	cout << "  Testing isInside edge cases..." << endl;
	if (c1->isInside(0)) {
		throw runtime_error("isInside returned false positive for null other");
	}
	if (!c1->isInside(c1)) {
		throw runtime_error("isInside failed for self");
	}

	// Test 5: Print details (visual check, should not crash)
	cout << "  Testing printDetails..." << endl;
	c1->printDetails();
	c2->printDetails();
	c3->printDetails();

	delete c3;
	delete c2;
	delete c1;

	cout << "  All Compartment tests passed!" << endl;
	cout << "NFcore::Compartment tests completed successfully." << endl;
}
