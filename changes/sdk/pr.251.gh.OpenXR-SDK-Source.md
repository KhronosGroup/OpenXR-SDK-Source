---
- issue.174.gh.OpenXR-SDK-Source
- issue.1554.gl
- issue.1338.gl
---
Hide prototypes for extension functions unless explicitly requested by defining `XR_EXTENSION_PROTOTYPES`. These functions are not exported from the loader, so having their prototypes available is confusing and leads to link errors, etc.
