add_library(genomes_infantry STATIC
    src/InfantrySimulation.cpp
    src/InfantryUnitController.cpp
    src/InfantryGenome.cpp
src/PhenotypeResolver.cpp
src/FaceAnatomy.cpp
    src/RigBuilder.cpp
src/AppearanceCompiler.cpp
src/AppearanceMeshBuilder.cpp
src/AppearanceMeshFinalizer.cpp
src/ReferenceSurfaceBuilder.cpp
    src/ReferenceBodySurfaceGenerator.cpp
    src/ReferenceFaceSurfaceGenerator.cpp
src/AppearanceMorphBuilder.cpp
    src/HairGenerator.cpp
    src/BodySurfaceGenerator.cpp
    src/FaceSurfaceGenerator.cpp
    src/GearSurfaceGenerator.cpp
    src/Equipment.cpp
    src/EquipmentCatalogLoader.cpp
    src/GearGenerator.cpp
    src/PostureProfile.cpp
    src/LocomotionController.cpp
    src/TwoBoneIK.cpp
    src/GroundContact.cpp
    src/FaceAnimator.cpp
    src/AnimationSystem.cpp
    src/PresentationAnimation.cpp
    src/InfantryDamage.cpp
    src/RagdollController.cpp
    src/AppearanceCatalog.cpp
    src/InfantryModelCompiler.cpp
    src/InfantryProcedural.cpp)

target_include_directories(genomes_infantry PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(genomes_infantry PRIVATE
    "${CMAKE_SOURCE_DIR}/external/json/single_include")
target_link_libraries(genomes_infantry PUBLIC
    genomes::api
genomes::foundation
    genomes::content
genomes::geometry
    genomes::simulation
    genomes::spatial
    genomes::jobs
    genomes::navigation)
target_link_libraries(genomes_infantry PUBLIC genomes::proc)
target_link_libraries(genomes_infantry PUBLIC genomes::combat)
target_link_libraries(genomes_infantry PUBLIC genomes::weapons)
target_link_libraries(genomes_infantry PUBLIC genomes::physics)
target_compile_features(genomes_infantry PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_infantry)

add_library(genomes::infantry ALIAS genomes_infantry)
