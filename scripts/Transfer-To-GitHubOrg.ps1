# Transfer DuskPlug repo from MrChriZ to the DuskPlug GitHub organization.
# Prerequisites:
#   1. Create org at https://github.com/organizations/plan (name: DuskPlug, Free plan)
#   2. gh auth refresh -h github.com -s admin:org
#   3. Run from repo root: .\scripts\Transfer-To-GitHubOrg.ps1

$ErrorActionPreference = 'Stop'
$org = 'DuskPlug'
$repo = 'DuskPlug'
$source = "MrChriZ/$repo"

Write-Host "Checking for organization $org..."
try {
    gh api "orgs/$org" | Out-Null
} catch {
    Write-Error @"
Organization '$org' not found. Create it first:
  https://github.com/organizations/plan?plan=free
Use organization name: $org
"@
}

Write-Host "Transferring $source -> $org/$repo ..."
gh api "repos/$source/transfer" -f "new_owner=$org"

Write-Host "Updating git remote..."
git remote set-url origin "https://github.com/$org/$repo.git"
git remote -v

Write-Host "Done. Push with: git push origin master"
Write-Host "Old URLs redirect automatically from https://github.com/$source"
