#pragma once

#include "YoshiPBR/ysAABB.h"
#include "YoshiPBR/ysStructures.h"

struct ysDrawInputBVH;
struct ysRayCastOutput;
struct ysSceneRayCastInput;
struct ysSceneRayCastOutput;
struct ysScene;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum struct ysRayCastFlowControlCode
{
    e_continue,
    e_clip,
    e_stop,
};

typedef ysRayCastFlowControlCode(*ysRayCastFlowControlFunction)(const ysSceneRayCastOutput&, void* userData);

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

    void Reset();
    void Create(const ysAABB* leafAABBs, const ysShapeId* leafShapeIds, ys_int32 leafCount);
    void Destroy();

    bool RayCastClosest(const ysScene* scene, ysSceneRayCastOutput*, const ysSceneRayCastInput&) const;
    void RayCast(const ysScene* scene, const ysSceneRayCastInput&, void* flowControlUserData, ysRayCastFlowControlFunction flowControlFcn) const;

    void DebugDraw(const ysDrawInputBVH&) const;

    Node* m_nodes; // Sorted so that parents preceed children
    ys_int32 m_nodeCount;

    ys_int32 m_depth;
};