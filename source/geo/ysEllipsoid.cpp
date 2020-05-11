#include "YoshiPBR/ysEllipsoid.h"
#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysRay.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysAABB ysEllipsoid::ComputeAABB() const
{
    // https://members.loria.fr/samuel.hornus/ellipsoid-bbox.html
    // The AABB is flush against the points on the ellipsoid with axis aligned normals. As the ellipsoid is convex, there is a 1-to-1
    // mapping between points and normals, so solve R*Sinv*x=[1,0,0] ... and similarly for [0,1,0] and [0,0,1].
    ysMtx44 RT = ysMtxFromQuat(ysConjugate(m_xf.q));
    ysVec4 aLS = ysNormalize3(RT.cx * m_s);
    ysVec4 bLS = ysNormalize3(RT.cy * m_s);
    ysVec4 cLS = ysNormalize3(RT.cz * m_s);
    ysVec4 a = ysMulT33(RT, aLS * m_s);
    ysVec4 b = ysMulT33(RT, bLS * m_s);
    ysVec4 c = ysMulT33(RT, cLS * m_s);
    ysVec4 span = ysVecSet(a.x, b.y, c.z);
    ysAssert(ysAllGT3(span, ysVec4_zero));
    ysAABB aabb;
    aabb.m_min = m_xf.p - span;
    aabb.m_max = m_xf.p + span;
    return aabb;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysEllipsoid::RayCast(ysRayCastOutput* output, const ysRayCastInput& input) const
{
    ysVec4 a = ysInvMul(m_xf, input.m_origin) * m_sInv;
    ysVec4 v = ysInvRotate(m_xf.q, input.m_direction) * m_sInv;
    // solve (a + v * s)^2 = 1
    ys_float32 aa = ysDot3(a, a);
    ys_float32 av = ysDot3(a, v);
    ys_float32 vv = ysDot3(v, v);
    ys_float32 dd = av * av - vv * (aa - 1.0f);
    if (dd < 0.0f)
    {
        return false;
    }
    ys_float32 d = sqrtf(dd);
    ys_float32 s = -(av + d) / vv;
    if (s < 0.0f)
    {
        return false;
    }
    ysVec4 nLS = a + v * ysSplat(s);

    ysVec4 n;
    ysVec4 t;
    {
        ysAssert(ysIsApproximatelyNormalized3(nLS));
        n = ysNormalize3(nLS * m_sInv);

        t = ysVecSet(-nLS.y, nLS.x, 0.0f) * m_s;
        if (ysLengthSqr3(t) < ys_zeroSafe)
        {
            t = ysVecSet(0.0f, -nLS.z, nLS.y) * m_s;
            ysAssert(ysLengthSqr3(t) >= ys_zeroSafe);
        }
        t = ysNormalize3(t);
    }

    output->m_hitPoint = ysMul(m_xf, nLS * m_s);
    output->m_hitNormal = ysRotate(m_xf.q, n);
    output->m_hitTangent = ysRotate(m_xf.q, t);
    output->m_lambda = s;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysEllipsoid::GenerateRandomSurfacePoint(ysSurfacePoint* point, ys_float32* probabilityDensity) const
{
    YS_REF(point);
    YS_REF(probabilityDensity);
    ysAssert(false); // TODO
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ys_float32 ysEllipsoid::ProbabilityDensityForGeneratedPoint(const ysVec4&) const
{
    ysAssert(false); // TODO
    return 0.0f;
}