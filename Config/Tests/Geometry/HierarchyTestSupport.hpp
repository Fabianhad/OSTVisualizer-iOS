// Shared helpers of the hole / attachment validation tests (GEO-M02): small builders for takeoffs, conditions and
// project-like families, and the parsers of the generated oracle tables. Not a test_*.cpp file.
#pragma once

#include "TestHarness.hpp"

#include "Geometry/hierarchy_validation.hpp"
#include "Geometry/polygon_region.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace geo_test
{
    using ost_domain::Condition;
    using ost_domain::ConditionType;
    using ost_domain::ShapeId;
    using ost_domain::Takeoff;
    using ost_geom::Polygon;

    inline std::vector<double> square(double left, double top, double right, double bottom)
    {
        return {left, top, right, top, right, bottom, left, bottom};
    }

    inline Takeoff make_takeoff(const std::string &uid, const std::string &condition_uid,
                                std::vector<double> position, const std::string &parent = "0",
                                const std::string &page = "pg1", double rotation = 0.0)
    {
        Takeoff t;
        t.uid = uid;
        t.condition_uid = condition_uid;
        t.page_uid = page;
        t.area_uid = "area-1";
        t.position = std::move(position);
        t.parent_uid = parent;
        t.rotation = rotation;
        return t;
    }

    inline Condition make_condition(const std::string &uid, ConditionType type, ShapeId shape = ShapeId::Square,
                                    double width = 0.0, double depth = 0.0, double display_size = 100.0)
    {
        Condition c;
        c.uid = uid;
        c.condition_type = type;
        c.shape = shape;
        c.width = width;
        c.depth = depth;
        c.display_size = display_size;
        return c;
    }

    // A project-like family: the stored takeoffs (their order is the Windows dict order) and the conditions.
    struct Family
    {
        std::vector<Takeoff> takeoffs;
        std::unordered_map<std::string, Condition> conditions;

        void add_condition(const Condition &c) { conditions[c.uid] = c; }
        void add(const Takeoff &t) { takeoffs.push_back(t); }
        Takeoff &at(const std::string &uid)
        {
            for (auto &t : takeoffs)
                if (t.uid == uid)
                    return t;
            FAIL_TEST("no takeoff " + uid);
            return takeoffs.front();
        }
        ost_hierarchy::HierarchyModel model() const { return ost_hierarchy::HierarchyModel(takeoffs, conditions); }
    };

    // Windows HierarchyDragTests._conditions(): ca (area), cat (attachment, rectangle 10 x 4), cl (linear).
    inline Family windows_family(const std::vector<Takeoff> &takeoffs)
    {
        Family f;
        f.add_condition(make_condition("ca", ConditionType::Area));
        f.add_condition(make_condition("cat", ConditionType::Attachment, ShapeId::Rectangle, 10.0, 4.0));
        f.add_condition(make_condition("cl", ConditionType::Linear));
        f.takeoffs = takeoffs;
        return f;
    }

    inline std::vector<double> parse_numbers(const std::string &token)
    {
        std::vector<double> out;
        std::string body = token;
        if (!body.empty() && body.front() == '[')
            body = body.substr(1, body.size() - 2);
        std::stringstream ss(body);
        std::string item;
        while (std::getline(ss, item, ','))
            if (!item.empty())
                out.push_back(std::strtod(item.c_str(), nullptr));
        return out;
    }

    inline std::vector<std::string> split_fields(const std::string &line, char sep = ' ')
    {
        std::vector<std::string> out;
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, sep))
            out.push_back(item);
        return out;
    }

    // "uid=[1,2,3];uid2=[...]" -> map; "-" -> empty.
    inline std::unordered_map<std::string, std::vector<double>> parse_position_map(const std::string &token)
    {
        std::unordered_map<std::string, std::vector<double>> out;
        if (token == "-")
            return out;
        for (const std::string &entry : split_fields(token, ';'))
        {
            const size_t eq = entry.find('=');
            out[entry.substr(0, eq)] = parse_numbers(entry.substr(eq + 1));
        }
        return out;
    }

    inline std::unordered_map<std::string, double> parse_number_map(const std::string &token)
    {
        std::unordered_map<std::string, double> out;
        if (token == "-")
            return out;
        for (const std::string &entry : split_fields(token, ';'))
        {
            const size_t eq = entry.find('=');
            out[entry.substr(0, eq)] = std::strtod(entry.c_str() + eq + 1, nullptr);
        }
        return out;
    }

    // Bounding box (left, top, width, height) of a polygon: Qt boundingRect of the Windows path.
    struct BBox
    {
        double x, y, w, h;
    };
    inline BBox bounding_rect(const Polygon &polygon)
    {
        double x0 = polygon.front()[0], x1 = x0, y0 = polygon.front()[1], y1 = y0;
        for (const auto &p : polygon)
        {
            x0 = std::min(x0, p[0]);
            x1 = std::max(x1, p[0]);
            y0 = std::min(y0, p[1]);
            y1 = std::max(y1, p[1]);
        }
        return {x0, y0, x1 - x0, y1 - y0};
    }
} // namespace geo_test
