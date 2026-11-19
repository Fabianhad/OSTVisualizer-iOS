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

    // One foreign-key style reference of the bid graph (Windows raw_bid_integrity.RawBidRelationship; the parent
    // column is always UID).
    struct BidRelationship
    {
        const char *child_table;
        const char *child_column;
        const char *parent_table;
        bool required;
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

    extern const std::vector<std::pair<const char *, const char *>> YESNO_COLUMNS;
    extern const std::vector<BidRelationship> BID_RELATIONSHIPS;

    // true when the column is YESNO in the Windows schema (see YESNO_COLUMNS).
    bool is_yesno_column(const std::string &table, const std::string &column);

    // Columns of type DATETIME (14) and IMAGE (30) in the Windows schema (database_creator.py _TABLE_DDL). The iOS DDL types
    // them TEXT/BLOB; the importer converts their values by the Windows type (IMP-20).
    extern const std::vector<std::pair<const char *, const char *>> DATETIME_COLUMNS;
    extern const std::vector<std::pair<const char *, const char *>> IMAGE_COLUMNS;
    bool is_datetime_column(const std::string &table, const std::string &column);
    bool is_image_column(const std::string &table, const std::string &column);

    std::string singular(const std::string &table_name);

}
