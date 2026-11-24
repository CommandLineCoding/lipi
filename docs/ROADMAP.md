# Lipi — roadmap

Where the project is going, in what order, and what has to be true before each release. The live list of issues is [#166](https://github.com/CommandLineCoding/lipi/issues/166); this document holds what a checklist cannot: the order of work, dependencies, risks and exit criteria.

Dates are targets for planning, not promises. They are reviewed at every release.

---

## 1. Rules for what gets built

Every proposed feature is judged against the rules in [#107](https://github.com/CommandLineCoding/lipi/issues/107):

- **Offline by default.** Nothing leaves the phone unless the user starts it: an export, a share or an explicit download.
- **No accounts, no analytics, no telemetry, no ads.**
- **Open licences.** Dependencies and models are MIT, Apache-2.0, BSD or similar. No GPL or AGPL code is bundled.
- **Models are documented.** Each has a model card, the training data and its licence, and measured results.
- **No server.** A feature that needs one is out of scope.

Out of scope: cloud sync, accounts, usage statistics, and generative features that call a server. Scanning to a computer over the network is parked until the network decision ([#20](https://github.com/CommandLineCoding/lipi/issues/20)) is made.

---

## 2. Milestones

| Milestone | Theme | Target | Exit criteria |
|---|---|---|---|
| **v0.1.0** | Android MVP | Dec 2026 | Scan to PDF or JPG works on real devices of at least two models. CI is green, including the no-network, 16 KB alignment and dependency checks. The build is reproducible, the APK is signed and the F-Droid metadata is ready. The two decisions below are recorded. |
| **v0.2.0** | OCR and models | Mar 2027 | Bengali, Hindi and English text recognition with measured error rates in the model cards. Searchable PDF opens correctly in common viewers. Search works across scans. Packs are signed and verified. |
| **v0.3.0** | Layout and privacy | Jun 2027 | Dewarped pages, tables, MRC compression. Redaction removes the underlying pixels and text, and a verifier proves it. |
| **v0.4.0** | Workflow and library | Sep 2027 | Tags and folders, page tools, trash, sign and annotate, PDF/A, export presets and size limits. |
| **v0.5.0** | Capture and restoration | Dec 2027 | Scan modes, multi-document photos, glare, finger and show-through removal, re-processing old scans. |
| **v1.0.0** | Ecosystem and trust | Mar 2028 | Open scanning API, signed scans, encrypted vault, accessibility pass, stable storage format with upgrade tests. |

---

## 3. v0.1.0: what is done and what is left

**Done.** Capture with a live page outline (PageNet on ncnn, about 18 ms per frame on one phone), adjusting corners with a loupe, enhancing with filters, sliders, compare, rotate, crop and add page, export as PDF or JPG through the system file picker, and a library that shows the saved scans.

**Left, in this order:**

1. **Decisions.** The network model ([#20](https://github.com/CommandLineCoding/lipi/issues/20)) and whether documents keep their originals ([#103](https://github.com/CommandLineCoding/lipi/issues/103)). Both change what the next items build.
2. **Document view** ([#104](https://github.com/CommandLineCoding/lipi/issues/104)), after the originals decision, because it sets what "export again" works from.
3. **Settings** ([#37](https://github.com/CommandLineCoding/lipi/issues/37)) and the **dark and light check** ([#38](https://github.com/CommandLineCoding/lipi/issues/38)), with the slider fix ([#105](https://github.com/CommandLineCoding/lipi/issues/105)).
4. **Auto-capture** ([#29](https://github.com/CommandLineCoding/lipi/issues/29)) and **quality hints** for low light and blur ([#31](https://github.com/CommandLineCoding/lipi/issues/31)).
5. **App icon** ([#167](https://github.com/CommandLineCoding/lipi/issues/167)) and the open triage item ([#106](https://github.com/CommandLineCoding/lipi/issues/106)).
6. **Release work.** Finish the dependency guard ([#41](https://github.com/CommandLineCoding/lipi/issues/41)), reproducible builds ([#42](https://github.com/CommandLineCoding/lipi/issues/42)), the signed release pipeline ([#45](https://github.com/CommandLineCoding/lipi/issues/45)) and the F-Droid metadata ([#44](https://github.com/CommandLineCoding/lipi/issues/44)). Screenshots for the store listing come from the finished app, so this goes last.
7. **Release candidate.** Test on at least two devices and check battery use during a live preview. Publishing a release or tag waits for the maintainer's approval.

---

## 4. Order and dependencies after v0.1.0

**Text recognition (v0.2.0)** has a fixed order:

- The OCR pipeline ([#46](https://github.com/CommandLineCoding/lipi/issues/46)) and the evaluation harness for error rates ([#48](https://github.com/CommandLineCoding/lipi/issues/48)) come first, so no claim of support is made without a measurement.
- The pack format and signatures ([#108](https://github.com/CommandLineCoding/lipi/issues/108)) come before the Indic packs ([#49](https://github.com/CommandLineCoding/lipi/issues/49)), because packs need somewhere to live.
- Searchable PDF ([#51](https://github.com/CommandLineCoding/lipi/issues/51), [#52](https://github.com/CommandLineCoding/lipi/issues/52)), full-text search ([#54](https://github.com/CommandLineCoding/lipi/issues/54)) and background OCR ([#53](https://github.com/CommandLineCoding/lipi/issues/53)) follow the pipeline.
- The Models screen ([#109](https://github.com/CommandLineCoding/lipi/issues/109)) and adding a model from a file ([#110](https://github.com/CommandLineCoding/lipi/issues/110)) follow the pack format. How downloads work is the open decision [#20](https://github.com/CommandLineCoding/lipi/issues/20).

**Privacy tools (v0.3.0)** build on each other: the ID number validators ([#120](https://github.com/CommandLineCoding/lipi/issues/120)) feed redaction ([#68](https://github.com/CommandLineCoding/lipi/issues/68)), the share guard ([#122](https://github.com/CommandLineCoding/lipi/issues/122)) and the redaction verifier ([#119](https://github.com/CommandLineCoding/lipi/issues/119)). They need the OCR text from v0.2.0.

**Workflow (v0.4.0).** Export size limits ([#128](https://github.com/CommandLineCoding/lipi/issues/128)) work with plain JPEG compression and get better when MRC ([#66](https://github.com/CommandLineCoding/lipi/issues/66)) lands. The two-sided ID page ([#130](https://github.com/CommandLineCoding/lipi/issues/130)) needs the scan modes ([#56](https://github.com/CommandLineCoding/lipi/issues/56)).

**Originals.** If documents keep their originals, then retention rules ([#136](https://github.com/CommandLineCoding/lipi/issues/136)) and re-processing old scans ([#151](https://github.com/CommandLineCoding/lipi/issues/151)) become possible. If not, they are dropped.

---

## 5. Decisions that gate the plan

**The network model ([#20](https://github.com/CommandLineCoding/lipi/issues/20)).** Three options are written up in the issue. It decides whether one-tap model downloads exist, what the Settings and welcome screens say, and the fate of [#50](https://github.com/CommandLineCoding/lipi/issues/50) and [#61](https://github.com/CommandLineCoding/lipi/issues/61). It must be settled before the Models screen is finished.

**Keeping originals ([#103](https://github.com/CommandLineCoding/lipi/issues/103)).** It changes the stored format, so it must be settled before the first release. Changing the format afterwards needs a migration for every user's library.

---

## 6. Risks

| Risk | Why it matters | What we do |
|---|---|---|
| Bengali and Hindi text recognition quality | Open training data is thin, handwriting especially. | Measure with the evaluation harness first. Ship printed text first. Publish error rates in the model cards. |
| Build time and F-Droid | ncnn built from source takes about 7 minutes cold. | ccache in CI, pinned versions, the reproducible-build check ([#42](https://github.com/CommandLineCoding/lipi/issues/42)). |
| Only one physical test phone | Speed and battery differ a lot across devices. | At least two devices before v0.1.0, measurements per device tier, an open call for testers. |
| Licences of training data | The ID-card data used for the small page model is CC BY-SA, so that model is not bundled. | State the licence in every model card. Prefer clean data. |
| Changing the stored format | Painful after release. | Settle [#103](https://github.com/CommandLineCoding/lipi/issues/103) now. Upgrade tests are [#165](https://github.com/CommandLineCoding/lipi/issues/165). |
| Too much scope | More than 100 issues. | Every milestone has a theme and exit criteria. New ideas pass section 1 and wait as P2 until a milestone has room. |

---

## 7. Definition of done

An issue is done when:

- it has tests, and the part that cannot be unit tested was checked on a real device;
- it works in the light and dark themes and with large text;
- screens have content descriptions and touch targets of at least 48 dp (the current round buttons are smaller, see [#168](https://github.com/CommandLineCoding/lipi/issues/168));
- the docs are updated, and a model has its model card with measured results;
- it follows the rules in section 1;
- CI is green.
