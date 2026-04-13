#include "../../NFcore/NFcore.hh"
#include "../../NFreactions/reactions/reaction.hh"
#include "../../NFreactions/transformations/transformationSet.hh"
#include "../../NFfunction/NFfunction.hh"
#include <iostream>
#include <vector>
#include <stdexcept>

using namespace std;
using namespace NFcore;

namespace NFtest_dor {
    void run() {
        cout << "Testing DOR population error..." << endl;

        System *s = new System("test_sys");

        vector<string> compName;
        vector<string> defaultCompState;
        vector<vector<string>> possibleCompStates;
        vector<bool> isIntegerCompState;
        MoleculeType *molPop = new MoleculeType("PopMol", compName, defaultCompState, possibleCompStates, isIntegerCompState, true, s);
        // molPop->makePopulationType(); // Replaced by constructor argument
        s->addMoleculeType(molPop);

        TemplateMolecule *tmPop = new TemplateMolecule(molPop);

        vector<TemplateMolecule*> templates;
        templates.push_back(tmPop);
        TransformationSet *ts = new TransformationSet(templates);

        // Add local function reference to the population template molecule
        ts->addLocalFunctionReference(tmPop, "ptr", 0);
        ts->finalize();

        // Create a mock CompositeFunction
        vector<string> args;
        args.push_back("ptr");
        vector<string> funcs;
        vector<string> params;
        CompositeFunction *func = new CompositeFunction(s, "func", "1.0", funcs, args, params);

        bool exception_thrown = false;
        try {
            // This should throw a runtime_error since we'll change the exit(1) to throw
            vector<string> argsPointerNameList;
            argsPointerNameList.push_back("ptr");
            DORRxnClass *rxn = new DORRxnClass("DOR_rxn", 1.0, "rate", ts, func, argsPointerNameList, s);
            cout << "Error: DORRxnClass should have thrown an exception!" << endl;
            delete rxn;
        } catch (const exception& e) {
            exception_thrown = true;
            cout << "Success: Caught expected exception: " << e.what() << endl;
        }

        if (!exception_thrown) {
            cout << "Warning: Expected exception was not thrown." << endl;
        }

        // Just let OS clean up memory to avoid segfaults in teardown of mock objects
        // as some are partially initialized or double-freed by System

    }
}
