#pragma once
#include <string>

namespace ost_domain
{

    struct Bid
    {
        std::string uid;
        std::string name;
        int bid_no = 0;
        std::string bid_date;
        std::string notes;
        std::string job_id;
        std::string status_uid;
        std::string estimator_uid;
        int measure_base = 0; // 0=Imperial, 1=Metric
        double takeoff_increments = 1.0;
        std::string orig_bid_project_uid;
        int copy_from_bid_no = 0;
        std::string copy_timestamp;

        /// Snap increment in OST units (inches). Converts metric mm to inches.
        double snap_increments_ost() const
        {
            if (takeoff_increments <= 0.0)
                return 0.0;
            return (measure_base == 1)
                       ? takeoff_increments / 25.4
                       : takeoff_increments;
        }
    };

}
