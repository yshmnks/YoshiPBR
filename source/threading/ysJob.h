#pragma once

#include "YoshiPBR/ysTypes.h"
#include <atomic>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ysJob
{
    void Reset();
    void Create(void(*fcn)(void*), void* fcnArg, ysJob* parent);

    void Execute();
    bool IsFinished() const;

    void __Finish();

    void(*m_fcn)(void*);
    void* m_fcnArg;
    ysJob* m_parent;
    std::atomic<ys_int32> m_unfinishedJobCount;

    /////////////
    // PADDING //
    /////////////
    struct PayloadFormat
    {
        void(*a)(ysJob*);
        void* b;
        ysJob* c;
        std::atomic<ys_int32> d;
    };
    static constexpr std::size_t PAYLOAD_SIZE = sizeof(PayloadFormat);
    static constexpr std::size_t FALSE_SHARING_SIZE = std::hardware_destructive_interference_size;
    ys_uint8 _PAD[FALSE_SHARING_SIZE - PAYLOAD_SIZE];
};