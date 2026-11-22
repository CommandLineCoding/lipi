# Contributing to Lipi

Thanks for your interest in Lipi! It's a small, friendly project: a portable C++ core, a few on-device models, and an Android app on top. You don't need to know machine learning to help; most work is ordinary app and systems code.

Ways to help:

- Build a feature or fix a bug from the [issue tracker](https://github.com/CommandLineCoding/lipi/issues)
- Improve the docs ([docs/](docs/), this file, the README)
- Contribute scanned test documents (see [#7](https://github.com/CommandLineCoding/lipi/issues/7))
- Share ideas in [Discussions → Ideas](https://github.com/CommandLineCoding/lipi/discussions/categories/ideas)

## Find something to work on

- **New here?** Start with [`good first issue`](https://github.com/CommandLineCoding/lipi/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22). These are small and well scoped.
- **Done a few PRs?** Try [`help wanted`](https://github.com/CommandLineCoding/lipi/issues?q=is%3Aissue+is%3Aopen+label%3A%22help+wanted%22) or `difficulty: medium`.
- **Want something big?** Issues labelled `difficulty: hard` (models, the OCR pipeline, dewarping) should be discussed in the issue before you write code.
- **Open questions:** issues labelled `decision` need an answer before work can start. Adding your view there is a valuable contribution.
- **What's next for the project:** the [milestones](https://github.com/CommandLineCoding/lipi/milestones) group issues by release.

Labels tell you where the work is (`area: core`, `area: android`, `area: ml`, `area: ocr`, `area: export`, `area: infra`, `area: design`), how soon it is needed (`P0` is the first release, `P1` comes next, `P2` is later) and how big it is (`good first issue`, `difficulty: medium`, `difficulty: hard`).

**Claiming an issue:** comment that you'd like to take it, and a maintainer will assign you. If there's no activity for two weeks, we may hand it to someone else. Just comment again if you're still on it. Please work on one issue at a time until your first PR is merged.

## Ground rules

- **No proprietary or network-dependent dependencies.** The app must build for F-Droid: no ML Kit, Firebase, Play Services or closed-source SDKs.
- **Check licenses.** The project is MIT. Do not add (A)GPL-licensed code or dependencies (for example MuPDF, iText or Ultralytics YOLO).
- **Keep the core portable.** Code under `core/` must not depend on Android types.
- **Models need provenance.** Any bundled or downloadable model comes with a model card, a dataset card and its training script.
- **Test data must be redistributable.** Only add images you have the right to publish, and record their source and license in the dataset card.

## Set up

The project is still being scaffolded, so there's nothing to build yet. The commands below describe the planned workflow and will be filled in when the first code lands ([#4](https://github.com/CommandLineCoding/lipi/issues/4), [#6](https://github.com/CommandLineCoding/lipi/issues/6), [#25](https://github.com/CommandLineCoding/lipi/issues/25)).

### Core and CLI

```bash
git clone https://github.com/<you>/lipi.git
cd lipi
cmake -S core -B build && cmake --build build
./build/cli/lipi-cli scan in.jpg -o out.png
```

### App

Open `android/` in Android Studio, or build from the command line:

```bash
cd android
./gradlew assembleDebug
```

You need the Android NDK for the native core. The versions we build with will be pinned in the repository so builds stay reproducible.

## Where things live

| Path | What's there |
|---|---|
| `core/` | The C++ image pipeline and its C ABI. A good place to start reading |
| `cli/` | The desktop command-line tool built on the core |
| `android/` | The Kotlin + Compose app: screens, CameraX capture, Room database |
| `ml/` | Training scripts and the evaluation harness |
| `models/` | Model cards, dataset cards and checksums |
| `metadata/` | Store listing text, screenshots and changelogs for F-Droid |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the pieces fit together |
| [docs/CORE_API.md](docs/CORE_API.md) | The C ABI contract of the core library |

## Making a change

1. **Branch from `main`**, named `<issue number>-<short-description>`, e.g. `11-perspective-warp`.
2. **Keep the PR focused** on one issue. Unrelated cleanups go in their own PR.
3. **Write commit messages** as `type(scope): summary`, e.g. `feat(core): add perspective warp`. Types: `feat`, `fix`, `docs`, `chore`, `refactor`, `test`. Scopes: `core`, `cli`, `android`, `ml`, `ocr`, `docs`, `ci`.
4. **Run the checks** for the part you changed:

   ```bash
   # core and CLI
   ctest --test-dir build

   # app
   cd android
   ./gradlew lint test
   ```

5. **Open a PR into `main`** and fill in the template. Put `Closes #<issue>` in the description. For UI changes, add screenshots and name the Figma frame you followed.

A maintainer will review it. Small follow-up commits on the same branch are fine; you don't need to squash.

### Core API changes

- Update [docs/CORE_API.md](docs/CORE_API.md) in the same PR.
- Keep the C ABI stable: add functions rather than changing existing signatures.

### Model changes

- Update the model card and dataset card in `models/` in the same PR.
- Report the evaluation numbers (corner error, IoU, latency, size) before and after.

## Questions

Ask in [Discussions → Q&A](https://github.com/CommandLineCoding/lipi/discussions/categories/q-a), or comment on the issue you're working on. No question is too small.

By contributing, you agree that your contributions are licensed under the project's [MIT license](LICENSE), and to follow our [Code of Conduct](CODE_OF_CONDUCT.md).
