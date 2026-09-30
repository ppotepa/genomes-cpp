This directory contains third-party source trees only.

Dependencies are pinned by the parent repository's gitlinks.  Initialize them
with:

    git submodule update --init --recursive external/DiligentEngine

The normal Genomes configuration uses DiligentCore and DiligentTools for the
Windows/D3D12 renderer and capture encoder. DiligentFX, samples and upstream
tests remain disabled by CMake; initialize the complete upstream tree only when
working on those upstream modules.

Do not implement Genomes features by editing vendor files in place.  An
upstream patch must be documented and preferably kept as an upstreamable
commit.  CMake configuration for external projects belongs in
cmake/GenomesDependencies.cmake and
cmake/GenomesToolkitDependencies.cmake; domain targets must not download
sources at configure or build time.

Lightweight toolkit pins
------------------------

* meshoptimizer `9e1f07b159d3cb777f1c67ed31fc11fd117986f4`: optional CPU index
  order preparation; core geometry never remaps vertices implicitly.
* MikkTSpace `3e895b49d05ea07e4c2133156cfa94369e19e409`: tangent generation;
  private C target, no public vendor types.
* earcut.hpp `c68c8835ccff2b7532d31d8fa8dfcf398f629498`: polygon triangulation;
  upstream tests/benchmarks/viz are disabled.
* fastgltf `0d1b67a28c4950ea2deb796702006dcbe31e02b3`: opt-in static glTF
  importer with pinned simdjson `7382dc2be88e53fbc35cb50369b831855656f0fd`
  single-header sources, no tests/examples/docs and no configure-time download.
* Manifold `0edd9d54876f3135e431575214dd6d8a72866fee`: opt-in CSG; downloads,
  tests, bindings, and examples are disabled.

The first three are core geometry implementation dependencies. fastgltf and
Manifold are never linked into the default game when their options are OFF.
