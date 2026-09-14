# Release checklist

## Before tagging

1. Bump version: `powershell -File scripts\bump-version.ps1` (patch by default; `-Part minor` or `-Part major` when needed). This updates [`VERSION`](../VERSION), `cpp/src/version.h`, and `cpp/macos/Info.plist`.
3. Bump matching versions in [`cpp/macos/Info.plist`](../cpp/macos/Info.plist) and packaging manifests (winget, Scoop). The MSI product version is taken from `VERSION` when you run `Build-Msi.cmd`.
4. Run tests locally: `cpp\ci-check.cmd` (Windows; runs both Windows and CMake test paths). On Linux/macOS: build `duskplug_tests` with CMake and run it.
5. Optional: build MSI locally with `Build-Msi.cmd`

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

1. Update [`packaging/winget/`](../packaging/winget/) manifests with the new version, MSI SHA256, and release URL.
2. Open a PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) (see [`packaging/winget/README.md`](../packaging/winget/README.md)).
3. Optionally update the Scoop manifest in [`packaging/scoop/`](../packaging/scoop/).
4. Write release notes on GitHub — highlight user-visible changes. Mention code signing when SignPath is active.

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
