add_library(genomes_weapons STATIC
    src/WeaponController.cpp
    src/WeaponCatalog.cpp
    src/WeaponGeometryGenerator.cpp
    src/WeaponProcedural.cpp
    src/WeaponHandlingSystem.cpp)

target_include_directories(genomes_weapons PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(genomes_weapons PRIVATE
    "${CMAKE_SOURCE_DIR}/external/json/single_include")
target_link_libraries(genomes_weapons PUBLIC genomes::api genomes::content genomes::foundation)
target_link_libraries(genomes_weapons PUBLIC genomes::proc)
target_compile_features(genomes_weapons PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_weapons)

add_library(genomes::weapons ALIAS genomes_weapons)
