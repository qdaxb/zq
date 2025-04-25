#include <cstring>
#include "FieldIndexer.h"
#include "IndexSink.h"


static char *
backwards_memcmp(const char *haystack, const char *end, const char *needle) {
    size_t haylen = end - haystack;

    if (*needle == '\0')
        return (char *)haystack;

    size_t needlelen = strlen(needle);
    if (needlelen > haylen)
        return NULL;

    const char *p = end - needlelen;
    for (;;) {
        if (memcmp(p, needle, needlelen) == 0) {
            return (char *)p;
        }
        if (p == haystack)
            return NULL;
        --p;
    }
}

void FieldIndexer::index(IndexSink &sink, StringView line, size_t fileOffset) {
    auto ptr = line.begin();
    auto end = line.end();
    if (!fields_.empty()) {
        auto iter = fields_.begin();
        for (auto i = 1; i <= fields_.back(); ++i) {
            auto nextSep = static_cast<const char *>(
                    memmem(ptr, end - ptr, separator_.c_str(),
                           separator_.size()));
            if (!nextSep) break;
            ptr = nextSep + separator_.size();
            if (i == (*iter)) {
                auto lastSep = memmem(ptr, end - ptr, separator_.c_str(),
                                      separator_.size());
                if (lastSep) end = static_cast<const char *>(lastSep);
                if (ptr != end)
                    sink.add(StringView(ptr, end - ptr), ptr - line.begin(),
                             std::to_string(i),
                             fileOffset);
            }
        }
    }

    if (!negFields_.empty()) {
        ptr = line.begin();
        end = line.end();
        auto currentEnd = end;
        auto negIter = negFields_.rbegin();
        for (auto i = -1; i >= negFields_.front(); i--) {
            loop:
            auto nextSep = backwards_memcmp(ptr, currentEnd,
                                            separator_.c_str());
            if (!nextSep) break;
            if (nextSep + separator_.size() == currentEnd) {
                currentEnd = nextSep;
                goto loop;
            }
            if (i == (*negIter)) {
                sink.add(StringView(nextSep + separator_.size(),
                                    currentEnd - nextSep - separator_.size()),
                         nextSep - line.begin(), std::to_string(i),
                         fileOffset);
                negIter++;
            }
            currentEnd = nextSep;
            if (currentEnd <= ptr) {
                break;
            }
        }
    }
}