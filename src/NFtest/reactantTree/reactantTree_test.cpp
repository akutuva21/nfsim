#include "reactantTree_test.hh"
#include "../../NFreactions/reactantLists/reactantTree.hh"
#include "../../NFreactions/transformations/transformationSet.hh"
#include <iostream>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

using namespace std;
using namespace NFcore;

void NFtest_reactantTree::run()
{
    cout << "Running ReactantTree tests..." << endl;

    int failCount = 0;

    // Test for exit(1) on removeMappingSet from empty ReactantTree
    cout << "  Testing removeMappingSet on empty tree (expecting exit(1))..." << endl;

#ifndef _WIN32
    // We will fork a process to test that it calls exit(1)
    pid_t pid = fork();
    if (pid == 0) {
        // In the child process
        // Redirect cerr so we don't spam the console if not needed, but here it's expected
        if (freopen("/dev/null", "w", stderr) == nullptr) {
            // Ignore if freopen fails
        }

        // Create an empty transformation set
        vector<TemplateMolecule*> tempMols;
        TransformationSet ts(tempMols);
        ts.finalize(); // Finalize to prevent "TransformationSet cannot generate blank mapping if it is not finalized!"

        // Create ReactantTree
        ReactantTree* tree = new ReactantTree(0, &ts, 10);

        // Call the method that should exit
        tree->removeMappingSet(123);

        // If we get here, the test failed (exit was not called)
        exit(0); // Return 0 to indicate failure of the test
    } else if (pid > 0) {
        // In the parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 1) {
                cout << "    Success: Empty tree removeMappingSet exited with code 1." << endl;
            } else {
                cout << "    Failure: Child process exited with code " << exit_status << " instead of 1." << endl;
                failCount++;
            }
        } else {
            cout << "    Failure: Child process did not exit normally." << endl;
            failCount++;
        }
    } else {
        cerr << "Fork failed!" << endl;
        failCount++;
    }
#else
    cout << "    Skipping exit(1) test on Windows as fork() is not available." << endl;
#endif

    cout << "  Testing single-mapping fast path transitions..." << endl;
    System testSystem("ReactantTree test");
    vector<string> componentNames;
    componentNames.push_back("site");
    MoleculeType *testMoleculeType = new MoleculeType(
            "TreeTest", componentNames, &testSystem);
    TemplateMolecule *testTemplate = new TemplateMolecule(testMoleculeType);
    vector<TemplateMolecule*> tempMols;
    tempMols.push_back(testTemplate);
    TransformationSet ts(tempMols);
    ts.finalize();
    ReactantTree tree(0, &ts, 4);

    MappingSet *first = tree.pushNextAvailableMappingSet();
    unsigned int firstId = first->getId();
    tree.confirmPush(firstId, 2.0);
    if (tree.size() != 1 || tree.getRateFactorSum() != 2.0 ||
            tree.getRateFactor(0) != 2.0) {
        cout << "    Failure: initial single mapping state is incorrect." << endl;
        failCount++;
    }

    MappingSet *picked = 0;
    tree.pickReactantFromValue(picked, 1.0, 1.0);
    if (picked != first) {
        cout << "    Failure: single mapping selection returned the wrong mapping." << endl;
        failCount++;
    }

    tree.updateValue(firstId, 3.0);
    tree.pickReactantFromValue(picked, 2.0, 1.0);
    if (tree.getRateFactorSum() != 3.0 || tree.getRateFactor(0) != 3.0 ||
            picked != first) {
        cout << "    Failure: single mapping rate update is incorrect." << endl;
        failCount++;
    }

    MappingSet *second = tree.pushNextAvailableMappingSet();
    unsigned int secondId = second->getId();
    tree.confirmPush(secondId, 5.0);
    tree.pickReactantFromValue(picked, 4.0, 1.0);
    if (tree.size() != 2 || tree.getRateFactorSum() != 8.0 ||
            picked != second) {
        cout << "    Failure: tree materialization after insertion is incorrect." << endl;
        failCount++;
    }

    tree.removeMappingSet(secondId);
    tree.pickReactantFromValue(picked, 2.0, 1.0);
    if (tree.size() != 1 || tree.getRateFactorSum() != 3.0 ||
            tree.getRateFactor(0) != 3.0 || picked != first) {
        cout << "    Failure: tree did not restore the single mapping state." << endl;
        failCount++;
    }

    tree.removeMappingSet(firstId);

    ReactantTree expansionTree(0, &ts, 4);
    vector<unsigned int> expansionIds;
    double expansionRateSum = 0.0;
    for (unsigned int i = 0; i < 5; ++i) {
        MappingSet *mapping = expansionTree.pushNextAvailableMappingSet();
        expansionIds.push_back(mapping->getId());
        double rate = static_cast<double>(i + 1);
        expansionRateSum += rate;
        expansionTree.confirmPush(mapping->getId(), rate);
    }
    if (expansionTree.size() != 5 ||
            expansionTree.getRateFactorSum() != expansionRateSum) {
        cout << "    Failure: tree expansion did not preserve rate factors." << endl;
        failCount++;
    }
    for (vector<unsigned int>::const_iterator it = expansionIds.begin();
            it != expansionIds.end(); ++it) {
        expansionTree.removeMappingSet(*it);
    }
    if (expansionTree.size() != 0 || expansionTree.getRateFactorSum() != 0.0) {
        cout << "    Failure: expanded tree cleanup is incorrect." << endl;
        failCount++;
    }

    if (failCount == 0) {
        cout << "All ReactantTree tests passed successfully!" << endl;
    } else {
        cout << "ReactantTree tests failed with " << failCount << " errors." << endl;
        exit(1);
    }
}
