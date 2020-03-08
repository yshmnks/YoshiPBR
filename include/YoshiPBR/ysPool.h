#pragma once

#include "YoshiPBR/ysMath.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct ysPool
{
    // This is an array with holes. To populate this with your struct T, T should contain the following unioned members:
    //
    //  struct T
    //  {
    //      ...
    //      union
    //      {
    //          ys_int32 m_poolIndex;
    //          ys_int32 m_poolNext;
    //      };
    //  };
    //
    // When querying for the element e corresponding to a particular array index i, you should check whether the result is a hole by comparing
    // e.m_poolIndex == i. If they are NOT equal, then e is a hole and the result of the query invalid.

public:
    ysPool();
    ~ysPool();

    void Create(ys_int32 capacity);
    void Create();
    void Destroy();

    ys_int32 Allocate();
    void Free(ys_int32 index);

    ys_int32 GetCount() const;
    ys_int32 GetCeiling() const;
    ys_int32 GetCapacity() const;
    T& operator[](ys_int32);
    const T& operator[](ys_int32) const;

private:
    T* m_data;
    ys_int32 m_count;
    ys_int32 m_ceiling;
    ys_int32 m_capacity;
    ys_int32 m_freeList;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
ysPool<T>::ysPool()
{
    Create();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
ysPool<T>::~ysPool()
{
    Destroy();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void ysPool<T>::Create(ys_int32 capacity)
{
    ysAssert(capacity >= 0);
    m_count = 0;
    m_ceiling = 0;
    m_capacity = capacity;

    if (capacity == 0)
    {
        m_data = nullptr;
        m_freeList = ys_nullIndex;
    }
    else
    {
        m_data = static_cast<T*>(ysMalloc(sizeof(T) * capacity));
        m_freeList = 0;
        for (ys_int32 i = 0; i < capacity - 1; ++i)
        {
            m_data[i].m_poolNext = i + 1;
        }
        m_data[capacity - 1].m_poolNext = ys_nullIndex;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void ysPool<T>::Create()
{
    Create(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void ysPool<T>::Destroy()
{
    m_count = 0;
    m_ceiling = 0;
    m_capacity = 0;
    ysFree(m_data);
    m_freeList = ys_nullIndex;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
ys_int32 ysPool<T>::Allocate()
{
    if (m_count == m_capacity)
    {
        ysAssert(m_freeList == ys_nullIndex && m_ceiling == m_capacity);

        {
            // expand capacity
            ys_int32 capacity = ysMax(m_capacity + 1, (m_capacity >> 1) + m_capacity);
            T* data = static_cast<T*>(ysMalloc(sizeof(T) * capacity));
            ysMemCpy(data, m_data, sizeof(T) * m_capacity);
            ysSwap(m_data, data);
            m_capacity = capacity;
        }

        {
            // link up free list
            for (ys_int32 i = m_count; i < m_capacity - 1; ++i)
            {
                m_data[i].m_poolNext = i + 1;
            }
            m_data[m_capacity - 1].m_poolNext = ys_nullIndex;
        }

        ys_int32 freeIndex = m_count;
        m_freeList = m_count + 1;
        m_count++;
        m_ceiling++;
        m_data[freeIndex].m_poolIndex = freeIndex;
        return freeIndex;
    }
    else
    {
        ysAssert(m_freeList != ys_nullIndex);
        ys_int32 freeIndex = m_freeList;
        m_freeList = m_data[freeIndex].m_poolNext;
        m_count++;
        m_ceiling = ysMax(m_ceiling, freeIndex);
        m_data[freeIndex].m_poolIndex = freeIndex;
        return freeIndex;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void ysPool<T>::Free(ys_int32 index)
{
    ysAssert(0 <= index && index < m_ceiling);
    ysAssert(m_data[index].m_poolIndex != index);
    m_data[index].m_poolNext = m_freeList;
    m_freeList = index;
    m_count--;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
ys_int32 ysPool<T>::GetCount() const
{
    return m_count;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
ys_int32 ysPool<T>::GetCeiling() const
{
    return m_ceiling;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
ys_int32 ysPool<T>::GetCapacity() const
{
    return m_capacity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
T& ysPool<T>::operator[](ys_int32 index)
{
    ysAssert(0 <= index && index < m_ceiling);
    return m_data[index];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
const T&  ysPool<T>::operator[](ys_int32 index) const
{
    ysAssert(0 <= index && index < m_ceiling);
    return m_data[index];
}