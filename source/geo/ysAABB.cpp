#include "YoshiPBR/ysAABB.h"
#include "YoshiPBR/ysRay.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysAABB::SetInvalid()
{
    m_min = ysVec4_maxFloat;
    m_max = -ysVec4_maxFloat;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysAABB::Contains(const ysAABB& aabb) const
{
    ysAssert(ysAllLE3(m_min, m_max) && ysAllLE3(aabb.m_min, aabb.m_max));
    return ysAllLE3(m_min, aabb.m_min) && ysAllLE3(aabb.m_max, m_max);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysAABB::IntersectsRay(const ysRay& ray) const
{
    const ys_float32 tol = ys_epsilon;

    const ysVec4& o = ray.m_origin;
    const ysVec4& d = ray.m_direction;
    ysVec4 dInv = ysVec4_one / d;

    ys_float32 tMin = 0.0f;
    ys_float32 tMax = ys_maxFloat;

    // Clip ray against yz planes
    if (d.x > tol)
    {
        tMin = ysMax(tMin, (m_min.x - o.x) * dInv.x);
        tMax = ysMin(tMax, (m_max.x - o.x) * dInv.x);
    }
    else if (d.x < -tol)
    {
        tMin = ysMax(tMin, (m_max.x - o.x) * dInv.x);
        tMax = ysMin(tMax, (m_min.x - o.x) * dInv.x);
    }

    if (tMin > tMax)
    {
        return false;
    }

    // Clip ray against yx planes
    if (d.y > tol)
    {
        tMin = ysMax(tMin, (m_min.y - o.y) * dInv.y);
        tMax = ysMin(tMax, (m_max.y - o.y) * dInv.y);
    }
    else if (d.y < -tol)
    {
        tMin = ysMax(tMin, (m_max.y - o.y) * dInv.y);
        tMax = ysMin(tMax, (m_min.y - o.y) * dInv.y);
    }

    if (tMin > tMax)
    {
        return false;
    }

    // Clip ray against xy planes
    if (d.z > tol)
    {
        tMin = ysMax(tMin, (m_min.z - o.z) * dInv.z);
        tMax = ysMin(tMax, (m_max.z - o.z) * dInv.z);
    }
    else if (d.z < -tol)
    {
        tMin = ysMax(tMin, (m_max.z - o.z) * dInv.z);
        tMax = ysMin(tMax, (m_min.z - o.z) * dInv.z);
    }

    if (tMin > tMax)
    {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysAABB ysAABB::Merge(const ysAABB& a, const ysAABB& b)
{
    ysAssert(ysAllLE3(a.m_min, a.m_max) && ysAllLE3(b.m_min, b.m_max));
    ysAABB ab;
    ab.m_min = ysMin(a.m_min, b.m_min);
    ab.m_max = ysMax(a.m_max, b.m_max);
    return ab;
}