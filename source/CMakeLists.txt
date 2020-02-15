set(YOSHIPBR_SOURCE_FILES
    common/ysMath.cpp
    geo/ysTriangle.cpp
    )

set(YOSHIPBR_HEADER_FILES
    ../include/YoshiPBR/YoshiPBR.h
    ../include/YoshiPBR/ysMath.h
    ../include/YoshiPBR/ysTriangle.h
    ../include/YoshiPBR/ysTypes.h
    )

add_library(YoshiPBR STATIC ${YOSHIPBR_SOURCE_FILES} ${YOSHIPBR_HEADER_FILES})
target_include_directories(YoshiPBR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_include_directories(YoshiPBR PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(YoshiPBR PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "src" FILES ${YOSHIPBR_SOURCE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../include" PREFIX "include" FILES ${YOSHIPBR_HEADER_FILES})