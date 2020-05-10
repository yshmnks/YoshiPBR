#include "ysMaterial.h"
#include "ysMaterialMirror.h"
#include "ysMaterialStandard.h"
#include "scene/ysScene.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysBSDF ysMaterial::EvaluateBRDF(const ysScene* scene, const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    ysBSDF f;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            f = subMat.EvaluateBRDF(incomingDirectionLS, outgoingDirectionLS);
            break;
        }
        case Type::e_mirror:
        {
            const ysMaterialMirror& subMat = scene->m_materialMirrors[m_typeIndex];
            f = subMat.EvaluateBRDF(incomingDirectionLS, outgoingDirectionLS);
            break;
        }
        default:
        {
            ysAssert(false);
            f.SetInvalid();
            break;
        }
    }
    ysAssert(f.IsValid());
    return f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomDirection(const ysScene* scene,
    const ysVec4& incomingDirectionLS, ysVec4* outgoingDirectionLS, ysBSDF* f) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, f);
            break;
        }
        case Type::e_mirror:
        {
            const ysMaterialMirror& subMat = scene->m_materialMirrors[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, f);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            f->SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid() && f->IsValid());
    ysAssert(p.m_perProjectedSolidAngle.m_isFinite == f->m_isFinite);
    ysAssert(ysIsApproximatelyNormalized3(*outgoingDirectionLS));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomDirection(const ysScene* scene,
    ysVec4* incomingDirectionLS, const ysVec4& outgoingDirectionLS, ysBSDF* f) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, f);
            break;
        }
        case Type::e_mirror:
        {
            const ysMaterialMirror& subMat = scene->m_materialMirrors[m_typeIndex];
            p = subMat.GenerateRandomDirection(incomingDirectionLS, outgoingDirectionLS, f);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            f->SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid() && f->IsValid());
    ysAssert(p.m_perProjectedSolidAngle.m_isFinite == f->m_isFinite);
    ysAssert(ysIsApproximatelyNormalized3(*incomingDirectionLS));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomDirection(const ysScene* scene,
    const ysVec4& w_i, ysVec4* w_o, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(w_i, w_o, f, pReverse);
            break;
        }
        case Type::e_mirror:
        {
            const ysMaterialMirror& subMat = scene->m_materialMirrors[m_typeIndex];
            p = subMat.GenerateRandomDirection(w_i, w_o, f, pReverse);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            f->SetInvalid();
            pReverse->SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid() && f->IsValid() && pReverse->IsValid());
    ysAssert(p.m_perProjectedSolidAngle.m_isFinite == f->m_isFinite);
    ysAssert(ysIsApproximatelyNormalized3(*w_o));
    return p;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::GenerateRandomDirection(const ysScene* scene,
    ysVec4* w_i, const ysVec4& w_o, ysBSDF* f, ysDirectionalProbabilityDensity* pReverse) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.GenerateRandomDirection(w_i, w_o, f, pReverse);
            break;
        }
        case Type::e_mirror:
        {
            const ysMaterialMirror& subMat = scene->m_materialMirrors[m_typeIndex];
            p = subMat.GenerateRandomDirection(w_i, w_o, f, pReverse);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            f->SetInvalid();
            pReverse->SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid() && f->IsValid() && pReverse->IsValid());
    ysAssert(p.m_perProjectedSolidAngle.m_isFinite == f->m_isFinite);
    ysAssert(ysIsApproximatelyNormalized3(*w_i));
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::ProbabilityDensityForGeneratedIncomingDirection(const ysScene* scene,
    const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedIncomingDirection(incomingDirectionLS, outgoingDirectionLS);
            break;
        }
        case Type::e_mirror:
        {
            const ysMaterialMirror& subMat = scene->m_materialMirrors[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedIncomingDirection(incomingDirectionLS, outgoingDirectionLS);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid());
    return p;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ysDirectionalProbabilityDensity ysMaterial::ProbabilityDensityForGeneratedOutgoingDirection(const ysScene* scene,
    const ysVec4& incomingDirectionLS, const ysVec4& outgoingDirectionLS) const
{
    ysDirectionalProbabilityDensity p;
    switch (m_type)
    {
        case Type::e_standard:
        {
            const ysMaterialStandard& subMat = scene->m_materialStandards[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedOutgoingDirection(incomingDirectionLS, outgoingDirectionLS);
            break;
        }
        case Type::e_mirror:
        {
            const ysMaterialMirror& subMat = scene->m_materialMirrors[m_typeIndex];
            p = subMat.ProbabilityDensityForGeneratedOutgoingDirection(incomingDirectionLS, outgoingDirectionLS);
            break;
        }
        default:
        {
            ysAssert(false);
            p.SetInvalid();
            break;
        }
    }
    ysAssert(p.IsValid());
    return p;
}