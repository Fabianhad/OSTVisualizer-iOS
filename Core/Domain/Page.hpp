#pragma once
#include <string>

namespace ost_domain
{

    struct Page
    {
        std::string uid;
        std::string name;

        // Page geometry (inches)
        double width = 0.0;
        double height = 0.0;
        double scale_factor1 = 1.0;
        double scale_factor2 = 1.0;
        int rotation = 0;
        bool flip_x = false;
        bool flip_y = false;

        std::string folder_uid;
        int page_index = 0;

        // BidPages.Sequence (page order: Sequence, Name, UID) and BidPages.SheetNo (label), as Windows loads them.
        int sequence = 0;
        std::string sheet_no;

        // Image paths (populated from BidPages.ImagePath / OverlayImagePath)
        // For .osp imports, these point to Documents/OST/{bid_name}/{filename}.pdf
        std::string image_path;
        std::string overlay_image_path;

        // Overlay positioning (for future PDF overlay compositing)
        double overlay_offset_x = 0.0;
        double overlay_offset_y = 0.0;
        double overlay_rotation = 0.0;        // radians
        double deskew_rotation_overlay = 0.0; // radians
        bool overlay_resized = false;
        std::string overlay_rect; // stored as text "x,y,w,h"

        // Display mode (BidPages.Show): 0=original, 1=overlay only, 2=composite
        int image_show_mode = 0;
    };

}
