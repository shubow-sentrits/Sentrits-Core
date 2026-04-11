# Release Packaging

This document is for maintainers cutting Sentrits releases.

User install and bootstrap docs live in:

- `get_started.md`
- `packaging/debian.md`
- `packaging/macos.md`

## Release Inputs

Sentrits release packaging is driven by:

- the version in `CMakeLists.txt`
- the pinned web client revision in `packaging/sentrits-web-revision.txt`
- the tag pushed from this repo

The packaged web client revision must match the exact `Sentrits-Web` commit you want bundled into the release.

## Release Workflow

The GitHub Actions workflow is:

- `.github/workflows/release-packaging.yml`

It does the following on tag push:

1. reads `packaging/sentrits-web-revision.txt`
2. checks out `Sentrits-Web` at that exact revision
3. builds host-admin assets from this repo
4. builds packaged Debian and macOS artifacts
5. runs install/bootstrap smoke checks
6. uploads artifacts to the GitHub Release for the tag

Current release artifacts:

- Debian package: `.deb`
- macOS package: `.tar.gz`

## Maintainer Steps

1. Update `packaging/sentrits-web-revision.txt` if the bundled web client should move.
2. Ensure `main` contains the release workflow and packaging changes you want.
3. Confirm the version in `CMakeLists.txt` matches the release you intend to cut.
4. Create and push a version tag:

```bash
git tag -a v0.1.0 -m "Sentrits v0.1.0"
git push origin v0.1.0
```

5. Watch the `Release Packaging` workflow in GitHub Actions.
6. Verify the GitHub Release contains both package artifacts.

## Secrets And Access

`Sentrits-Web` is private, so the release workflow expects:

- `DEPLOY_KEY_SENTRITS_WEB`

That secret must contain the private SSH key for a read-enabled deploy key configured on the `Sentrits-Web` repository.

## Notes

- Release packaging currently uses tag-triggered GitHub Actions, not local ad hoc release scripts.
- macOS release packaging is tarball-first for now. Signing, notarization, and `.pkg` installers are future work.
