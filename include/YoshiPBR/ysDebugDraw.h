#pragma once

#include "YoshiPBR/ysMath.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Color
{
    Color()
    {
        r = 1.0f;
        g = 1.0f;
        b = 1.0f;
        a = 1.0f;
    }

    Color(ys_float32 rIn, ys_float32 gIn, ys_float32 bIn, ys_float32 aIn = 1.0f)
    {
        r = rIn;
        g = gIn;
        b = bIn;
        a = aIn;
    }

    ys_float32 r, g, b, a;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysDebugDraw
{
    // color is per primitive (not per vertex) i.e. for triangles you should pass 3x as many vertices as colors
    virtual void DrawSegmentList(const ysVec4* vertices, const Color* colors, ys_int32 segmentCount);
    virtual void DrawTriangleList(const ysVec4* vertices, const Color* colors, ys_int32 triangleCount);

    virtual void DrawWireBox(const ysVec4& halfDimensions, const ysTransform&, const Color&);
    virtual void DrawWireEllipsoid(const ysVec4& halfDimensions, const ysTransform&, const Color&);

    virtual void DrawBox(const ysVec4& halfDimensions, const ysTransform&, const Color&);
    virtual void DrawEllipsoid(const ysVec4& halfDimensions, const ysTransform&, const Color&);
};