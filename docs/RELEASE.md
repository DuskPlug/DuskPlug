# Release checklist

## Before tagging

1. Bump version: `powershell -File scripts\bump-version.ps1` (patch by default; `-Part minor` or `-Part major` when needed). This updates [`VERSION`](../VERSION), `cpp/src/version.h`, and `cpp/macos/Info.plist`.
2. Bump matching versions in packaging manifests (winget, Scoop) and the landing page version badge in [`docs/site/index.html`](site/index.html). The MSI product version is taken from `VERSION` when you run `Build-Msi.cmd`.
3. Run tests locally: `cpp\ci-check.cmd` (Windows; runs both Windows and CMake test paths). On Linux/macOS: build `duskplug_tests` with CMake and run it.
4. Optional: build MSI locally with `Build-Msi.cmd`

## Publish

1. Create and push an annotated tag:
   ```cmd
   git tag -a v1.0.1 -m "v1.0.1"
   git push origin v1.0.1
   ```
2. GitHub Actions ([`.github/workflows/ci.yml`](../.github/workflows/ci.yml)) runs tests, builds `DuskPlug.exe`, `DuskPlug.msi`, `DuskPlug-Windows.zip`, Linux and macOS packages, then uploads them to the GitHub release.
3. If SignPath secrets are configured, the release job submits binaries for Authenticode signing before upload.
4. The `release-manifest` job computes SHA256 for each asset and uploads **`latest.json`** to the same release. In-app updaters fetch it from `https://github.com/DuskPlug/DuskPlug/releases/latest/download/latest.json`.

## After the release

1. **winget** — Update [`packaging/winget/`](../packaging/winget/) with new version, MSI SHA256, and release URL. Open a PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) (see [`packaging/winget/README.md`](../packaging/winget/README.md)).
2. **Scoop bucket** — Update [`packaging/scoop/duskplug.json`](../packaging/scoop/duskplug.json) and push the same change to [DuskPlug/scoop-bucket](https://github.com/DuskPlug/scoop-bucket).
3. **Scoop Extras** (optional, when ~100 GitHub stars) — Open a package-request issue and PR to [ScoopInstaller/Extras](https://github.com/ScoopInstaller/Extras).
4. **Landing page** — Update version string and download links in [`docs/site/index.html`](site/index.html). Push to `master`; the Pages workflow deploys automatically.
5. **Release notes** — Write notes on GitHub. Include searchable phrases naturally (e.g. "Tuya Windows tray app", "dusk dawn smart plug"). Mention code signing when SignPath is active.

## Discoverability

- **Landing page:** https://duskplug.github.io/DuskPlug/
- **Google Search Console:** follow [`docs/SEARCH-CONSOLE.md`](SEARCH-CONSOLE.md) after Pages deploys.
- **Community posts:** use drafts in [`docs/ANNOUNCEMENTS.md`](ANNOUNCEMENTS.md) and schedule in [`docs/LAUNCH-SCHEDULE.md`](LAUNCH-SCHEDULE.md).

## SignPath (when approved)

Update your SignPath application so project URLs point at **https://github.com/DuskPlug/DuskPlug** (repo moved from a personal account to the `DuskPlug` org).

Add these repository secrets in GitHub → Settings → Secrets → Actions:

| Secret | Description |
|--------|-------------|
| `SIGNPATH_API_TOKEN` | API token from SignPath |
| `SIGNPATH_ORGANIZATION_ID` | Organization ID |
| `SIGNPATH_PROJECT_SLUG` | Project slug |
| `SIGNPATH_SIGNING_POLICY_SLUG` | Signing policy slug |

Until all four are set, CI builds unsigned release assets as today.
