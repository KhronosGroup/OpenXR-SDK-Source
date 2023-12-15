# Changelog Fragments

The OpenXR specification editor uses the "proclamation" package to assemble
changelogs for each public release, which incorporate fragments of changelog
text added by the author of a change in one of these directories.

If you want to preview the changelog or perform a release, you can run a command
like the following to install it locally:

```sh
pipx install proclamation
```

See <https://gitlab.com/proclamation/proclamation> for more details on *proclamation*.

## Fragments

Each change should add a changelog fragment file, whose contents are
Markdown-formatted text describing the change briefly. Reference metadata will
be used to automatically add links to associated issues/merge requests/pull
requests, so no need to add these in your fragment text.

## References

The changelog fragment system revolves around "references" - these are issue
reports, public pull requests, or private pull requests associated with a
change. Each fragment must have at least one of these, which forms the main part
of the filename. If applicable, additional can be added within the file - see
below for details.

The format of references for internal issues/MRs is:

```txt
<ref_type>.<number>.gl
```

where

- `ref_type` is "issue" or "mr"
- `number` is the issue or MR number
- `gl` refers to internal GitLab.

The format of references for public (GitHub) issues/pull requests is:

```txt
<ref_type>.<number>.gh.<repo_name>
```

where

- `ref_type` is "issue" or "pr"
- `number` is the issue or PR number
- `gh` refers to public GitHub
- `repo_name` is the repository name: one of "OpenXR-Docs", "OpenXR-SDK-Source",
  etc.

Your changelog fragment filename is simply the "main" reference with the `.md`
extension added.

To specify additional references in a file, prefix the contents of the changelog
fragment with a block delimited above and below by `---`, with one reference on
each line. (This can be seen as a very minimal subset of "YAML Front Matter", if
you're familiar with that concept.) For example:

```md
---
- issue.35.gl
- mr.93.gl
---
Your changelog fragment text goes here.
```
