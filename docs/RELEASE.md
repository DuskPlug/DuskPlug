# Release checklist

## Before tagging

1. Bump `Version` in [`installer/DuskPlug.wxs`](../installer/DuskPlug.wxs) to match the new tag.
2. Run tests locally: `cpp\test.cmd`
3. Optional: build MSI locally with `Build-Msi.cmd`

## Publish

1. Create and push an annotated tag:
   ```cmd
   git tag -a v1.0.1 -m "v1.0.1"
   git push origin v1.0.1
   ```
2. GitHub Actions ([`.github/workflows/ci.yml`](../.github/workflows/ci.yml)) runs tests, builds `DuskPlug.exe`, `DuskPlug.msi`, and `DuskPlug-Windows.zip`, then uploads them to the GitHub release.
3. If SignPath secrets are configured, the release job submits binaries for Authenticode signing before upload.

## After the release

1. Update [`packaging/winget/`](../packaging/winget/) manifests with the new version, MSI SHA256, and release URL.
2. Open a PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) (see [`packaging/winget/README.md`](../packaging/winget/README.md)).
3. Optionally update the Scoop manifest in [`packaging/scoop/`](../packaging/scoop/).
4. Write release notes on GitHub — highlight user-visible changes. Mention code signing when SignPath is active.

## SignPath secrets (when approved)

Add these repository secrets in GitHub → Settings → Secrets → Actions:

| Secret | Description |
|--------|-------------|
| `SIGNPATH_API_TOKEN` | API token from SignPath |
| `SIGNPATH_ORGANIZATION_ID` | Organization ID |
| `SIGNPATH_PROJECT_SLUG` | Project slug |
| `SIGNPATH_SIGNING_POLICY_SLUG` | Signing policy slug |

Until all four are set, CI builds unsigned release assets as today.
