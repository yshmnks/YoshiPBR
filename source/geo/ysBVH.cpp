#include "YoshiPBR/ysBVH.h"
#include "YoshiPBR/ysDebugDraw.h"
#include "YoshiPBR/ysRay.h"
#include "YoshiPBR/ysShape.h"
#include "YoshiPBR/ysStructures.h"
#include "scene/ysScene.h"

#include <algorithm>

// Construct a 61 bit integer by inserting two zeroes between each bit of src. ( ...1 0 1 1 becomes ...001 000 001 001)
static ys_uint64 sSparsifyUint21(const ys_uint64& src21)
{
    // https://stackoverflow.com/a/1024889
    ysAssert((src21 >> 21) == 0);
    ys_uint64 dst = src21;
    // Reserve 32 zero bits (hence the shift by 32) to be interleaved into the lowest 16 bits
    dst = (dst | (dst << 32)) & 0x001F00000000FFFF;
    // Reserve 16 zero bits to be interleaved into the every cluster of 8 bits
    dst = (dst | (dst << 16)) & 0x001F0000FF0000FF;
    // Reserve 8 zero bits to be interleaved into the every cluster of 4 bits
    dst = (dst | (dst << 8)) & 0x100F00F00F00F00F;
    // Reserve 4 zero bits to be interleaved into the every cluster of 2 bits
    dst = (dst | (dst << 4)) & 0x10C30C30C30C30C3;
    // Reserve 2 zero bits to be interleaved into the every cluster of 1 bit
    dst = (dst | (dst << 2)) & 0x1249249249249249;
    return dst;
}

//
static ys_uint64 sInterleaveUint21s(const ys_uint64& a, const ys_uint64& b, const ys_uint64& c)
{
    ys_uint64 aSparse = sSparsifyUint21(a);
    ys_uint64 bSparse = sSparsifyUint21(b);
    ys_uint64 cSparse = sSparsifyUint21(c);
    return (aSparse << 2) | (bSparse << 1) | (cSparse << 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysBVHBuilder
{
    // Based on http://graphics.cs.cmu.edu/projects/aac/aac_build.pdf
    // See also https://luebke.us/publications/eg09.pdf

    //
    struct Cluster
    {
        ysAABB m_aabb;
        ys_uint64 m_zOrder;
        ys_int32 m_srcIndex;
        ys_int32 m_parent;
        ys_int32 m_left;
        ys_int32 m_right;

        // Linked list of clusters
        ys_int32 m_prev;
        ys_int32 m_next;

        ys_float32 m_bestCost;
        union
        {
            ys_int32 m_bestMatch;
            ys_int32 m_depth;
        };

        union
        {
            ys_int32 m_primCount;
            ys_int32 m_remap;
        };
    };

    //
    struct ClusterList
    {
        ys_int32 m_first;
        ys_int32 m_last;
        ys_int32 m_count;
    };

    //
    static bool sCompareLeafClusters(const Cluster& a, const Cluster& b)
    {
        if (a.m_zOrder < b.m_zOrder)
        {
            return true;
        }

        if (a.m_zOrder > b.m_zOrder)
        {
            return false;
        }

        ysAssert(a.m_srcIndex != b.m_srcIndex);
        return (a.m_srcIndex < b.m_srcIndex);
    }

    //
    ys_int32 f(ys_int32 leafCount)
    {
        // f = c * leafCount^alpha where we choose c = sqrt(delta)/2 and alpha = 1/2
        ys_float32 a = sqrtf(ys_float32(leafCount * m_delta)) * 0.5f;
        return ysMax(1, (ys_int32)a);
    }

    //
    void Build(ysBVH* output, const ysAABB* leafAABBs, const ysShapeId* leafShapeIds, ys_int32 leafCount, ys_int32 delta)
    {
        ysAssert(delta >= 2 && leafCount >= 0);
        if (leafCount == 0)
        {
            output->m_nodes = nullptr;
            output->m_nodeCount = 0;
            output->m_depth = 0;
            return;
        }
        m_delta = delta;
        m_leafCount = leafCount;
        m_clusterCapacity = 2 * leafCount - 1;
        m_nodeCount = leafCount;

        // Single allocation for clusters and finalization stack
        ys_int32 clustersByteCount = sizeof(Cluster) * m_clusterCapacity;
        ys_int32 stackByteCount = sizeof(ys_int32) * m_clusterCapacity;
        ys_int32 memBufferByteCount = clustersByteCount + stackByteCount;
        void* memBuffer = ysMalloc(memBufferByteCount);
        m_clusters = static_cast<Cluster*>(memBuffer);
        ys_int32* clusterIdxStack = (ys_int32*)(static_cast<ys_int8*>(memBuffer) + clustersByteCount);

        ysAABB centersAABB;
        centersAABB.SetInvalid();
        for (ys_int32 i = 0; i < leafCount; ++i)
        {
            ysVec4 center = (leafAABBs[i].m_min + leafAABBs[i].m_max) * ysVec4_half;
            centersAABB.m_min = ysMin(centersAABB.m_min, center);
            centersAABB.m_max = ysMax(centersAABB.m_max, center);
        }

        // Compute zOrder using the cube with the centers-AABB squashed into the lowest zOrder corner. Under the assumption that leaf shapes
        // have reasonably uniform aspect ratio and are distrbuted roughly uniformly throughout the bounds, this will bias partitioning
        // across the longest AABB axis at each depth.
        ysVec4 invCentersCubeSpan;
        {
            ysVec4 centersSpan = centersAABB.m_max - centersAABB.m_min;
            ys_float32 centersSpanMax = ysMax(ysMax(centersSpan.x, centersSpan.y), centersSpan.z);
            invCentersCubeSpan = (centersSpanMax < ys_epsilon) ? ysVec4_zero : ysSplat(1.0f / centersSpanMax);
        }

        // Convert a float in range [0.0f, 1.0f] to a 21 bit integer in range [0, (1<<21)-1];
        const ysVec4 f2i = ysSplat(float((1 << 21) - 1));

        for (ys_int32 i = 0; i < leafCount; ++i)
        {
            ysVec4 center = (leafAABBs[i].m_min + leafAABBs[i].m_max) * ysVec4_half;
            ysVec4 centerNorm = (center - centersAABB.m_min) * invCentersCubeSpan;
            centerNorm = ysClamp(centerNorm, ysVec4_zero, ysVec4_one);
            ysVec4 centerGrid = centerNorm * f2i;
            ys_uint64 centerGridX = (ys_uint64)centerGrid.x;
            ys_uint64 centerGridY = (ys_uint64)centerGrid.y;
            ys_uint64 centerGridZ = (ys_uint64)centerGrid.z;

            Cluster* cluster = m_clusters + i;
            cluster->m_aabb = leafAABBs[i];
            cluster->m_zOrder = sInterleaveUint21s(centerGridX, centerGridY, centerGridZ);
            cluster->m_srcIndex = i;
            cluster->m_parent = ys_nullIndex;
            cluster->m_left = ys_nullIndex;
            cluster->m_right = ys_nullIndex;
            cluster->m_primCount = 1;
            cluster->m_prev = ys_nullIndex;
            cluster->m_next = ys_nullIndex;
            cluster->m_bestCost = ys_maxFloat;
            cluster->m_bestMatch = ys_nullIndex;
        }
        std::sort(m_clusters, m_clusters + leafCount, sCompareLeafClusters);

        for (ys_int32 i = leafCount; i < m_clusterCapacity; ++i)
        {
            Cluster* cluster = m_clusters + i;
            cluster->m_aabb.SetInvalid();
            cluster->m_zOrder = 0;
            cluster->m_srcIndex = ys_nullIndex;
            cluster->m_parent = ys_nullIndex;
            cluster->m_left = ys_nullIndex;
            cluster->m_right = ys_nullIndex;
            cluster->m_primCount = 0;
            cluster->m_prev = ys_nullIndex;
            cluster->m_next = ys_nullIndex;
            cluster->m_bestCost = ys_maxFloat;
            cluster->m_bestMatch = ys_nullIndex;
        }

        ys_int32 leafBegin = 0;
        ys_int32 leafEnd = leafCount;
        ys_int32 zOrderBitPosition = 62;
        ClusterList halfBakedClusterList = BuildTree(leafBegin, leafEnd, zOrderBitPosition);
        ClusterList clusterList = CombineClusters(halfBakedClusterList, 1);
        ysAssert(clusterList.m_count == 1 && clusterList.m_first == clusterList.m_last);
        ysAssert(m_clusters[clusterList.m_first].m_parent == ys_nullIndex);
        for (ys_int32 i = 0; i < m_clusterCapacity; ++i)
        {
            // Not really necessary, but initializing this to null can be used to check for cycles.
            m_clusters[i].m_remap = ys_nullIndex;
        }
        output->m_depth = 0;

        ys_int32 clusterIdxStackCount = 1;
        clusterIdxStack[0] = clusterList.m_first;
        m_clusters[clusterList.m_first].m_depth = 0;
        ys_int32 nodeIdx = 0;
        while (clusterIdxStackCount > 0)
        {
            ys_int32 clusterIdx = clusterIdxStack[--clusterIdxStackCount];
            ysAssert(0 <= clusterIdx && clusterIdx < m_clusterCapacity);
            Cluster* cluster = m_clusters + clusterIdx;
            ysAssert(cluster->m_remap == ys_nullIndex);
            cluster->m_remap = nodeIdx++;
            ysAssert((cluster->m_left == ys_nullIndex) == (cluster->m_right == ys_nullIndex));
            if (cluster->m_left != ys_nullIndex)
            {
                ysAssert(cluster->m_left != cluster->m_right);
                ysAssert(clusterIdxStackCount + 1 < m_clusterCapacity)
                clusterIdxStack[clusterIdxStackCount++] = cluster->m_left;
                clusterIdxStack[clusterIdxStackCount++] = cluster->m_right;
                m_clusters[cluster->m_left].m_depth = cluster->m_depth + 1;
                m_clusters[cluster->m_right].m_depth = cluster->m_depth + 1;
                output->m_depth = ysMax(output->m_depth, cluster->m_depth + 1);
            }
        }
        output->m_depth += 1;

        ysAssert(m_nodeCount == m_nodeCapacity);
        output->m_nodeCount = m_nodeCount;
        output->m_nodes = static_cast<ysBVH::Node*>(ysMalloc(sizeof(ysBVH::Node) * m_nodeCount));
        for (ys_int32 i = 0; i < m_nodeCount; ++i)
        {
            const Cluster* src = m_clusters + i;
            ysBVH::Node* dst = output->m_nodes + src->m_remap;
            dst->m_aabb = src->m_aabb;
            dst->m_shapeId = (src->m_srcIndex == ys_nullIndex) ? ys_nullShapeId : leafShapeIds[src->m_srcIndex];
            dst->m_parent = (src->m_parent == ys_nullIndex) ? ys_nullIndex : m_clusters[src->m_parent].m_remap;
            dst->m_left = (src->m_left == ys_nullIndex) ? ys_nullIndex : m_clusters[src->m_left].m_remap;
            dst->m_right = (src->m_right == ys_nullIndex) ? ys_nullIndex : m_clusters[src->m_right].m_remap;
            ysAssert((dst->m_left == ys_nullIndex) == (dst->m_right == ys_nullIndex));
        }

        ysFree(memBuffer);
    }

    //
    ys_int32 MakePartition(ys_int32 beginIdx, ys_int32 endIdx, ys_int32 zOrderBitPosition)
    {
        ysAssert(endIdx - beginIdx >= m_delta);
        ys_int32 i = beginIdx;
        ys_int32 k = endIdx;
        do
        {
            ys_int32 j = (i + k) / 2;
            ysAssert(i < j && j < k);
            bool bitL = (m_clusters[j - 1].m_zOrder >> zOrderBitPosition) & 1;
            bool bitR = (m_clusters[j - 0].m_zOrder >> zOrderBitPosition) & 1;
            ysAssert(bitR == true || bitL == false);
            if (bitL != bitR)
            {
                return j;
            }

            if (bitL)
            {
                k = j;
            }
            else
            {
                i = j;
            }

        } while (i + 1 < k);
        ysAssert(i + 1 == k);

        return ys_nullIndex;
    }

    //
    ClusterList BuildTree(ys_int32 leafBegin, ys_int32 leafEnd, ys_int32 inBitPosition)
    {
        ys_int32 subLeafCount = leafEnd - leafBegin;
        ysAssert(subLeafCount > 0);
        if (subLeafCount < m_delta)
        {
            ysAssert(m_clusters[leafBegin].m_prev == ys_nullIndex && m_clusters[leafBegin].m_next == ys_nullIndex);
            for (ys_int32 i = leafBegin + 1; i < leafEnd; ++i)
            {
                ysAssert(m_clusters[i].m_prev == ys_nullIndex && m_clusters[i].m_next == ys_nullIndex);
                m_clusters[i - 0].m_prev = i - 1;
                m_clusters[i - 1].m_next = i - 0;
            }
            ClusterList clusterList;
            clusterList.m_first = leafBegin;
            clusterList.m_last = leafEnd - 1;
            clusterList.m_count = subLeafCount;
            return CombineClusters(clusterList, f(m_delta));
        }

        ys_int32 midIdx = ys_nullIndex;
        ys_int32 bitPosition = inBitPosition;
        while (midIdx == ys_nullIndex && bitPosition >= 0)
        {
            midIdx = MakePartition(leafBegin, leafEnd, bitPosition--);
        }

        if (midIdx != ys_nullIndex)
        {
            ClusterList clustersL = BuildTree(leafBegin, midIdx, bitPosition);
            ClusterList clustersR = BuildTree(midIdx, leafEnd, bitPosition);
            ysAssert(clustersL.m_count > 0 && clustersL.m_first != ys_nullIndex && clustersL.m_last != ys_nullIndex);
            ysAssert(clustersR.m_count > 0 && clustersR.m_first != ys_nullIndex && clustersR.m_last != ys_nullIndex);
            ysAssert(m_clusters[clustersL.m_last].m_next == ys_nullIndex);
            ysAssert(m_clusters[clustersR.m_first].m_prev == ys_nullIndex);
            m_clusters[clustersL.m_last].m_next = clustersR.m_first;
            m_clusters[clustersR.m_first].m_prev = clustersL.m_last;
            ClusterList clustersLUR;
            clustersLUR.m_first = clustersL.m_first;
            clustersLUR.m_last = clustersR.m_last;
            clustersLUR.m_count = clustersL.m_count + clustersR.m_count;
            return CombineClusters(clustersLUR, f(subLeafCount));
        }
        else
        {
            for (ys_int32 i = leafBegin + 1; i < leafEnd; ++i)
            {
                ysAssert(m_clusters[i].m_prev == ys_nullIndex && m_clusters[i].m_next == ys_nullIndex);
                m_clusters[i - 0].m_prev = i - 1;
                m_clusters[i - 1].m_next = i - 0;
            }
            ClusterList clusterList;
            clusterList.m_first = leafBegin;
            clusterList.m_last = leafEnd - 1;
            clusterList.m_count = subLeafCount;
            return CombineClusters(clusterList, f(m_delta));
        }
    }

    //
    void FindBestMatch(ys_int32 clusterIdxA, ClusterList clusterList)
    {
        ysAssert(0 <= clusterIdxA && clusterIdxA < m_clusterCapacity);
        Cluster* clusterA = m_clusters + clusterIdxA;
        ysAssert(ysAllLE3(clusterA->m_aabb.m_min, clusterA->m_aabb.m_max));
        clusterA->m_bestMatch = ys_nullIndex;
        clusterA->m_bestCost = ys_maxFloat;
        ys_int32 clusterIdxB = clusterList.m_first;
        for (ys_int32 i = 0; i < clusterList.m_count; ++i)
        {
            ysAssert(0 <= clusterIdxB && clusterIdxB < m_clusterCapacity);
            if (clusterIdxB == clusterIdxA)
            {
                clusterIdxB = m_clusters[clusterIdxB].m_next;
                continue;
            }
            Cluster* clusterB = m_clusters + clusterIdxB;
            ysAssert(ysAllLE3(clusterB->m_aabb.m_min, clusterB->m_aabb.m_max));
            ysAABB mergedAABB = ysAABB::Merge(clusterA->m_aabb, clusterB->m_aabb);
            ysVec4 mergedAABBSpan = mergedAABB.m_max - mergedAABB.m_min;
            ys_float32 mergedAABBArea
                = mergedAABBSpan.x * mergedAABBSpan.y
                + mergedAABBSpan.y * mergedAABBSpan.z
                + mergedAABBSpan.z * mergedAABBSpan.x;
            ys_float32 cost = mergedAABBArea;
            if (cost < clusterA->m_bestCost)
            {
                clusterA->m_bestCost = cost;
                clusterA->m_bestMatch = clusterIdxB;
            }
            clusterIdxB = clusterB->m_next;
        }
        ysAssert(clusterIdxB == ys_nullIndex);
    }

    //
    ClusterList CombineClusters(ClusterList clusterList, ys_int32 agglomeratedClusterCount)
    {
        ysAssert(agglomeratedClusterCount >= 1);
        ClusterList agglomeratedList = clusterList;

        ys_int32 clusterIdx = clusterList.m_first;
        for (ys_int32 i = 0; i < clusterList.m_count; ++i)
        {
            FindBestMatch(clusterIdx, clusterList);
            clusterIdx = m_clusters[clusterIdx].m_next;
        }
        ysAssert(clusterIdx == ys_nullIndex);

        while (agglomeratedList.m_count > agglomeratedClusterCount)
        {
            ys_float32 bestCost = ys_maxFloat;
            ys_int32 idxL = ys_nullIndex;
            ys_int32 idxR = ys_nullIndex;
            clusterIdx = agglomeratedList.m_first;
            for (ys_int32 i = 0; i < agglomeratedList.m_count; ++i)
            {
                ysAssert(0 <= clusterIdx && clusterIdx < m_clusterCapacity);
                Cluster* cluster = m_clusters + clusterIdx;
                if (cluster->m_bestCost < bestCost)
                {
                    bestCost = cluster->m_bestCost;
                    idxL = clusterIdx;
                    idxR = cluster->m_bestMatch;
                }
                clusterIdx = cluster->m_next;
            }
            ysAssert(clusterIdx == ys_nullIndex);
            ysAssert(idxL != ys_nullIndex && idxR != ys_nullIndex);
            ysAssert(idxL != idxR);
            Cluster* nodeL = m_clusters + idxL;
            Cluster* nodeR = m_clusters + idxR;
            ysAssert(nodeL->m_parent == ys_nullIndex && nodeR->m_parent == ys_nullIndex);
            ysAssert(nodeL->m_primCount > 0 && nodeR->m_primCount > 0);
            ysAssert(m_nodeCount < m_nodeCapacity);
            ys_int32 idxLUR = m_nodeCount++;
            Cluster* nodeLUR = m_clusters + idxLUR;
            nodeLUR->m_left = idxL;
            nodeLUR->m_right = idxR;
            nodeLUR->m_parent = ys_nullIndex;
            nodeL->m_parent = idxLUR;
            nodeR->m_parent = idxLUR;
            nodeLUR->m_aabb = ysAABB::Merge(nodeL->m_aabb, nodeR->m_aabb);
            nodeLUR->m_primCount = nodeL->m_primCount + nodeR->m_primCount;

            // Remove L from the cluster list
            {
                if (nodeL->m_prev != ys_nullIndex)
                {
                    m_clusters[nodeL->m_prev].m_next = nodeL->m_next;
                }

                if (nodeL->m_next != ys_nullIndex)
                {
                    m_clusters[nodeL->m_next].m_prev = nodeL->m_prev;
                }

                if (idxL == agglomeratedList.m_first)
                {
                    agglomeratedList.m_first = nodeL->m_next;
                }

                if (idxL == agglomeratedList.m_last)
                {
                    agglomeratedList.m_last = nodeL->m_prev;
                }

                nodeL->m_prev = ys_nullIndex;
                nodeL->m_next = ys_nullIndex;
                agglomeratedList.m_count--;
                ysAssert(agglomeratedList.m_count >= 0);
            }

            // Remove R from the cluster list
            {
                if (nodeR->m_prev != ys_nullIndex)
                {
                    m_clusters[nodeR->m_prev].m_next = nodeR->m_next;
                }

                if (nodeR->m_next != ys_nullIndex)
                {
                    m_clusters[nodeR->m_next].m_prev = nodeR->m_prev;
                }

                if (idxR == agglomeratedList.m_first)
                {
                    agglomeratedList.m_first = nodeR->m_next;
                }

                if (idxR == agglomeratedList.m_last)
                {
                    agglomeratedList.m_last = nodeR->m_prev;
                }

                nodeR->m_prev = ys_nullIndex;
                nodeR->m_next = ys_nullIndex;
                agglomeratedList.m_count--;
                ysAssert(agglomeratedList.m_count >= 0);
            }

            // Add LUR to the cluster list
            {
                nodeLUR->m_prev = agglomeratedList.m_last;
                if (agglomeratedList.m_last != ys_nullIndex)
                {
                    m_clusters[agglomeratedList.m_last].m_next = idxLUR;
                    agglomeratedList.m_last = idxLUR;
                }
                else
                {
                    ysAssert(agglomeratedList.m_first == ys_nullIndex);
                    agglomeratedList.m_first = idxLUR;
                    agglomeratedList.m_last = idxLUR;
                }
                agglomeratedList.m_count++;
            }

            // Find the best match for LUR
            // Update the best match for any clusters whose best match was either L or R
            {
                FindBestMatch(idxLUR, agglomeratedList);

                clusterIdx = agglomeratedList.m_first;
                for (ys_int32 i = 0; i < agglomeratedList.m_count; ++i)
                {
                    ysAssert(0 <= clusterIdx && clusterIdx < m_clusterCapacity);
                    Cluster* cluster = m_clusters + clusterIdx;
                    if (cluster->m_bestMatch == idxL || cluster->m_bestMatch == idxR)
                    {
                        FindBestMatch(clusterIdx, agglomeratedList);
                    }
                    clusterIdx = cluster->m_next;
                }
                ysAssert(clusterIdx == ys_nullIndex);
            }
        }
        ysAssert(agglomeratedList.m_count <= agglomeratedClusterCount);
        return agglomeratedList;
    }

    Cluster* m_clusters;
    ys_int32 m_leafCount;
    ys_int32 m_nodeCount;
    union
    {
        ys_int32 m_clusterCapacity;
        ys_int32 m_nodeCapacity;
    };
    ys_int32 m_delta;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysBVH::Reset()
{
    m_nodes = nullptr;
    m_nodeCount = 0;
    m_depth = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysBVH::Create(const ysAABB* leafAABBs, const ysShapeId* leafShapeIds, ys_int32 leafCount)
{
    ysBVHBuilder builder;
    ys_int32 delta = 8;
    builder.Build(this, leafAABBs, leafShapeIds, leafCount, delta);

    // Validation
    {
        ysAssert((leafCount == 0) == (m_nodeCount == 0));
        ysAssert(m_nodeCount == 0 || m_nodeCount == 2 * leafCount - 1);
        if (m_nodeCount == 0)
        {
            return;
        }
        ysAssert(m_nodes[0].m_parent == ys_nullIndex);
        ysAssert(m_nodes[0].m_left == ys_nullIndex || (0 < m_nodes[0].m_left && m_nodes[0].m_left < m_nodeCount));
        ysAssert(m_nodes[0].m_right == ys_nullIndex || (0 < m_nodes[0].m_right && m_nodes[0].m_right < m_nodeCount));
        ysAssert((m_nodes[0].m_left == ys_nullIndex) == (m_nodes[0].m_right == ys_nullIndex));
        ysAssert(m_nodes[0].m_left == ys_nullIndex || m_nodes[0].m_left != m_nodes[0].m_right);
        ysAssert((m_nodes[0].m_left == ys_nullIndex) == (m_nodes[0].m_shapeId != ys_nullShapeId));
        for (ys_int32 i = 1; i < m_nodeCount; ++i)
        {
            const Node* node = m_nodes + i;
            ysAssert(node->m_left == ys_nullIndex || (i < node->m_left && node->m_left < m_nodeCount));
            ysAssert(node->m_right == ys_nullIndex || (i < node->m_right && node->m_right < m_nodeCount));
            ysAssert((node->m_left == ys_nullIndex) == (node->m_right == ys_nullIndex));
            ysAssert(node->m_left == ys_nullIndex || node->m_left != node->m_right);
            ysAssert((node->m_left == ys_nullIndex) == (node->m_shapeId != ys_nullShapeId));
            ysAssert(0 <= node->m_parent && node->m_parent < i);
            const Node* parent = m_nodes + node->m_parent;
            YS_REF(parent);
            ysAssert(parent->m_aabb.Contains(node->m_aabb));
            ysAssert(parent->m_left == i || parent->m_right == i);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysBVH::Destroy()
{
    ysFree(m_nodes);
    m_nodes = nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool ysBVH::RayCastClosest(const ysScene* scene, ysSceneRayCastOutput* output, const ysSceneRayCastInput& input) const
{
    bool anyHit = false;
    ysRayCastInput rci;
    rci.m_origin = input.m_origin;
    rci.m_direction = input.m_direction;
    rci.m_maxLambda = input.m_maxLambda;

    const ys_int32 k_stackSize = 256;
    ysAssert(m_depth < k_stackSize);
    ys_int32 nodeIndexStack[k_stackSize];
    nodeIndexStack[0] = 0;
    ys_int32 stackCount = 1;
    while (stackCount > 0)
    {
        stackCount--;
        const Node* node = m_nodes + nodeIndexStack[stackCount];
        ysRay ray;
        ray.m_origin = rci.m_origin;
        ray.m_direction = rci.m_direction;
        bool rayIntersectsNode = node->m_aabb.IntersectsRay(ray, rci.m_maxLambda);
        if (rayIntersectsNode == false)
        {
            continue;
        }

        ysAssert((node->m_left == ys_nullIndex) == (node->m_right == ys_nullIndex));
        if (node->m_left != ys_nullIndex)
        {
            ysAssert(node->m_shapeId == ys_nullShapeId);
            nodeIndexStack[stackCount] = node->m_left;
            stackCount++;
            nodeIndexStack[stackCount] = node->m_right;
            stackCount++;
        }
        else
        {
            ysAssert(node->m_shapeId != ys_nullShapeId);
            const ysShape& shape = scene->m_shapes[node->m_shapeId.m_index];
            ysRayCastOutput rco;
            bool hit = shape.RayCast(scene, &rco, rci);
            if (hit)
            {
                rci.m_maxLambda = rco.m_lambda;
                output->m_hitPoint = rco.m_hitPoint;
                output->m_hitNormal = rco.m_hitNormal;
                output->m_hitTangent = rco.m_hitTangent;
                output->m_lambda = rco.m_lambda;
                output->m_shapeId = node->m_shapeId;
                anyHit = true;
            }
        }
    }
    return anyHit;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysBVH::RayCast(const ysScene* scene, const ysSceneRayCastInput& input, void* dat, ysRayCastFlowControlFunction fcn) const
{
    ysRayCastInput rci;
    rci.m_origin = input.m_origin;
    rci.m_direction = input.m_direction;
    rci.m_maxLambda = input.m_maxLambda;

    const ys_int32 k_stackSize = 256;
    ysAssert(m_depth < k_stackSize);
    ys_int32 nodeIndexStack[k_stackSize];
    nodeIndexStack[0] = 0;
    ys_int32 stackCount = 1;
    while (stackCount > 0)
    {
        stackCount--;
        const Node* node = m_nodes + nodeIndexStack[stackCount];
        ysRay ray;
        ray.m_origin = rci.m_origin;
        ray.m_direction = rci.m_direction;
        bool rayIntersectsNode = node->m_aabb.IntersectsRay(ray, rci.m_maxLambda);
        if (rayIntersectsNode == false)
        {
            continue;
        }

        ysAssert((node->m_left == ys_nullIndex) == (node->m_right == ys_nullIndex));
        if (node->m_left != ys_nullIndex)
        {
            ysAssert(node->m_shapeId == ys_nullShapeId);
            nodeIndexStack[stackCount] = node->m_left;
            stackCount++;
            nodeIndexStack[stackCount] = node->m_right;
            stackCount++;
        }
        else
        {
            ysAssert(node->m_shapeId != ys_nullShapeId);
            const ysShape& shape = scene->m_shapes[node->m_shapeId.m_index];
            ysRayCastOutput rco;
            bool hit = shape.RayCast(scene, &rco, rci);
            if (hit)
            {
                ysSceneRayCastOutput srco;
                srco.m_hitPoint = rco.m_hitPoint;
                srco.m_hitNormal = rco.m_hitNormal;
                srco.m_hitTangent = rco.m_hitTangent;
                srco.m_lambda = rco.m_lambda;
                srco.m_shapeId = node->m_shapeId;
                ysRayCastFlowControlCode code = fcn(srco, dat);
                switch (code)
                {
                    case ysRayCastFlowControlCode::e_stop:
                        return;
                    case ysRayCastFlowControlCode::e_continue:
                        continue;
                    case ysRayCastFlowControlCode::e_clip:
                        rci.m_maxLambda = rco.m_lambda;
                        continue;
                    default:
                        ysAssert(false);
                        return;
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ysBVH::DebugDraw(const ysDrawInputBVH& input) const
{
    Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);

    if (input.depth < 0)
    {
        for (ys_int32 i = 0; i < m_nodeCount; ++i)
        {
            const ysAABB& aabb = m_nodes[i].m_aabb;
            ysVec4 halfDims = (aabb.m_max - aabb.m_min) * ysVec4_half;
            ysTransform xf;
            xf.q = ysQuat_identity;
            xf.p = (aabb.m_min + aabb.m_max) * ysVec4_half;
            input.debugDraw->DrawWireBox(halfDims, xf, color);
        }
        return;
    }

    if (input.depth > m_depth - 1)
    {
        return;
    }

    const ys_int32 k_stackSize = 256;
    ysAssert(input.depth < k_stackSize);
    ys_int32 nodeIndexStack[k_stackSize];
    ys_int32 nodeDepthStack[k_stackSize];
    nodeIndexStack[0] = 0;
    nodeDepthStack[0] = 0;
    ys_int32 stackCount = 1;
    while (stackCount > 0)
    {
        stackCount--;
        const Node* node = m_nodes + nodeIndexStack[stackCount];
        ys_int32 depth = nodeDepthStack[stackCount];
        if (depth == input.depth)
        {
            const ysAABB& aabb = node->m_aabb;
            ysVec4 halfDims = (aabb.m_max - aabb.m_min) * ysVec4_half;
            ysTransform xf;
            xf.q = ysQuat_identity;
            xf.p = (aabb.m_min + aabb.m_max) * ysVec4_half;
            input.debugDraw->DrawWireBox(halfDims, xf, color);
            continue;
        }
        ysAssert(depth < input.depth);
        ysAssert((node->m_left == ys_nullIndex) == (node->m_right == ys_nullIndex));
        if (node->m_left != ys_nullIndex)
        {
            nodeIndexStack[stackCount] = node->m_left;
            nodeDepthStack[stackCount] = depth + 1;
            stackCount++;
            nodeIndexStack[stackCount] = node->m_right;
            nodeDepthStack[stackCount] = depth + 1;
            stackCount++;
        }
    }
}