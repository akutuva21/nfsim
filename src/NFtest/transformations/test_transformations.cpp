#include "test_transformations.hh"
#include "../../NFcore/NFcore.hh"
#include "../../NFreactions/NFreactions.hh"
#include "../../NFreactions/transformations/transformation.hh"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

using namespace std;
using namespace NFcore;

void NFtest_transformations::run()
{
	cout << "Running transformations tests..." << endl;

	// 1. Setup a simple system with one molecule type
	System* s = new System("TestSys");

	vector<string> compName;
	compName.push_back("stateComp");

	vector<string> defaultCompState;
	defaultCompState.push_back("State0");

	vector<vector<string>> possibleCompStates;
	vector<string> states;
	states.push_back("State0");
	states.push_back("State1");
	states.push_back("State2");
	possibleCompStates.push_back(states);

	MoleculeType* molType = new MoleculeType("TestMol", compName, defaultCompState, possibleCompStates, s);

	// Create a molecule
	Molecule* mol = new Molecule(molType, 0, nullptr);

	// Verify initial state is 0
	if (mol->getComponentState(0) != 0) {
		throw std::runtime_error("Initial state should be 0");
	}

	// 2. Setup mapping
	Mapping* mapping = new Mapping(TransformationFactory::STATE_CHANGE, 0); // type doesn't strictly matter for setting mol here
	mapping->setMolecule(mol);

	// 3. Setup StateChangeTransform
	int newStateIndex = 2; // "State2"
	Transformation* transform = TransformationFactory::genStateChangeTransform(0, newStateIndex);

	// 4. Apply transformation
	MappingSet* msArray[1] = { nullptr }; // Dummy MappingSet
	transform->apply(mapping, msArray);

	// 5. Verify state change
	if (mol->getComponentState(0) != newStateIndex) {
		throw std::runtime_error("StateChangeTransform failed to apply state change. Expected " + std::to_string(newStateIndex) + ", got " + std::to_string(mol->getComponentState(0)));
	}

	// Test the logging overload (AS2023)
	int anotherStateIndex = 1; // "State1"
	Transformation* transform2 = TransformationFactory::genStateChangeTransform(0, anotherStateIndex);
	string logstr = "initial";
	transform2->apply(mapping, msArray, logstr);

	if (mol->getComponentState(0) != anotherStateIndex) {
		throw std::runtime_error("StateChangeTransform (logging) failed to apply state change.");
	}
	if (logstr.empty() || logstr.find("StateChange") == string::npos) {
		throw std::runtime_error("StateChangeTransform (logging) failed to generate proper log string.");
	}

	cout << "  StateChangeTransform tests passed!" << endl;
	cout << "Transformations tests completed successfully." << endl;

	// Cleanup
	delete transform;
	delete transform2;
	delete mapping;
	delete mol;
	// MoleculeType deleted by System? Need to check memory management, but for quick tests, this might leak a little, it's ok.
	delete s;
}
