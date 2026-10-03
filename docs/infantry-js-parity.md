# Infantry JavaScript mockup and native C++ validation

Status: exact JS-to-C++ parity is not a product requirement or release gate.

`reference/code` and its pinned fixtures document the origins of the infantry
concept and provide concrete examples of characters, equipment, animation
states, and visual direction. They are useful for exploration, provenance, and
design discussion. They do not define the C++ implementation or require the
same algorithms, intermediate values, vertex numbering, buffer order, or final
numeric output.

Native C++ behavior is defined by the owning C++ contracts and focused native
tests. Those tests should assert meaningful requirements such as deterministic
generation for a fixed request, finite and valid geometry, stable rig/artifact
schemas, bounded animation transitions, reachable IK targets, and ground-contact
constraints where those are product requirements. They should not compare every
output component to JavaScript merely because a fixture exists.

The old exact-comparison executable remains available only as an opt-in
investigation tool:

```text
cmake -DGENOMES_BUILD_INFANTRY_REFERENCE_COMPARISON=ON ...
```

It is not part of the default CTest run and its result does not determine
acceptance. `GENOMES_ENABLE_JS_REFERENCE_PARITY` controls regeneration and
byte-for-byte reproduction of checked-in reference artifacts; it does not make
those artifacts normative for C++.

The native fixture reader remains useful for checking GNIF structure, stream
types, lengths, hashes, and provenance. Passing that reader proves the fixture is
well-formed, not that native output must reproduce it.

## Historical record

Earlier revisions treated the JS exporter and numerical comparisons as a strict
porting contract. Their stage tables, tolerances, and pending mismatch reports
are historical investigation records only. In particular, the former
`infantry.reference_parity` gate has been replaced by the opt-in
`infantry.reference_comparison` diagnostic. Do not use old parity percentages
or statuses as current completion criteria.

Fixture regeneration must still be explicit and source-pinned so examples remain
reproducible. Changing a fixture does not change the native contract; changing a
native contract requires an explicit C++ design decision and corresponding
native tests.
