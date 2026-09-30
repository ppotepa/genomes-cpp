include(GenomesMeshoptimizer)
genomes_configure_meshoptimizer()

add_library(genomes_geometry STATIC
    src/MeshBuilder.cpp
    src/MeshData.cpp
    src/MeshOperations.cpp
    src/MeshOptimizer.cpp
    src/GeometryRecipe.cpp
    src/PolygonOps.cpp
    src/SolidOps.cpp
    src/TangentSpace.cpp
    src/PrimitiveBuilder.cpp
    src/MeshRepair.cpp
    src/IndexOrderOptimizer.cpp
    src/ProfileSurface.cpp
    src/ParametricSurface.cpp
    src/MeshValidation.cpp)

target_include_directories(genomes_geometry PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_geometry PUBLIC genomes::foundation)
# All toolkit integrations remain private implementation details. Geometry's
# public headers expose only neutral data and Result contracts, never upstream
# MikkTSpace, earcut, meshoptimizer or Manifold types.
if(GENOMES_ENABLE_EARCUT)
    target_link_libraries(genomes_geometry PRIVATE genomes::earcut)
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_EARCUT=1)
else()
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_EARCUT=0)
endif()
if(GENOMES_ENABLE_CSG)
    target_link_libraries(genomes_geometry PRIVATE genomes::manifold)
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_MANIFOLD=1)
else()
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_MANIFOLD=0)
endif()
if(GENOMES_ENABLE_MIKKTSPACE)
    target_link_libraries(genomes_geometry PRIVATE genomes::mikktspace)
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_MIKKTSPACE=1)
else()
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_MIKKTSPACE=0)
endif()
if(GENOMES_ENABLE_MESHOPTIMIZER)
    target_link_libraries(genomes_geometry PRIVATE genomes::meshoptimizer)
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_MESHOPTIMIZER=1)
else()
    target_compile_definitions(genomes_geometry PRIVATE GENOMES_HAS_MESHOPTIMIZER=0)
endif()
target_compile_features(genomes_geometry PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_geometry)
add_library(genomes::geometry ALIAS genomes_geometry)

