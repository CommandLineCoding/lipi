# Lipi documentation

Documentation follows the [Diátaxis](https://diataxis.fr) framework. Each page belongs to exactly one of four kinds:

| Kind | Purpose | Folder |
| --- | --- | --- |
| Tutorials | Learning-oriented lessons that take a newcomer through a first success | [tutorials/](tutorials/) |
| How-to guides | Task-oriented steps for a specific goal | [how-to/](how-to/) |
| Reference | Accurate, complete descriptions (C ABI, CLI flags, file formats) | [reference/](reference/) |
| Explanation | Background and reasoning (architecture, privacy model, model choices) | [explanation/](explanation/) |

Design decisions are recorded as architecture decision records in [decisions/](decisions/). Each open question in the issue tracker (label `decision`) ends as an ADR.

## Writing rules

- One kind per page; link across kinds instead of mixing them.
- Keep pages short, in plain language, with runnable examples.
- Update the docs in the same pull request as the change.
