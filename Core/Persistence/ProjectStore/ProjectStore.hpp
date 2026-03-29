// Layer: Persistence
// Depends on: Domain (ProjectData, DatabaseHierarchy), OstXmlParser
// Forbidden: UIKit, Foundation, Renderer
// All SQL and DB ↔ domain mapping lives here. Called only by ImportService.
#pragma once
#include "../../Data/ProjectData.hpp"
#include "../../Domain/DatabaseHierarchy.hpp"
#include "../../Domain/RenderConfig.hpp"
#include "../../Import/OstXmlParser.hpp"
#include <string>
#include <vector>

namespace ost_store
{

    bool create_schema(const std::string &db_path);

    bool import_project(const std::string &db_path,
                        const ost_import::OstRawData &raw_data);

    bool is_bid_imported(const std::string &db_path,
                         const std::string &bid_uid);

    ost_domain::DatabaseHierarchy load_hierarchy(const std::string &db_path);

    ost_data::ProjectData load_project(const std::string &db_path,
                                       const std::string &bid_uid);

    bool delete_project(const std::string &db_path,
                        const std::string &bid_uid);

    bool permanently_delete_project(const std::string &db_path,
                                    const std::string &bid_uid);

    std::string create_folder(const std::string &db_path,
                              const std::string &name);

    bool rename_folder(const std::string &db_path,
                       const std::string &folder_uid,
                       const std::string &new_name);

    bool delete_folder(const std::string &db_path,
                       const std::string &folder_uid);

    bool assign_bid_to_folder(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &folder_uid);

    bool remove_bid_from_folder(const std::string &db_path,
                                const std::string &bid_uid);

    ost_domain::RenderConfig load_render_config(const std::string &db_path);
    bool save_render_config(const std::string &db_path,
                            const ost_domain::RenderConfig &config);

    bool set_layer_visibility(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &layer_uid,
                              bool visible);

    bool save_takeoff_position(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::string &takeoff_uid,
                               const std::vector<double> &position);

    std::string insert_takeoff(const std::string &db_path,
                               const std::string &bid_uid,
                               const ost_domain::Takeoff &takeoff);

    bool delete_takeoff(const std::string &db_path,
                        const std::string &takeoff_uid);

    bool set_takeoff_negative(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &takeoff_uid,
                              bool is_negative);

    bool update_condition(const std::string &db_path,
                          const ost_domain::Condition &cdn);

    bool shift_condition_ref_nos(const std::string &db_path,
                                 const std::string &bid_uid,
                                 int from_ref_no,
                                 const std::string &exclude_uid);

    std::string duplicate_condition(const std::string &db_path,
                                    const std::string &bid_uid,
                                    const std::string &condition_uid);

    bool delete_condition(const std::string &db_path,
                          const std::string &bid_uid,
                          const std::string &condition_uid);

}
