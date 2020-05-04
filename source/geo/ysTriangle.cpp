#include "YoshiPBR/ysTriangle.h"
#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysRay.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysAABB ysTriangle::ComputeAABB() const
{
    ysAABB aabb;
    aabb.m_min = ysMin(ysMin(m_v[0], m_v[1]), m_v[2]);
    aabb.m_max = ysMax(ysMax(m_v[0], m_v[1]), m_v[2]);
    return aabb;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysTriangle::RayCast(ysRayCastOutput* output, const ysRayCastInput& input) const
{
    // Moller and Trumbore (http://graphics.stanford.edu/courses/cs348b-17-spring-content/lectures/02_rt1/02_rt1_slides.pdf)
    // Solve
    //     O + tD = (1 - b1 - b2) * A + b1 * B + b2 * C
    // where (b0 = 1 - b1 - b2, b1, b2) are the barycentric coordinates of the intersection

    const ysVec4& o = input.m_origin;
    const ysVec4& d = input.m_direction;

    const ys_float32 dnTol = ys_epsilon;
    ys_float32 dn = ysDot3(d, m_n);
    if (ysAbs(dn) < dnTol)
    {
        return false;
    }

    ysVec4 n = m_n;
    if (dn > 0.0f)
    {
        n = -m_n;
        if (m_twoSided == false)
        {
            return false;
        }
    }

    ysVec4 e1 = m_v[1] - m_v[0];
    ysVec4 e2 = m_v[2] - m_v[0];
    ysVec4 s = o - m_v[0];
    ysVec4 s1 = ysCross(d, e2);
    ysVec4 s2 = ysCross(s, e1);

    ysVec4 tb1b2 = ysVecSet(ysDot3(s2, e2), ysDot3(s1, s), ysDot3(s2, d)) / ysSplatDot3(s1, e1);
    ys_float32 t = tb1b2.x;
    ys_float32 b1 = tb1b2.y;
    ys_float32 b2 = tb1b2.z;
    ys_float32 b0 = 1.0f - b1 - b2;

    if (t < 0.0f || input.m_maxLambda < t)
    {
        return false;
    }

    if (b0 < 0.0f || b0 > 1.0f ||
        b1 < 0.0f || b1 > 1.0f ||
        b2 < 0.0f || b2 > 1.0f)
    {
        return false;
    }

    ysVec4 p = ysSplat(b0) * m_v[0] + ysSplat(b1) * m_v[1] + ysSplat(b2) * m_v[2];

    output->m_hitPoint = p;
    output->m_hitNormal = n;
    output->m_hitTangent = m_t;
    output->m_lambda = t;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysTriangle::GenerateRandomSurfacePoint(ysSurfacePoint* point, ys_float32* probabilityDensity) const
{
    // https://mathworld.wolfram.com/TrianglePointPicking.html
    ysVec4 u = m_v[1] - m_v[0];
    ysVec4 v = m_v[2] - m_v[0];
    ys_float32 a = ysRandom(0.0f, 1.0f);
    ys_float32 b = ysRandom(0.0f, 1.0f);
    if (m_twoSided)
    {
        if (a + b > 1.0f)
        {
            a = 1.0f - a;
            b = 1.0f - b;
            point->m_normal = -m_n;
            point->m_tangent = m_t;
        }
        else
        {
            point->m_normal = m_n;
            point->m_tangent = m_t;
        }
        point->m_point = m_v[0] + ysSplat(a) * u + ysSplat(b) * v;
        *probabilityDensity = 1.0f / ysLength3(ysCross(u, v));
    }
    else
    {
        if (a + b > 1.0f)
        {
            a = 1.0f - a;
            b = 1.0f - b;
        }
        point->m_normal = m_n;
        point->m_tangent = m_t;
        point->m_point = m_v[0] + ysSplat(a) * u + ysSplat(b) * v;
        *probabilityDensity = 2.0f / ysLength3(ysCross(u, v));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysTriangle::GenerateRandomVisibleSurfacePoint(ysSurfacePoint* point, ys_float32* probabilityDensity, const ysVec4& vantagePoint) const
{
    // https://mathworld.wolfram.com/TrianglePointPicking.html
    ysVec4 u = m_v[1] - m_v[0];
    ysVec4 v = m_v[2] - m_v[0];
    ys_float32 a = ysRandom(0.0f, 1.0f);
    ys_float32 b = ysRandom(0.0f, 1.0f);
    if (a + b > 1.0f)
    {
        a = 1.0f - a;
        b = 1.0f - b;
    }
    point->m_point = m_v[0] + ysSplat(a) * u + ysSplat(b) * v;
    *probabilityDensity = 2.0f / ysLength3(ysCross(u, v));

    ysVec4 toVantage = vantagePoint - point->m_point;
    if (ysIsSafeToNormalize3(toVantage))
    {
        toVantage = ysNormalize3(toVantage);
    }
    else
    {
        return false;
    }

    const ys_float32 cosAngleThresh = sinf(ys_pi / 180.0f);
    ys_float32 dot = ysDot3(toVantage, m_n);

    if (m_twoSided)
    {
        if (dot < -cosAngleThresh)
        {
            point->m_normal = -m_n;
            point->m_tangent = m_t;
        }
        else if (dot > cosAngleThresh)
        {
            point->m_normal = m_n;
            point->m_tangent = m_t;
        }
        else
        {
            return false;
        }
    }
    else
    {
        if (dot < cosAngleThresh)
        {
            return false;
        }
        point->m_normal = m_n;
        point->m_tangent = m_t;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_float32 ysTriangle::ProbabilityDensityForGeneratedPoint(const ysVec4& point, const ysVec4& vantagePoint) const
{
    ysVec4 toVantage = vantagePoint - point;
    if (ysIsSafeToNormalize3(toVantage))
    {
        toVantage = ysNormalize3(toVantage);
    }
    else
    {
        return 0.0f;
    }

    const ys_float32 cosAngleThresh = sinf(ys_pi / 180.0f);
    ys_float32 dot = ysDot3(toVantage, m_n);

    if (m_twoSided)
    {
        if (-cosAngleThresh < dot && dot < cosAngleThresh)
        {
            return 0.0f;
        }
    }
    else
    {
        if (dot < cosAngleThresh)
        {
            return 0.0f;
        }
    }

    return 2.0f / ysLength3(ysCross(m_v[1] - m_v[0], m_v[2] - m_v[0]));
}