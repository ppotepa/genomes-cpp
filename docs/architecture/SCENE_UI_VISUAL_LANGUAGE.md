# Scene UI visual language

Status: compact Material-inspired direction; GPU visual acceptance pending.

## Compact component contract

Controls share a 32dp height and 13dp text, with 14dp body text. Use sentence
case for field labels and actions. Spacing follows 4/8/12/16/24dp steps.
Neutral charcoal surfaces (#12151b, #181c24, #232933) and one pale-blue accent
(#a9c7ff) establish hierarchy without nested outlined cards. Corners stay square.
This is an RmlUi-native adaptation, not a dependency on React/Material UI.

Every field uses a fixed label column (112dp, 104dp in Unit Lab) and a flexible
control column. Range controls share that column with a fixed 50dp readout.
Checkboxes and helper text follow the same axis. Disclosure bodies have no
horizontal padding: opening a group must not shift its controls. Long headings
wrap instead of clipping; dense gene rows may use a full-width label above the
slider and reset action. Readout text such as seeds is not a fixed-width chip.

Shared controls, disclosure, presets, list rows and readouts live in theme.rcss.
Unit Lab only sets scene geometry and its label-column width. The viewport
remains transparent. Do not use filters, shadows or effects requiring renderer
features absent from the current RmlUi adapter.

Review the gallery and all eight routes at 1280x720 and 1920x1080 with 75%,
100% and 150% UI scale. Check keyboard focus, disabled state, long labels,
dropdowns, open disclosures and slider dragging in the actual application.
Source checks alone do not establish visual acceptance.

## Intent

The interface is a restrained technical desktop layered over the simulation. The world remains the visual focus; UI surfaces are compact, dark and readable rather than decorative. Cool blue identifies primary actions, cyan identifies keyboard focus, green reports healthy live state, amber marks prototypes or warnings, and red is reserved for destructive actions and validation.

## Ownership

- `mods/core/ui/theme.rcss` owns typography, surfaces, controls, interaction states, spacing patterns, scrollbars and reusable component classes.
- `mods/core/scenes/*/screen.rcss` owns only scene placement, dimensions and exceptional composition.
- Scene RML uses semantic classes such as `screen-header`, `field`, `field-label`, `range-row`, `check-row`, `command-bar`, `metric-list`, `prototype-banner` and `hud-card`.
- `mods/core/ui-test.rml` is the visual contract and should show every reusable state before a new pattern is adopted by a product scene.

## Hierarchy

1. An uppercase eyebrow establishes route and context.
2. A short title and one-line subtitle state purpose.
3. Related fields are grouped by dividers, section labels or collapsible sections.
4. Secondary actions stay neutral or ghosted. One primary action anchors the command bar.
5. Dense diagnostics use metric rows rather than competing with the main content.

## Interaction rules

- Every keyboard-focusable control has a high-contrast cyan outline independent of hover.
- Disabled and read-only states differ from enabled controls in both color and contrast.
- Destructive actions use the danger treatment and remain visually separate from navigation.
- Overlays trap attention with a dim background; scene inspectors and HUD cards do not consume pointer input outside their bounds.
- UI scale must preserve the same hierarchy at 75–150%; fixed pixel values are limited to compact component geometry and scene viewport boundaries.

## Evolution

New styling should first extend an existing semantic component. Add a new shared class only when the pattern is useful in more than one scene or belongs in the gallery. Scene styles must not fork the base appearance of buttons, inputs, selects, focus, validation or disabled states.
