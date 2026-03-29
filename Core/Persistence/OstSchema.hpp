#pragma once
#include <vector>
#include <string>
#include <utility>

namespace ost_schema
{

    struct DefaultLayerSeed
    {
        const char *name;
        int show;
        int is_locked;
        int sequence;
    };

    extern const std::vector<const char *> TABLE_DDL;
    extern const std::vector<std::pair<const char *, const char *>> FK_INDEXES;
    extern const std::vector<const char *> UNIQUE_INDEX_DDL;
    extern const std::vector<int> SCHEMA_VERSIONS;
    extern const std::vector<DefaultLayerSeed> DEFAULT_LAYERS;

    extern const std::vector<const char *> BID_SECTIONS;
    extern const std::vector<const char *> PAGE_SECTIONS;
    extern const std::vector<const char *> BID_TAIL_SECTIONS;
    extern const std::vector<const char *> GLOBAL_SECTIONS;

    std::string singular(const std::string &table_name);

}
