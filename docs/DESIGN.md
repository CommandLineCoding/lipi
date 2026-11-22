# Lipi — design tokens

The app follows the Figma file [FOSS Document Scanner – App Design](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF) (frames: [Library](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF?node-id=2-5), [Camera](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF?node-id=2-218), [Adjust corners](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF?node-id=3-3), [Enhance](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF?node-id=3-43), [Export](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF?node-id=5-3), [Settings](https://www.figma.com/design/7BVcimN4rnR0zgMLh78RXF?node-id=5-104)). The file has no variables, components or dark theme, so this page records the values read from the frames, and the dark palette that was derived from them. The code is in [`android/app/src/main/java/in/commandlinecoding/lipi/ui/theme/`](../android/app/src/main/java/in/commandlinecoding/lipi/ui/theme/).

## Colours

| Token | Light (from Figma) | Dark (derived) | Material 3 role |
|---|---|---|---|
| Background | `#F5F6FA` | `#0E1117` | `background` |
| Surface (cards, search, chips) | `#FFFFFF` | `#171B24` | `surface`, `surfaceVariant` |
| Outline | `#E6E8EF` | `#2A303C` | `outline`, `outlineVariant` |
| Text | `#10131A` | `#E8EAF0` | `onBackground`, `onSurface` |
| Secondary text | `#6B7280` | `#9AA3B2` | `onSurfaceVariant` |
| Primary (buttons, active chip, scan button) | `#3D5AFE` | `#8C9EFF` | `primary` |
| On primary | `#FFFFFF` | `#0B1030` | `onPrimary` |
| Page-count pill background | `#10131A` at 72% | black at 72% | `LipiColors.scrim` |
| Paper title line / text lines | `#C3C9D8` / `#E3E6EE` | `#3A4152` / `#2B3140` | `LipiColors.paperTitle`, `paperLine` |
| Thumbnail tints (blue, green, sand, lilac, teal, rose) | `#E6EAF7` `#E5F0E8` `#F3EADF` `#ECE6F5` `#E4F0F3` `#F2E7E7` | `#1B2133` `#1B2A22` `#2E271D` `#262033` `#1B2A2F` `#2E2323` | `LipiColors.thumbTints` |

The dark values keep the hues and contrast ratios of the light ones; they are a proposal, not part of the Figma file.

## Type

Inter (SIL Open Font License, see [`third_party/fonts/inter/`](../third_party/fonts/inter/)), one variable font file.

| Use | Size and weight |
|---|---|
| Screen title ("Library") | 30 bold |
| Section heading ("Recent") | 15 semibold |
| Body, search placeholder | 15 regular |
| Chip, tab label | 13 medium |
| Card title | 14 semibold |
| Meta text ("6 pages · Today") | 12 regular |
| Pill, bottom-bar label | 11 medium |

## Shape and spacing

Screen side padding 20 dp; grid gap 14 dp; cards, search field 16 dp radius; chips and pills fully rounded; round icon buttons 40 dp; scan button 64 dp with an indigo shadow at 35%; bottom bar 84 dp.

## Gaps in the design

The draft shows only the happy path. Empty, permission-denied, error and loading states are still to be designed and are tracked with the screens that need them. The components (chip, pill, document card, slider, bottom bar) are implemented as Compose components with these tokens; creating the matching components and variables in the Figma file itself is left to the design owner.
