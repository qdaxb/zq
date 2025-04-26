#pragma once

#include "LineIndexer.h"

#include <string>
#include <vector>
#include <algorithm>

// A LineIndexer that indexes based on a separator and field number.
class FieldIndexer : public LineIndexer {
    std::string separator_;
    std::vector<int> fields_;
    std::vector<int> negFields_;
public:
    FieldIndexer(std::string separator, std::vector<int> fields) : separator_(
            std::move(separator)) {
        std::sort(fields.begin(), fields.end());
        for (auto iter = fields.begin(); iter != fields.end(); iter++) {
            if (*iter > 0) {
                fields_.push_back(*iter);
            } else {
                negFields_.push_back(*iter);
            }
        }
    }

    void index(IndexSink &sink, StringView line,size_t fileOffset) override;
};
