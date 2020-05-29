# SDK Changes

Please create changelog fragments in this directory for changes that affect
SDK code only. These fragments will only be applied to the
SDK changelog.

See the README.md in the parent directory for information on the format and
naming of changelog fragment files.

## Format tips

Since there are a variety of components included in the SDK code, it's generally
helpful to format your fragment as follows:

> Loader: Fix the fooing of the bar.

Note the following aspects:

- Starts with component name ("Loader") and a colon. Skip this if your change
  affects multiple different components that can't be easily grouped (e.g.
  Layers can be grouped, but a change affecting hello_xr and the loader can't
  be.) Component names (formatted correctly) include:
  - Loader
  - Layers (use this if your change affects more than one layer)
  - API Dump Layer
  - Validation Layer
  - hello_xr
- Description of the human-readable change effect, in the "imperative". (That
  is, as a command/instruction, like a good commit message: "Fix problem" not
  "Fixes problem" or "Fixed problem".) This may be more than one sentence, if
  needed.
- Sentence or sentences end with a period.
