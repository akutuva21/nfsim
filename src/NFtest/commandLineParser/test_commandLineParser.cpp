#include "test_commandLineParser.hh"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <map>
#include <vector>
#include <string>
#include "../../NFinput/NFinput.hh"

using namespace std;
using namespace NFinput;

void NFtest_commandLineParser::run() {
    cout << "Running CommandLineParser tests..." << endl;

    // We can redirect cout to test outputs if necessary
    streambuf* oldCout = cout.rdbuf();

    try {
        // Test parseArguments
        {
            map<string, string> argMap;
            const char* argv[] = {"nfsim", "-xml", "model.xml", "-sim", "10", "-ut"};
            int argc = 6;

            stringstream out;
            cout.rdbuf(out.rdbuf());

            bool result = parseArguments(argc, argv, argMap);

            cout.rdbuf(oldCout);

            if (!result) {
                throw runtime_error("parseArguments failed on valid input.");
            }
            if (argMap.size() != 3) {
                throw runtime_error("Expected 3 arguments parsed, got " + to_string(argMap.size()));
            }
            if (argMap["xml"] != "model.xml") throw runtime_error("Failed to parse -xml flag");
            if (argMap["sim"] != "10") throw runtime_error("Failed to parse -sim flag");
            if (argMap["ut"] != "") throw runtime_error("Failed to parse -ut flag");
        }

        // Test invalid arg (repeated flag)
        {
            map<string, string> argMap;
            const char* argv[] = {"nfsim", "-xml", "model1.xml", "-xml", "model2.xml"};
            int argc = 5;

            stringstream out;
            cout.rdbuf(out.rdbuf());

            bool result = parseArguments(argc, argv, argMap);

            cout.rdbuf(oldCout);

            if (result) {
                throw runtime_error("Expected parseArguments to fail on repeated flag.");
            }
        }

        // Test invalid arg (---)
        {
            map<string, string> argMap;
            const char* argv[] = {"nfsim", "--"};
            int argc = 2;

            stringstream out;
            cout.rdbuf(out.rdbuf());

            bool result = parseArguments(argc, argv, argMap);

            cout.rdbuf(oldCout);

            if (result) {
                throw runtime_error("Expected parseArguments to fail on --.");
            }
        }

        // Test invalid arg (-)
        {
            map<string, string> argMap;
            const char* argv[] = {"nfsim", "-"};
            int argc = 2;

            stringstream out;
            cout.rdbuf(out.rdbuf());

            bool result = parseArguments(argc, argv, argMap);

            cout.rdbuf(oldCout);

            if (result) {
                throw runtime_error("Expected parseArguments to fail on -.");
            }
        }

        // Test parseAsInt
        {
            map<string, string> argMap;
            argMap["sim"] = "100";
            argMap["invalid"] = "abc";

            stringstream out;
            cout.rdbuf(out.rdbuf());

            int val1 = parseAsInt(argMap, "sim", 10);
            int val2 = parseAsInt(argMap, "missing", 20);
            int val3 = parseAsInt(argMap, "invalid", 30);

            cout.rdbuf(oldCout);

            if (val1 != 100) throw runtime_error("Expected parseAsInt to return 100");
            if (val2 != 20) throw runtime_error("Expected parseAsInt to return default 20");
            if (val3 != 30) throw runtime_error("Expected parseAsInt to return default 30 on invalid input");

            string output = out.str();
            if (output.find("Warning: I couldn't parse your flag '-invalid abc' as an integer.") == string::npos) {
                throw runtime_error("Expected error message in output for parseAsInt, got: " + output);
            }
        }

        // Test parseAsDouble
        {
            map<string, string> argMap;
            argMap["val"] = "3.14";
            argMap["invalid"] = "abc";

            stringstream out;
            cout.rdbuf(out.rdbuf());

            double val1 = parseAsDouble(argMap, "val", 1.0);
            double val2 = parseAsDouble(argMap, "missing", 2.0);
            double val3 = parseAsDouble(argMap, "invalid", 3.0);

            cout.rdbuf(oldCout);

            if (val1 != 3.14) throw runtime_error("Expected parseAsDouble to return 3.14");
            if (val2 != 2.0) throw runtime_error("Expected parseAsDouble to return default 2.0");
            if (val3 != 3.0) throw runtime_error("Expected parseAsDouble to return default 3.0 on invalid input");

            string output = out.str();
            if (output.find("Warning: I couldn't parse your flag '-invalid abc' as a double.") == string::npos) {
                throw runtime_error("Expected error message in output for parseAsDouble, got: " + output);
            }
        }

        // Test parseAsCommaSeparatedSequence
        {
            map<string, string> argMap;
            argMap["seq"] = "1, 2, 3,4";

            vector<int> seq;
            parseAsCommaSeparatedSequence(argMap, "seq", seq);

            if (seq.size() != 4 || seq[0] != 1 || seq[1] != 2 || seq[2] != 3 || seq[3] != 4) {
                throw runtime_error("parseAsCommaSeparatedSequence failed");
            }
        }

        // Test createSystemDumper
        {
            NFcore::System s("test");
            bool result = createSystemDumper("[1;2]->/tmp", &s, false);
            if (!result) throw runtime_error("createSystemDumper failed");
        }

        // Test parseSequence
        {
            vector<double> outTimes;
            bool result = parseSequence("1:2:5", outTimes);

            if (!result || outTimes.size() != 3 || outTimes[0] != 1 || outTimes[1] != 3 || outTimes[2] != 5) {
                throw runtime_error("parseSequence failed for start:step:end");
            }

            outTimes.clear();
            result = parseSequence("1:5", outTimes);
            if (!result || outTimes.size() != 5 || outTimes[0] != 1 || outTimes[4] != 5) {
                throw runtime_error("parseSequence failed for start:end");
            }

            stringstream out;
            cout.rdbuf(out.rdbuf());
            outTimes.clear();
            result = parseSequence("5:1:2", outTimes); // start > end
            cout.rdbuf(oldCout);
            if (result) throw runtime_error("parseSequence should fail for start > end");
        }

    } catch (...) {
        cout.rdbuf(oldCout);
        throw;
    }

    cout.rdbuf(oldCout);
    cout << "CommandLineParser tests passed!" << endl;
}
