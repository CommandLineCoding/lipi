# Contributing to Lipi

Thanks for helping. The project is at the planning stage, so the most useful contributions right now are discussion on open `decision` issues and work on `P0` issues in the current milestone.

## Ground rules

- **No proprietary or network-dependent dependencies.** The app must build for F-Droid: no ML Kit, Firebase, Play Services or closed-source SDKs.
- **Check licenses.** The project is MIT. Do not add (A)GPL-licensed code or dependencies (for example MuPDF, iText, Ultralytics YOLO).
- **Keep the core portable.** Code under the C++ core must not depend on Android types.
- **Models need provenance.** Any bundled or downloadable model comes with a model card, dataset card and training script.

## Workflow

1. Fork the repository and create a branch from `main`.
2. Pick an issue (look for `P0` in the current milestone) and comment so others know it is taken.
3. Keep changes focused, update the docs in the same pull request (see [docs/README.md](docs/README.md)), and open a pull request that links the issue.

## Reporting bugs and ideas

Open an issue. For UI work, reference the relevant frame in the Figma file linked from the README.
