#include "File.h"
#include "Index.h"
#include "LineSink.h"
#include "ConsoleLog.h"

#include <tclap/CmdLine.h>

#include <iostream>
#include <stdexcept>
#include <utility>
#include "RangeFetcher.h"

using namespace std;
using namespace TCLAP;

namespace {

struct PrintSink : LineSink {
    std::string prefix;

    explicit PrintSink(string prefix) : prefix(prefix) {}

    bool onLine(size_t /*l*/, size_t, std::string query, const char *line,
                size_t /*length*/) override {
        std::string myLine;
        std::stringstream ss(line);
        std::getline(ss, myLine, '\n');
        if (myLine.find(query) != std::string::npos) {
            std::cout << prefix << myLine << endl;
        }
        return true;
    }
};

struct PrintHandler : RangeFetcher::Handler {
    Index &index;
    LineSink &sink;
    const bool printSep;
    const std::string sep;

    PrintHandler(Index &index, LineSink &sink, bool printSep,
                 std::string sep)
            : index(index), sink(sink), printSep(printSep),
              sep(std::move(sep)) {}

    void onLine(uint64_t /*line*/) override {
//        index.getLine(line, sink);
    }

    void onSeparator() override {
        if (printSep)
            cout << sep << endl;
    }
};


}

int Main(int argc, const char *argv[]) {
    CmdLine cmd("Lookup indices in a compressed text file");

    UnlabeledValueArg<string> query(
            "query", "Query for <query>", true, "", "<query>", cmd);
    UnlabeledMultiArg<string> inputFile(
            "input-file", "Read input from <file>", true, "<files>", cmd);

    SwitchArg verbose("v", "verbose", "Be more verbose", cmd);
    SwitchArg debug("", "debug", "Be even more verbose", cmd);
    SwitchArg forceColour("", "colour", "Use colour even on non-TTY", cmd);
    SwitchArg forceColor("", "color", "Use color even on non-TTY", cmd);
    SwitchArg warnings("w", "warnings", "Log warnings at info level", cmd);

    ValueArg<string> queryIndexArg("i", "index",
                                   "Use specified index for searching", false,
                                   "", "index", cmd);


    cmd.parse(argc, argv);

    ConsoleLog log(
            debug.isSet() ? Log::Severity::Debug : verbose.isSet()
                                                   ? Log::Severity::Info
                                                   : Log::Severity::Warning,
            forceColour.isSet() || forceColor.isSet(), warnings.isSet());

    auto compressedFiles = inputFile.getValue();
    for (auto iter = compressedFiles.begin();
         iter != compressedFiles.end(); iter++) {
        auto compressedFile = *iter;
        if (compressedFile.find(".zindex") != std::string::npos) {
            continue;
        }

        try {
            File in(fopen(compressedFile.c_str(), "rb"));
            if (in.get() == nullptr) {
                log.error("Could not open ", compressedFile, " for reading");
                return 1;
            }

            auto indexFile = compressedFile + ".zindex";
            auto index = Index::load(log, std::move(in), indexFile.c_str(), false);
            auto queryIndex = queryIndexArg.isSet() ? queryIndexArg.getValue()
                                                    : "-1";

            uint64_t before = 0u;
            uint64_t after = 0u;
            log.debug("Fetching context of ", before, " lines before and ",
                      after,
                      " lines after");
            PrintSink sink(compressedFile + ":");
            PrintHandler ph(index, sink, false, "");

            index.queryIndex(queryIndex, query.getValue(), sink);
        } catch (const exception &e) {
            log.error(
                    "processing ", compressedFile.c_str(), " error: ",e.what());
            return 1;
        }

    }

    return 0;
}

int main(int argc, const char *argv[]) {
    try {
        return Main(argc, argv);
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
}
