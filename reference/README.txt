GENOMES C++ REFERENCE PROJECT
=============================

TARGET:
    ppotepa/genomes-cpp

SOURCE:
    ppotepa/genomes

PURPOSE
-------

This directory contains ONLY the minimum reference material needed to migrate
legacy Genomes behavior into the native C++ implementation.

It is NOT a mirror of the legacy repository.

Do not copy the entire private source tree here.

REFERENCE CLASSES
-----------------

reference/source/
    source repository identity and pinned baseline metadata.

reference/fixtures/
    small deterministic inputs/outputs used by parity tests.

reference/golden/
    selected canonical semantic outputs or visual/debug baselines.

reference/schemas/
    versioned data contracts exported from legacy systems.

reference/manifests/
    mapping from legacy subsystem/path/symbol to native chapter/module.

PATH NOTATION USED BY THE MANUAL
--------------------------------

TARGET::<path>
    path in ppotepa/genomes-cpp.

SOURCE::<path>
    path in ppotepa/genomes.

RULE
----

A native implementation chapter may inspect SOURCE directly through the GitHub
connector or a local checkout.

If the chapter needs persistent parity data, it exports ONLY the smallest
necessary fixture/schema/golden artifact into this reference project.

Every exported artifact must include provenance:
    source repository
    source commit
    source path/symbol
    generator/schema version where applicable
    export command/script
    expected parity class
