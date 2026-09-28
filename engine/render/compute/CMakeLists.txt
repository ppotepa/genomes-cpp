add_library(genomes_render_compute STATIC
    src/FieldAtlas.cpp)

# The visibility kernel is an optional backend asset.  It is intentionally
# retained as a header-only source so the native/default build does not depend
# on an HLSL compiler or a graphics device.
set(GENOMES_VISIBILITY_BATCH_SHADER
    "${CMAKE_SOURCE_DIR}/shaders/compute/visibility_batch.hlsl")
if(EXISTS "${GENOMES_VISIBILITY_BATCH_SHADER}")
    set_source_files_properties("${GENOMES_VISIBILITY_BATCH_SHADER}"
        PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(genomes_render_compute PRIVATE
        "${GENOMES_VISIBILITY_BATCH_SHADER}")
endif()

# Keep the reference shader visible to IDEs and packaging without making the
# native build depend on a particular shader compiler/backend.
set(GENOMES_FIELD_ATLAS_CLEAR_SHADER
    "${CMAKE_SOURCE_DIR}/shaders/compute/field_atlas_clear.hlsl")
if(EXISTS "${GENOMES_FIELD_ATLAS_CLEAR_SHADER}")
    set_source_files_properties("${GENOMES_FIELD_ATLAS_CLEAR_SHADER}"
        PROPERTIES HEADER_FILE_ONLY TRUE)
    target_sources(genomes_render_compute PRIVATE
        "${GENOMES_FIELD_ATLAS_CLEAR_SHADER}")
endif()

target_include_directories(genomes_render_compute PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(genomes_render_compute PUBLIC genomes::foundation)
target_compile_features(genomes_render_compute PUBLIC cxx_std_20)
genomes_set_project_warnings(genomes_render_compute)

add_library(genomes::render_compute ALIAS genomes_render_compute)
