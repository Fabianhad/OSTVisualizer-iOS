#pragma once
#include "Types.hpp"
#include <string>

namespace ost_domain
{

    struct Condition
    {

        std::string uid;
        std::string name;
        ConditionType condition_type = ConditionType::Linear;
        double thickness = 0.0;
        double height = 0.0;
        double width = 0.0;
        double depth = 0.0;
        double rise = 0.0;
        double run = 0.0;
        ShapeId shape = ShapeId::Square;
        int color_fill = 0;
        int color_line = 0;
        double z_value = 0.0;
        bool is_top = false;
        Pattern pattern = Pattern::Solid;
        double display_size = 100.0;
        bool backout = false;
        bool drop_run = false;
        double drop_value = 0.0;
        std::string layer_uid;
        std::string cdn_type_uid;
        std::string cdn_type_name;
        std::string condition_folder_uid;
        int ref_no = 0;
        std::string notes;
        double spacing = 0.0;
        int uom1 = 0;
        int uom2 = 0;
        int uom3 = 0;
        int calc_type1 = 0;
        int calc_type2 = 0;
        int calc_type3 = 0;
        double round_up = 0.0;
        bool round_quantity = false;
        bool connect = false;
        double connect_tolerance = 6.0;
        bool trim = false;
        bool is_curved_segment = false;
        bool grid = false;
        double grid_size1 = 0.0;
        double grid_size2 = 0.0;
        double gap = 0.0;
        bool display_dimension = false;
        bool display_name = false;
        bool display_grid_while_drawing = false;
        int snap_to_linear = -1;

        bool is_linear() const { return condition_type == ConditionType::Linear; }
        bool is_area() const { return condition_type == ConditionType::Area; }
        bool is_count() const { return condition_type == ConditionType::Count; }
        bool is_attachment() const { return condition_type == ConditionType::Attachment; }

        // ColorFill is a BGR integer: r = c & 0xFF, g = (c >> 8) & 0xFF, b = (c >> 16) & 0xFF
        // (Windows color_service.int_to_hex). A negative value (system colour / alpha bits) uses the low bytes of its
        // two's complement, exactly like the Python ints there (-1 -> white): there is no grey fallback.
        // Every place that turns a ColorFill into a colour (meshes, 2D overlays, list and edit swatches) uses this.
        static void rgb_from_color_fill(int color_fill, float &r, float &g, float &b)
        {
            const unsigned int c = static_cast<unsigned int>(color_fill);
            r = (c & 0xFFu) / 255.0f;
            g = ((c >> 8) & 0xFFu) / 255.0f;
            b = ((c >> 16) & 0xFFu) / 255.0f;
        }

        void get_rgb(float &r, float &g, float &b) const { rgb_from_color_fill(color_fill, r, g, b); }
    };

}
