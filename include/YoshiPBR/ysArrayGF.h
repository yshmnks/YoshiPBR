#pragma once

#include "YoshiPBR/ysTypes.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, ys_int32 N>
struct ysArrayGF
{
public:
    ysArrayGF();
    ~ysArrayGF();
    void Create();
    void Destroy();
    void SetCapacity(ys_int32);
    void SetCount(ys_int32);
    void PushBack(const T&);
    T* Allocate();
    T* GetEntries();
    ys_int32 GetCapacity() const;
    ys_int32 GetCount() const;
    T& operator[](ys_int32);
    const T& operator[](ys_int32) const;
private:
    T m_buffer[N];
    T* m_entries;
    ys_int32 m_capacity;
    ys_int32 m_count;
};