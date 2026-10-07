# Kanvex Web Interface

## Direction

- Feel: compact automotive diagnostic console with calm, technical hierarchy
- Domain cues: CAN bus, live telemetry, ECU diagnostics, warning states, workshop instruments
- Color world: graphite dashboard, black trim, muted steel, Infiniti gold, semantic green/red/blue
- Signature: gold diagnostic labels and large monospaced live values on dark instrument-like surfaces

## Depth and spacing

- Use borders-only depth with quiet dark surface shifts; avoid decorative shadows and gradients
- Base spacing unit: 4 px
- Control gaps: 8–12 px; card padding: 14–16 px; section gaps: 24–28 px
- Buttons use an 8 px radius; cards use 10 px; dialogs use 12 px

## Hierarchy

- Live numeric values are the focal elements: 25 px, semibold, monospaced
- Metric names are secondary and muted; PID/DID identifiers use the gold accent
- Supporting notes use muted text with approximately 1.45 line height
- Preserve the established graphite-and-gold palette and existing system font stack

## Navigation pattern

- Main navigation uses equal-width grid buttons with centered labels and visible icons
- Desktop: three equal columns, centered, with a maximum total width of 700 px
- Up to 640 px: two equal columns; an unpaired final button keeps one-column width and never stretches across the row
- Mobile navigation buttons have a minimum height of 54 px and may wrap labels without horizontal scrolling
- Keep every primary destination visible without a menu or disclosure control

## Diagnostic cards

- Use the existing bordered dark card with gold PID/DID identifier, muted name, and monospaced value
- Put contextual help in a native button and native dialog
- Dim stale values without hiding enabled controls
- Convert raw OBD values only when the PID formula is verified; explain bit fields or discarded status flags in help text
