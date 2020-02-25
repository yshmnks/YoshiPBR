set(YOSHIPBR_SOURCE_FILES
	common/ysDebugDraw.cpp
    common/ysMath.cpp
	common/ysStructures.cpp
    geo/ysAABB.cpp
    geo/ysBVH.cpp
    geo/ysRay.cpp
    geo/ysShape.cpp
    geo/ysTriangle.cpp
    mat/ysMaterial.cpp
    mat/ysMaterial.h
    mat/ysMaterialStandard.cpp
    mat/ysMaterialStandard.h
    scene/ysScene.cpp
    )

set(YOSHIPBR_HEADER_FILES
    ../include/YoshiPBR/YoshiPBR.h
    ../include/YoshiPBR/ysAABB.h
    ../include/YoshiPBR/ysArrayG.h
    ../include/YoshiPBR/ysArrayGF.h
    ../include/YoshiPBR/ysBVH.h
	../include/YoshiPBR/ysDebugDraw.h
    ../include/YoshiPBR/ysMath.h
    ../include/YoshiPBR/ysRay.h
    ../include/YoshiPBR/ysScene.h
    ../include/YoshiPBR/ysShape.h
	../include/YoshiPBR/ysStructures.h
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