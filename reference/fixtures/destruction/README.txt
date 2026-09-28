DESTRUCTION REFERENCE FIXTURES
==============================

Schema version: destruction-fixtures-1
SOURCE commit: da885ca68b2ae63154a004574fed00eb9dfeb458

These JSON files contain small semantic cases used for diagnostics and
provenance. They are not runtime assets and the native build does not require
the SOURCE repository. Expected values are contracts/invariants, not renderer
pixel captures.

Cases:
    materials_layers.json       ordered solid/void traversal
    moving_target_impact.json   relative velocity and energy accounting
    rubble_conservation.json    mixed material deposit/remove/relaxation

When a value changes intentionally, add a new fixture schema version and record
the reason and SOURCE provenance in the validation report.
