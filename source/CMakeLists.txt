set(YOSHIPBR_SOURCE_FILES
	common/ysDebugDraw.cpp
    common/ysMath.cpp
    common/ysMemoryPool.cpp
    common/ysProbability.cpp
    common/ysProbability.h
    common/ysRadiometry.cpp
    common/ysRadiometry.h
	common/ysStructures.cpp
    common/ysThreading.cpp
    common/ysUnitTests.cpp
    geo/ysAABB.cpp
    geo/ysBVH.cpp
    geo/ysEllipsoid.cpp
    geo/ysRay.cpp
    geo/ysShape.cpp
    geo/ysTriangle.cpp
    light/ysLight.cpp
    light/ysLight.h
    light/ysLightPoint.cpp
    light/ysLightPoint.h
    mat/emissive/ysEmissiveMaterial.cpp
    mat/emissive/ysEmissiveMaterial.h
    mat/emissive/ysEmissiveMaterialUniform.cpp
    mat/emissive/ysEmissiveMaterialUniform.h
    mat/reflective/ysMaterial.cpp
    mat/reflective/ysMaterial.h
    mat/reflective/ysMaterialMirror.cpp
    mat/reflective/ysMaterialMirror.h
    mat/reflective/ysMaterialStandard.cpp
    mat/reflective/ysMaterialStandard.h
    scene/ysRender.cpp
    scene/ysRender.h
    scene/ysScene.cpp
    scene/ysScene.h
    threading/ysJobSystem.cpp
    threading/ysJobSystem.h
    threading/ysParallelAlgorithms.cpp
    threading/ysParallelAlgorithms.h
    )

set(YOSHIPBR_HEADER_FILES
    ../include/YoshiPBR/YoshiPBR.h
    ../include/YoshiPBR/ysAABB.h
    ../include/YoshiPBR/ysArrayG.h
    ../include/YoshiPBR/ysArrayGF.h
    ../include/YoshiPBR/ysMemoryPool.h
    ../include/YoshiPBR/ysPool.h
    ../include/YoshiPBR/ysBVH.h
	../include/YoshiPBR/ysDebugDraw.h
    ../include/YoshiPBR/ysEllipsoid.h
    ../include/YoshiPBR/ysMath.h
    ../include/YoshiPBR/ysRay.h
    ../include/YoshiPBR/ysShape.h
	../include/YoshiPBR/ysStructures.h
    ../include/YoshiPBR/ysThreading.h
    ../include/YoshiPBR/ysTriangle.h
    ../include/YoshiPBR/ysTypes.h
    ../include/YoshiPBR/ysUnitTests.h
    )

add_library(YoshiPBR STATIC ${YOSHIPBR_SOURCE_FILES} ${YOSHIPBR_HEADER_FILES})
target_include_directories(YoshiPBR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_include_directories(YoshiPBR PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(YoshiPBR PRIVATE -W4 -WX)
set_target_properties(YoshiPBR PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "src" FILES ${YOSHIPBR_SOURCE_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../include" PREFIX "include" FILES ${YOSHIPBR_HEADER_FILES})