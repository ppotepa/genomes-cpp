This directory contains third-party source trees only.

Dependencies are pinned by the parent repository's gitlinks.  Initialize them
with:

    git submodule update --init --recursive external/DiligentEngine/DiligentCore

The normal Genomes configuration only needs DiligentCore.  DiligentTools,
DiligentFX and DiligentSamples remain uninitialized because they are disabled
by CMake; initialize the complete upstream tree only when working on those
upstream modules.

Do not implement Genomes features by editing vendor files in place.  An
upstream patch must be documented and preferably kept as an upstreamable
commit.  CMake configuration for external projects belongs in
cmake/GenomesDependencies.cmake; domain targets must not download sources at
configure or build time.
