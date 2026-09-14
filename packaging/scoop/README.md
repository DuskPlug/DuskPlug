# Scoop manifest

Manifest source for [Scoop Extras](https://github.com/ScoopInstaller/Extras).

## Submit

1. Fork `ScoopInstaller/Extras`.
2. Add `bucket/duskplug.json` using [`duskplug.json`](duskplug.json) as a template.
3. Update `version` and the 64-bit `url`/`hash` on each release.
4. Open a PR with a short description and a link to the GitHub release. New Extras packages also need a [package-request issue](https://github.com/ScoopInstaller/Extras/issues) and usually ~100 GitHub stars.

## Local install (personal bucket)

```powershell
scoop bucket add duskplug path\to\packaging\scoop
scoop install duskplug
```

Or install directly from the manifest file in this folder after updating paths.
