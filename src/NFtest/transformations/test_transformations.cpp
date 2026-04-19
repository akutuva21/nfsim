#include "test_transformations.hh"
#include "../../NFreactions/transformations/moleculeCreator.hh"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

using namespace std;
using namespace NFcore;

void NFtest_transformations::run()
{
	cout << "Running transformations tests..." << endl;

	cout << "  Testing TransformationSet..." << endl;

	// Create a dummy system
	System *s = new System("TestSystem");

	// Create MoleculeType X
	vector<string> xComps;
	xComps.push_back("a");
	xComps.push_back("p");
	vector<string> xStates;
	xStates.push_back("No State");
	xStates.push_back("U");
	vector<vector<string> > xAllowedStates;
	vector<string> aStates;
	xAllowedStates.push_back(aStates);
	vector<string> pStates;
	pStates.push_back("U");
	pStates.push_back("P");
	xAllowedStates.push_back(pStates);
	MoleculeType *molX = new MoleculeType("X", xComps, xStates, xAllowedStates, s);
	s->addMoleculeType(molX);

	// Create MoleculeType Y
	vector<string> yComps;
	yComps.push_back("a");
	vector<string> yStates;
	yStates.push_back("No State");
	vector<vector<string> > yAllowedStates;
	vector<string> aStatesY;
	yAllowedStates.push_back(aStatesY);
	MoleculeType *molY = new MoleculeType("Y", yComps, yStates, yAllowedStates, s);
	s->addMoleculeType(molY);

	// Create template molecules
	TemplateMolecule *tx = new TemplateMolecule(molX);
	TemplateMolecule *ty = new TemplateMolecule(molY);

	vector<TemplateMolecule*> reactants;
	reactants.push_back(tx);
	reactants.push_back(ty);

	// Create a TransformationSet
	TransformationSet *ts = new TransformationSet(reactants);

	if (ts->getNreactants() != 2) {
		throw runtime_error("TransformationSet getNreactants failed");
	}

	if (ts->getNmappingSets() != 2) {
		throw runtime_error("TransformationSet getNmappingSets failed");
	}

	// Test adding transformations
	ts->addStateChangeTransform(tx, "p", "P");
	if (ts->getNumOfTransformations(0) != 1) {
	    throw runtime_error("TransformationSet getNumOfTransformations failed for state change");
	}

	ts->addBindingTransform(tx, "a", ty, "a");
	if (ts->getNumOfTransformations(0) != 2) {
	    throw runtime_error("TransformationSet getNumOfTransformations failed for binding (first reactant)");
	}
	if (ts->getNumOfTransformations(1) != 1) {
	    throw runtime_error("TransformationSet getNumOfTransformations failed for binding (second reactant)");
	}

	if (ts->hasSymBindingTransform()) {
	    throw runtime_error("TransformationSet hasSymBindingTransform failed: X and Y are not symmetric");
	}

	// Test adding molecule creation transform
	TemplateMolecule *newT = new TemplateMolecule(molX);
	vector<pair<int, int> > stateValues; // Empty state values
	MoleculeCreator *mc = new MoleculeCreator(newT, molX, stateValues, NULL);
	ts->addAddMolecule(mc);
	if (ts->getNumOfAddMoleculeTransforms() != 1) {
	    throw runtime_error("TransformationSet getNumOfAddMoleculeTransforms failed");
	}

	// Finalize and check
	ts->finalize();
	if (!ts->isFinalized()) {
		throw runtime_error("TransformationSet finalize failed");
	}

	// Complex Bookkeeping checks
	ts->setComplexBookkeeping(true);
	if (!ts->getComplexBookkeeping()) {
	    throw runtime_error("TransformationSet getComplexBookkeeping failed");
	}

	delete ts;

    // Test symmetric binding
    TemplateMolecule *tx3 = new TemplateMolecule(molX);
	TemplateMolecule *ty3 = new TemplateMolecule(molX);
	vector<TemplateMolecule*> reactants3;
	reactants3.push_back(tx3);
	reactants3.push_back(ty3);
	TransformationSet *ts3 = new TransformationSet(reactants3);
	ts3->addBindingTransform(tx3, "a", ty3, "a");
	if (!ts3->hasSymBindingTransform()) {
	    throw runtime_error("TransformationSet hasSymBindingTransform failed: should be symmetric");
	}
	ts3->finalize();
	delete ts3;

    // Test symmetric unbinding
    TemplateMolecule *tx2 = new TemplateMolecule(molX);
    TemplateMolecule *ty2 = new TemplateMolecule(molX);
    tx2->addBond("a", ty2, "a");
    ty2->addBond("a", tx2, "a"); // Note: addBond already adds the reverse.
	vector<TemplateMolecule*> reactants2;
	reactants2.push_back(tx2);
	TransformationSet *ts2 = new TransformationSet(reactants2);
	ts2->addUnbindingTransform(tx2, "a", ty2, "a");
	if (!ts2->hasSymUnbindingTransform()) {
	    throw runtime_error("TransformationSet hasSymUnbindingTransform failed");
	}
	ts2->finalize();
	delete ts2;

	cout << "  TransformationSet basic tests passed!" << endl;

	cout << "Transformations tests completed successfully." << endl;
}
