# Google Search Console setup

One-time steps to get the DuskPlug GitHub Pages site indexed by Google.

**Site URL:** https://duskplug.github.io/DuskPlug/

## Prerequisites

1. GitHub Pages deployed (see [`.github/workflows/pages.yml`](../.github/workflows/pages.yml) — push to `master` with `docs/site/` changes).
2. Visit the site URL in a browser and confirm it loads.

## 1. Add property

1. Open [Google Search Console](https://search.google.com/search-console).
2. Click **Add property** → **URL prefix**.
3. Enter: `https://duskplug.github.io/DuskPlug/`

## 2. Verify ownership (HTML tag method)

1. Search Console shows a meta tag like:
   ```html
   <meta name="google-site-verification" content="YOUR_TOKEN_HERE">
   ```
2. Verification meta tag is set in [`docs/site/index.html`](site/index.html) and [`docs/site/getting-started.html`](site/getting-started.html).
3. After any change, push and wait for the Pages workflow to finish.
4. Click **Verify** in Search Console.

## 3. Submit sitemap

1. In Search Console, open **Sitemaps**.
2. Submit: `https://duskplug.github.io/DuskPlug/sitemap.xml`

## 4. Request indexing

1. Use **URL Inspection** for:
   - `https://duskplug.github.io/DuskPlug/`
   - `https://duskplug.github.io/DuskPlug/getting-started.html`
2. Click **Request indexing** for each.

## 5. Monitor

- Check **Performance** after 2–6 weeks for queries like `duskplug`, `tuya windows tray`, `smart plug sunset`.
- Community posts (HN, Reddit, blog) in [`ANNOUNCEMENTS.md`](ANNOUNCEMENTS.md) accelerate indexing via backlinks.

## Notes

- GitHub Pages for org repos uses `https://<org>.github.io/<repo>/`.
- Do not use a custom domain unless you add DNS verification in Search Console.
