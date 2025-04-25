#pragma once

#include "IndexSink.h"

struct CaptureSink : IndexSink {
    std::vector<std::string> captured;

    virtual void add(StringView index, size_t, size_t fileOffset) override {
        captured.emplace_back(index.str());
    }

    void reset() { captured.clear(); }
};
