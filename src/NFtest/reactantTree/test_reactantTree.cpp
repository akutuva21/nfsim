#include "test_reactantTree.hh"
#include "../../NFreactions/transformations/transformation.hh"
#include "../../NFreactions/transformations/transformationSet.hh"
#include "../../NFcore/NFcore.hh"
#include <iostream>
#include <cstdlib>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

using namespace std;
using namespace NFcore;

namespace NFtest_reactantTree {

void test_remove_from_empty_tree() {
    cout << "  - testing ReactantTree::removeMappingSet empty tree exit(1)... " << flush;

#ifndef _WIN32
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        // Redirect cerr to /dev/null to suppress the expected error message
        if (freopen("/dev/null", "w", stderr) == NULL) {
            // handle error if needed, but not strictly required for test
        }

        System* s = new System("test_sys");
        vector<string> compNames;
        vector<string> defaultStates;
        vector<vector<string>> possibleStates;
        MoleculeType* mt = new MoleculeType("TestMol", compNames, defaultStates, possibleStates, s);
        TemplateMolecule* tm = new TemplateMolecule(mt);
        vector<TemplateMolecule*> tmList;
        tmList.push_back(tm);
        TransformationSet* ts = new TransformationSet(tmList);

        // Create an empty ReactantTree
        ReactantTree* tree = new ReactantTree(0, ts, 4);

        // This should call exit(1)
        tree->removeMappingSet(0);

        // If we get here, the test failed
        exit(0);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
            cout << "passed." << endl;
        } else {
            cout << "FAILED! Expected exit(1), got " << WEXITSTATUS(status) << endl;
            exit(1);
        }
    } else {
        cerr << "fork() failed!" << endl;
        exit(1);
    }
#else
    cout << "skipped (unsupported on Windows)." << endl;
#endif
}

void run() {
    cout << "Running ReactantTree tests..." << endl;
    test_remove_from_empty_tree();
    cout << "ReactantTree tests complete." << endl;
}

}
