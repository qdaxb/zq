#include "File.h"
#include "Index.h"
#include "ConsoleLog.h"
#include "FieldIndexer.h"
#include "IndexParser.h"

#include <tclap/CmdLine.h>

#include <iostream>
#include <stdexcept>
#include <limits.h>

using namespace std;
using namespace TCLAP;

namespace {

string getRealPath(const string &relPath) {
    char realPathBuf[PATH_MAX];
    auto result = realpath(relPath.c_str(), realPathBuf);
    if (result == nullptr) return relPath;
    return string(relPath);
}

}

static void split(const std::string &s, std::vector<int> &tokens,
                  const std::string &delimiters = " ") {
    std::string::size_type lastPos = s.find_first_not_of(delimiters, 0);
    std::string::size_type pos = s.find_first_of(delimiters, lastPos);
    while (std::string::npos != pos || std::string::npos != lastPos) {
        tokens.push_back(std::stoi(s.substr(lastPos, pos - lastPos)));
        lastPos = s.find_first_not_of(delimiters, pos);
        pos = s.find_first_of(delimiters, lastPos);
    }
}

int Main(int argc, const char *argv[]) {
    CmdLine cmd("Create indices in a compressed text file");
    UnlabeledValueArg<string> inputFile(
            "input-file", "Read input from <file>", true, "", "file", cmd);
    SwitchArg verbose("v", "verbose", "Be more verbose", cmd);
    SwitchArg debug("", "debug", "Be even more verbose", cmd);
    SwitchArg forceColour("", "colour", "Use colour even on non-TTY", cmd);
    SwitchArg forceColor("", "color", "Use color even on non-TTY", cmd);
    SwitchArg warnings("w", "warnings", "Log warnings at info level", cmd);
    ValueArg<uint64_t> checkpointEvery(
            "", "checkpoint-every",
            "Create a compression checkpoint every <bytes>", false,
            0, "bytes", cmd);
    ValueArg<string> field("f", "field", "Create an index using field <num> "
                                         "(delimited by -d/--delimiter, 1-based)",
                           false, "", "num", cmd);
    ValueArg<string> delimiterArg(
            "d", "delimiter", "Use <delim> as the field delimiter", false, " ",
            "delim", cmd);
    SwitchArg tabDelimiterArg(
            "", "tab-delimiter", "Use a tab character as the field delimiter",
            cmd);
    cmd.parse(argc, argv);

    ConsoleLog log(
            debug.isSet() ? Log::Severity::Debug : verbose.isSet()
                                                   ? Log::Severity::Info
                                                   : Log::Severity::Warning,
            forceColour.isSet() || forceColor.isSet(), warnings.isSet());

    try {
        auto realPath = getRealPath(inputFile.getValue());
        File in(fopen(realPath.c_str(), "rb"));
        if (in.get() == nullptr) {
            log.debug("Unable to open ", inputFile.getValue(), " (as ",
                      realPath,
                      ")");
            log.error("Could not open ", inputFile.getValue(), " for reading");
            return 1;
        }

        auto outputFile = inputFile.getValue() + ".zindex";
        Index::Builder builder(log, std::move(in), realPath, outputFile);


        Index::IndexConfig config{};
        config.numeric = false;
        config.unique = false;
        config.sparse = false;

        auto delimiter = delimiterArg.getValue();
        if (tabDelimiterArg.isSet() && delimiterArg.isSet()) {
            log.error("Cannot set both --delimiter and --tab-delimiter");
            return 1;
        }
        if (tabDelimiterArg.isSet())
            delimiter = "\t";


        if (field.isSet()) {
            ostringstream name;
            name << "Field " << field.getValue() << " delimited by '"
                 << delimiter << "'";
            std::vector<int> fields;
            split(field.getValue(), fields, ",");
            builder.addIndexer(fields, config,
                               std::unique_ptr<LineIndexer>(
                                       new FieldIndexer(delimiter, fields)
                               )
            );

        }
        if (checkpointEvery.isSet())
            builder.indexEvery(checkpointEvery.getValue());
        builder.build();
    } catch (const exception &e) {
        log.error(e.what());
        return 1;
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
