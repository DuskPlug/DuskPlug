# winget manifest

Source-of-truth manifests for submitting DuskPlug to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs).

## Submit or update

1. Fork `microsoft/winget-pkgs`.
2. Copy the three YAML files into:
   ```
   manifests/d/DuskPlug/DuskPlug/<version>/
   ```
3. On each release, update:
   - `PackageVersion` in all three files
   - `InstallerUrl` and `InstallerSha256` in the installer manifest
   - `ProductCode` if the MSI product code changes (rebuild after changing UpgradeCode)
4. Open a PR using the winget template. Validation runs automatically.

## Verify locally

```powershell
winget validate --manifest manifests/d/DuskPlug/DuskPlug/1.0.1
```

After merge, users can install with:

```powershell
winget install DuskPlug.DuskPlug
```

## Updates

winget installs use the same MSI as the direct installer. Users should update with:

```powershell
winget upgrade DuskPlug.DuskPlug
```

When DuskPlug detects a winget/Scoop install, the tray menu shows package-manager instructions instead of downloading updates in-app.
