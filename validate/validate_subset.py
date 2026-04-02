import unittest
import os
import sys
from validate import ParametrizedTestCase, TestNFSimFile

if __name__ == "__main__":
    suite = unittest.TestSuite()
    mfolder = './basicModels'
    nIterations = 3 # reduced for speed
    
    for index in ['42', '43', '44']:
        suite.addTest(ParametrizedTestCase.parametrize(TestNFSimFile, param={'num': index,
                    'odir': mfolder, 'iterations': nIterations}))

    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)
