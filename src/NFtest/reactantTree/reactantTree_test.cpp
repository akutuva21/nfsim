#include "reactantTree_test.hh"
#include "../../NFreactions/transformations/transformation.hh"
#include "../../NFreactions/transformations/transformationSet.hh"
#include "../../NFreactions/mappings/mappingSet.hh"
#include "../../NFreactions/reactantLists/reactantTree.hh"
#include <iostream>
#include <vector>

using namespace std;
using namespace NFcore;

void NFtest_reactantTree::run()
{
    cout << "Running ReactantTree tests..." << endl;

    int failCount = 0;

    // Create a mock system and molecule type
    System* sys = new System("testSys");
    vector<string> compNames;
    vector<string> defaultStates;
    vector<vector<string>> possibleStates;
    MoleculeType* mt = new MoleculeType("TestMol", compNames, defaultStates, possibleStates, sys);

    // Create mock TransformationSet
    vector<TemplateMolecule*> tmList;
    TemplateMolecule* tm = new TemplateMolecule(mt);
    tmList.push_back(tm);
    TransformationSet* ts = new TransformationSet(tmList);
    ts->finalize();

    // Create a ReactantTree with an initial capacity of 2
    ReactantTree* tree = new ReactantTree(0, ts, 2);

    // Initial state check
    if (tree->size() != 0) {
        cerr << "Failed: Initial size should be 0, got " << tree->size() << endl;
        failCount++;
    }

    // Add 2 mappings, filling the initial capacity
    MappingSet* ms1 = tree->pushNextAvailableMappingSet();
    tree->confirmPush(ms1->getId(), 1.0);
    MappingSet* ms2 = tree->pushNextAvailableMappingSet();
    tree->confirmPush(ms2->getId(), 2.0);

    if (tree->size() != 2) {
        cerr << "Failed: Size should be 2, got " << tree->size() << endl;
        failCount++;
    }

    if (tree->getRateFactorSum() != 3.0) {
        cerr << "Failed: Expected RateFactorSum 3.0, got " << tree->getRateFactorSum() << endl;
        failCount++;
    }

    // Add a 3rd mapping to trigger expandTree(4)
    MappingSet* ms3 = tree->pushNextAvailableMappingSet();
    tree->confirmPush(ms3->getId(), 3.0);

    // Verify tree state after expansion
    if (tree->size() != 3) {
        cerr << "Failed: Size should be 3 after expansion, got " << tree->size() << endl;
        failCount++;
    }

    if (tree->getRateFactorSum() != 6.0) {
        cerr << "Failed: Expected RateFactorSum 6.0 after expansion, got " << tree->getRateFactorSum() << endl;
        failCount++;
    }

    // Test updateValue on the new elements
    tree->updateValue(ms2->getId(), 5.0); // 2.0 -> 5.0
    if (tree->getRateFactorSum() != 9.0) {
        cerr << "Failed: Expected RateFactorSum 9.0 after update, got " << tree->getRateFactorSum() << endl;
        failCount++;
    }

    // Test a large expansion
    for (int i = 0; i < 97; i++) {
        MappingSet* ms = tree->pushNextAvailableMappingSet();
        tree->confirmPush(ms->getId(), 1.0);
    }

    if (tree->size() != 100) {
        cerr << "Failed: Size should be 100 after large expansion, got " << tree->size() << endl;
        failCount++;
    }

    if (tree->getRateFactorSum() != 106.0) { // 9 + 97*1.0
        cerr << "Failed: Expected RateFactorSum 106.0 after large expansion, got " << tree->getRateFactorSum() << endl;
        failCount++;
    }

    // Clean up
    delete tree;
    delete ts;
    delete sys;

    if (failCount == 0) {
        cout << "All ReactantTree tests passed successfully!" << endl;
    } else {
        cout << "ReactantTree tests failed with " << failCount << " errors." << endl;
        exit(1);
    }
}
