# Never commit untested code

Commits must only be made after the user has confirmed the feature works
correctly at runtime. Do not commit based on static analysis (type-checking,
linting, test suite) alone — wait for the user to test and confirm before
committing.

This avoids repeated amend cycles and regressions that only surface at
runtime (profile closure, snap interaction, constraint interference).
