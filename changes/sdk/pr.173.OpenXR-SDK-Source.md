loader: Replace global static initializers with functions that return static locals. With this change, code that includes OpenXR doesn't have to page in this code and initialize these during startup.
