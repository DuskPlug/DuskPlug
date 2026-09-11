# Scoop manifest

Manifest source for [Scoop Extras](https://github.com/ScoopInstaller/Extras).

## Submit

1. Fork `ScoopInstaller/Extras`.
2. Add `bucket/duskplug.json` using [`duskplug.json`](duskplug.json) as a template.
3. Update `version`, `url`, and `hash` on each release.
4. Open a PR with a short description and link to the GitHub release.

## Local install (personal bucket)

```powershell
scoop bucket add duskplug path\to\packaging\scoop
scoop install duskplug
```

Or install directly from the manifest file in this folder after updating paths.
