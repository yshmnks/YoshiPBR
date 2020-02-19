#pragma once

#include "YoshiPBR/ysAABB.h"
#include "YoshiPBR/ysShape.h"

struct ysDrawBVHInput;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysBVH
{
    struct Node
    {
        void Reset()
        {
            m_aabb.SetInvalid();
            m_parent = ys_nullIndex;
            m_left = ys_nullIndex;
            m_right = ys_nullIndex;
        }

        ysAABB m_aabb;
        ysShapeId m_shapeId;
        ys_int32 m_parent;
        ys_int32 m_left;
        ys_int32 m_right;
    };

    ysBVH()
    {
        Reset();
    }

    void Reset()
    {
        m_nodes = nullptr;
        m_nodeCount = 0;
    }

    void Build(const ysAABB* leafAABBs, const ysShapeId* leafShapeIds, ys_int32 leafCount);

    void DebugDraw(const ysDrawBVHInput*) const;

    Node* m_nodes; // Sorted so that parents proceed children
    ys_int32 m_nodeCount;
};