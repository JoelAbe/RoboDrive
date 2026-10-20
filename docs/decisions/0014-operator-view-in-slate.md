# 0014. Build the operator view in Slate, fed by a telemetry snapshot

- **Status:** Accepted

## Context

The operator view is an overlay on the game: panels that show what the robot is
doing, and controls to change it. Unreal offers two ways to build UI:

- **UMG Widget Blueprints**, laid out in a visual designer. The usual choice for
  game interfaces, but stored as binary assets that cannot be read, diffed or
  reviewed, and that only the editor can change.
- **Slate**, Unreal's C++ UI framework, which UMG is built on and the editor
  itself is written in. Layouts are code: verbose, with no visual designer, but
  readable and reviewable like everything else in the project.

There is also the question of where the view gets its data. Reading actors and
subsystems from inside layout code would mix "what does the operator need to
know?" with "how should it look?".

## Decision

The view is written in Slate. A thin UMG widget wraps it, so the player
controller creates it and adds it to the screen like any other widget, with no
Widget Blueprint to assign.

What it shows is gathered once per frame into a plain struct,
`FRoboOperatorTelemetry`, which reads the vehicle, its components and the
simulation's subsystems. Every value on screen reads from that one snapshot.
Colours, fonts, sizes and brushes are defined in a single style file.

Ground truth appears only in its own labelled section. The panel may show the
operator what the robot cannot know; it never passes it to a brain
([ADR 0009](0009-brains-receive-a-snapshot.md)).

Operator actions, from keys or buttons, go through the player controller.

## Consequences

- The whole interface can be reviewed in a diff, including its layout.
- Changing the look means editing hex values in one file.
- Tweaking spacing means recompiling rather than dragging in a designer. Live
  Coding makes that quick.
- Values missing for a real reason, such as no goal or no lidar, show as
  missing rather than as zero, because each group in the snapshot carries a
  flag.
- All values on screen describe the same moment.
- The panels overlay the game view rather than shrinking it, and Tab hides
  them. Sizes scale with the screen's DPI, so they keep the same proportion at
  any resolution.
- Slate is less familiar than UMG to most game developers, so the layout code
  is commented as it goes.
