#include "YoshiPBR/ysAABB.h"

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
ysAABB ysAABB::Merge(const ysAABB& a, const ysAABB& b)
{
    ysAssert(ysAllLE3(a.m_min, a.m_max) && ysAllLE3(b.m_min, b.m_max));
    ysAABB ab;
    ab.m_min = ysMin(a.m_min, b.m_min);
    ab.m_max = ysMax(a.m_max, b.m_max);
    return ab;
}