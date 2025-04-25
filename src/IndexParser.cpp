#include "IndexParser.h"
#include "FieldIndexer.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

void IndexParser::buildIndexes(Index::Builder *builder, ConsoleLog &log) {
    std::ifstream in(fileName_, std::ifstream::in);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    cJSON *root = cJSON_Parse(contents.c_str());
    if (root == nullptr) {
        throw std::runtime_error("Could not parse the json config file. "
                                         "Careful, this is a strict parser.");
    }
    if (!cJSON_HasObjectItem(root, "indexes")) {
        throw std::runtime_error("No indexes to be found in the config file.");
    }
    cJSON *indexes = cJSON_GetObjectItem(root, "indexes");
    auto arraySize = cJSON_GetArraySize(indexes);
    for (int i = 0; i < arraySize; i++) {
        parseIndex(cJSON_GetArrayItem(indexes, i), builder, log);
    }
}
static void split(const std::string& s, std::vector<int>& tokens, const std::string& delimiters = " ")
{
    std::string::size_type lastPos = s.find_first_not_of(delimiters, 0);
    std::string::size_type pos = s.find_first_of(delimiters, lastPos);
    while (std::string::npos != pos || std::string::npos != lastPos) {
        tokens.push_back(std::stoi(s.substr(lastPos, pos - lastPos)));
        lastPos = s.find_first_not_of(delimiters, pos);
        pos = s.find_first_of(delimiters, lastPos);
    }
}
void IndexParser::parseIndex(cJSON *index, Index::Builder *builder,
                             ConsoleLog &/*log*/) {
    if (!cJSON_HasObjectItem(index, "type")) {
        throw std::runtime_error(
                "All indexes must have a type field, i.e. Regex, Field, ");
    }
    std::string indexName = "default";
    if (cJSON_HasObjectItem(index, "name")) {
        indexName = cJSON_GetObjectItem(index, "name")->valuestring;
    }
    std::string type = cJSON_GetObjectItem(index, "type")->valuestring;
    Index::IndexConfig config{};
    config.numeric = getBoolean(index, "numeric");
    config.unique = getBoolean(index, "unique");
    config.sparse = getBoolean(index, "sparse");
    config.indexLineOffsets = getBoolean(index, "indexLineOffsets");

 if (type == "field") {
        auto delimiter = getOrThrowStr(index, "delimiter");
        auto fieldNum = getOrThrowStr(index, "fieldNum");
        std::vector<int> fieldNums;
        split(fieldNum, fieldNums, ",");
        std::ostringstream name;
        name << "Field " << fieldNum << " delimited by '"
             << delimiter << "'";
        builder->addIndexer(fieldNums,  config,
                            std::unique_ptr<LineIndexer>(
                                    new FieldIndexer(delimiter, fieldNums)));
    } else {
        throw std::runtime_error("unknown index " + type);
    }
}

bool IndexParser::getBoolean(cJSON *index, const char *field) const {
    return cJSON_GetObjectItem(index, field)
           && cJSON_GetObjectItem(index, field)->type == cJSON_True;
}

std::string IndexParser::getOrThrowStr(cJSON *index, const char *field) const {
    auto *item = cJSON_GetObjectItem(index, field);
    if (!item)
        throw std::runtime_error("Could not parse the json config file. Field '"
                                 + std::string(field) + "' was missing");
    return item->valuestring;
}

unsigned IndexParser::getOrThrowUint(cJSON *index, const char *field) const {
    auto *item = cJSON_GetObjectItem(index, field);
    if (!item)
        throw std::runtime_error("Could not parse the json config file. Field '"
                                 + std::string(field) + "' was missing");
    if (item->valueint < 0)
        throw std::runtime_error("Could not parse the json config file. Field '"
                                 + std::string(field) + "' was negative");
    return static_cast<unsigned>(item->valueint);
}
