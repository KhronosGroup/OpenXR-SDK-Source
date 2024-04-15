# Registry Changes

Please create changelog fragments in this directory for changes that affect the
`xr.xml` registry. These fragments will be included in both the docs and SDK
changelogs.

See the README.md in the parent directory for information on the format and
naming of changelog fragment files.

## Common entries

During a patch release series, the only thing that should happen to the
registry aside from extensions is "fix" - so include that in your fragment if
applicable.

The most common entries in the registry changelog involve extensions. Use the
following as templates.

For adding/enabling a vendor/multi-vendor extension:

> New vendor extension: `XR_MYVENDOR_myextension`

- Add "provisional" before "vendor" if required.
- If this is an EXT multi-vendor extension, change "vendor" to "multi-vendor".

For adding/enabling a KHR or KHX extension:

> New ratified Khronos extension: `XR_KHR_myextension`

- Add "provisional" before "Khronos" if it is a KHX extension.
- Note that these all require the review period and board ratification!

For reserving one or more extensions:

> Extension reservation: Reserve an extension for VendorName.

- Pluralize "an extension" if reserving multiple extensions
- May prepend "Register author ID and" if applicable (after the colon).
- You may optionally provide information about your plans for those extensions,
  but this is not required, just permitted.
