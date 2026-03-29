#pragma once

namespace ost_domain
{
    enum class ConditionType : int
    {
        Linear = 0,
        Area = 1,
        Count = 2,
        Attachment = 3
    };
    enum class ShapeId : int
    {
        Square = 0,
        Circle = 1,
        Triangle = 2,
        Rectangle = 3,
        Rhombus = 12,
        Ellipse = 13,
        Hexagon = 14,
        Octagon = 15
    };
    enum class Pattern : int
    {
        None = 0,
        Solid = 1,
        Horizontal = 2,
        Vertical = 3,
        BackwardDiag = 4,
        ForwardDiag = 5,
        Crosshatch = 6,
        DiagCrosshatch = 7,
        Transparent = 8
    };

}
