# RestockerApp

A standalone C++ service that detects when **Pokémon Trading Card Game** products go live and when out-of-stock items are restocked at Australian retailers — currently **Kmart** and **BigW** — then fires notification webhooks (Discord and/or a generic HTTP POST).

Each retailer runs its own discovery + inventory loops on their own threads, sharing one SQLite database, the notifier set, and the restock-decision engine. Products are keyed by `(distributor, product_id)` (`distributor`: 1 = Kmart, 2 = BigW), so the two catalogues coexist in the same tables. Alerts carry the retailer name. The **Kmart** pipeline is described below; the **BigW** pipeline ([BigW section](#bigw)) mirrors it with retailer-specific clients.

The Kmart pipeline runs a **two-tier loop**:

1. **Discovery loop** (fast, ~30s) — sweeps the Kmart **product sitemap**
   (`sitemap-index.xml` → the `product-sitemap-*.xml` files), keeps URLs containing
   `/product/pokemon-trading-card-game:`, and derives each product's **keycode** (the trailing
   digits of the URL, equal to Constructor's `variation_id`) plus a name from the URL slug. New
   keycodes are inserted into `products` and immediately **cross-referenced against the
   Constructor.io browse endpoint** (`ac.cnstrc.com/browse/group_id/<id>`) to fill pre-order,
   `FulfilmentChannel`, regional `stateOOS` (→ *tracked*), price, image, and the clean display
   name. Every `intervals.browse_refresh_seconds` (default 5 min) the whole browse group is
   re-swept to refresh **all** existing rows. A product is only *tracked* (polled) when its
   `stateOOS` map does **not** flag `kmart.target_state` (default `QLD`) as out-of-stock.
   The sitemap sweep is cheap because the large product sitemaps are **HEAD-diffed** (ETag /
   Last-Modified) and only re-downloaded when they actually change.
2. **Inventory loop** (event-driven, ~5 min safety net) — **woken immediately** whenever
   discovery cross-references the browse endpoint (a new product, or the periodic refresh), so a
   restock is caught ASAP rather than on a fixed timer. It batches the tracked, currently
   out-of-stock keycodes into a single `getProductAvailability` call against the Kmart GraphQL
   gateway (`api.kmart.com.au/gateway/graphql`) at **postcode 4221**, and compares each returned
   `available` integer to the previous DB state. When any channel increases it fires **one
   consolidated alert per product** — enriched with the product image, price, Home Delivery /
   Click & Collect counts, the pre-order release date (for not-yet-purchasable items), and a
   nearby-store breakdown (store name · **units in stock** · phone) built from the per-store
   `getProductAvailability` numbers, with names/phone enriched from a follow-up `getFindInStore`
   call. **Stores with no stock are omitted.** Notifications respect `FulfilmentChannel`:
   channels 3/5 (and unknown) alert on the online restock signal; channel 2 (in-store only)
   alerts only when a nearby store actually holds stock.

## BigW

Enabled by the `bigw` config block (`"enabled": true`). It mirrors the Kmart two-tier design with
BigW-specific clients:

1. **Discovery loop** — paginates the BigW **search API**
   (`POST api.bigw.com.au/search/v1/search`, `text: "pokemon tcg"`), keeping only results whose
   `information.specifications` carry the configured `EssentialSafety: "For ages 6+"` entry (the
   genuine-TCG gate). Each result maps to a product (`product_id = identifiers.articleId`, name,
   brand, `prices.<STATE>.price.cents/100`, pre-order, and a `fulfilment_channel` of 2/3/5 from the
   pickup/delivery flags). The search has no product URL, so it is resolved from the BigW
   **sitemaps** (`sitemap.xml` → `product-en-aud-*.xml`), indexed by the trailing `/p/<id>` and
   HEAD-diff cached.
2. **Inventory loop** — polls availability **one product at a time**
   (`GET api.bigw.com.au/api/availability/v0/product/{id}?storeId=…`). It relies on the per-channel
   `available` **boolean** (BigW's quantity is unreliable), mapping `instore → IN_STORE`,
   `pickup → CLICK_AND_COLLECT`, and any delivery method → `HOME_DELIVERY`, stored as 0/1. A product
   becoming available in any channel fires an alert; if every channel goes unavailable and one later
   returns, it alerts again. Store names for the alert come from the cached
   `GET api.bigw.com.au/api/stores/v0/list`. Because the search only returns in-stock items, the
   loop polls **all** tracked BigW products so it can observe them going out of stock.

Both the availability and stores endpoints sit behind Akamai, so BigW uses the same cookie-replay +
browser-harvest transport pattern as Kmart, pointed at `bigw.com.au`.

The `--distributor <all|kmart|bigw>` flag restricts a run to one retailer.

## Build (CMake + vcpkg)

Requires CMake ≥ 3.20, a C++17 compiler, Ninja, and [vcpkg](https://github.com/microsoft/vcpkg).
Dependencies (`nlohmann-json`, `sqlitecpp`, `spdlog`) are declared in `vcpkg.json` and installed
automatically in manifest mode. The HTTP transport uses **curl-impersonate** (see
[TLS impersonation](#tls-impersonation-akamai-bot-manager) below), fetched by CMake at
configure time — so the first configure needs network access.

```sh
# point CMake at your vcpkg toolchain
export VCPKG_ROOT=/path/to/vcpkg          # Windows (PowerShell): $env:VCPKG_ROOT="C:\path\to\vcpkg"

cmake --preset vcpkg
cmake --build --preset vcpkg
# binary: build/RestockerApp (build/RestockerApp.exe on Windows)
```

If you don't use `VCPKG_ROOT`, pass the toolchain explicitly:

```sh
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

## Configure

```sh
cp config.example.json config.json
# edit config.json — set notifier webhook URLs and enable them
```

Key fields:
- `constructor.url_prefix_filter` — substring the discovery loop matches in sitemap product URLs
  and in browse results (default `/product/pokemon-trading-card-game:`).
- `constructor.browse_group_id` — Constructor.io group id of the Pokémon TCG category that the
  browse cross-reference reads status from (default the live TCG group).
- `constructor.sitemap_index_url` / `product_sitemap_filter` — the sitemap index to sweep and the
  substring identifying its product sitemaps (default `product-sitemap`).
- `constructor.sitemap_max_concurrency` — max parallel sitemap fetches (default `6`).
- `kmart.postcode` — `4221` (Gold Coast). The gateway returns the nearest Click & Collect
  stores automatically; no store IDs are hardcoded. Also used for the per-store `getFindInStore`
  lookup that enriches alerts.
- `kmart.target_state` — state code (default `QLD`, matching postcode `4221`) whose regional
  availability gates the polling tier. Products flagged out-of-stock for this state in the
  Constructor `stateOOS` map are stored but never polled. Set to `""` to disable the gate.
- `kmart.instore_max` — max nearby stores listed in an alert (default `8`); also caps how many
  nearby stores the availability query requests.
- `notifiers.discord` / `notifiers.generic` — enable and set URLs (see
  [Set up Discord alerts](#set-up-discord-alerts-private-channel) below). BigW shares these
  webhooks and the same fulfilment-channel routing; the alert names the retailer.
- `intervals.*` — `sitemap_seconds` (fast discovery poll, default 30), `browse_refresh_seconds`
  (full browse-group status refresh, default 300), `inventory_seconds` (inventory idle cadence,
  default 300; usually preempted by the discovery wake), plus per-loop jitter.
- `bigw` — the BigW pipeline (see [BigW](#bigw)). `enabled` (default `false`); `store_id` (`0284`),
  `state` (`QLD`), `zone`, `delivery_postcode`/`delivery_suburb` for the availability lookup;
  `required_spec_name`/`required_spec_value` (the `EssentialSafety` / `For ages 6+` gate);
  `search_seconds`/`availability_seconds`/`stores_refresh_seconds`/`per_product_delay_ms` cadence;
  and `cookie`/`user_agent`/`transport` for the Akamai-fronted endpoints.

## Set up Discord alerts (private channel)

Alerts arrive as a rich embed (product image, stock counts, nearby-store list, and pre-order
date). To keep them private to **just you**, send them to a webhook in a server/channel only you
can see:

1. **Create a private space.** In Discord, click **＋ → Create My Own** to make a personal server
   (you're the only member), or add a channel and restrict its permissions to yourself.
2. **Add a webhook.** Open **Channel Settings → Integrations → Webhooks → New Webhook**. Name it
   `Kmart Restocker`, confirm the target channel, then **Copy Webhook URL**.
3. **Configure the app.** In `config.json`, enable Discord and paste the URL:
   ```json
   "notifiers": {
     "discord": { "enabled": true, "webhook_url": "https://discord.com/api/webhooks/XXXX/YYYY" }
   }
   ```
4. **Test it.** Run `./build/RestockerApp --test-notify` — a sample embed (image + store list +
   pre-order banner) should appear in your channel within a second.

> **Why a channel and not a true DM?** A Discord *webhook* can only post into a channel, never a
> user's direct messages. A private one-member server is the simplest way to keep alerts to
> yourself. A genuine DM would require running a **Discord bot** (a bot application + token, your
> numeric user ID, and the bot sharing a server with you) — more setup for the same result, so
> this build uses the private-channel webhook.

## Run

```sh
./build/RestockerApp                      # run both loops forever (Ctrl-C to stop)
./build/RestockerApp --once               # one discovery + one inventory pass, then exit
./build/RestockerApp --discovery-only     # only the discovery loop
./build/RestockerApp --inventory-only     # only the inventory loop
./build/RestockerApp --dry-run            # detect restocks but never POST to notifiers
./build/RestockerApp --test-notify        # send a synthetic restock to all notifiers, exit
./build/RestockerApp --config other.json  # use a different config file
```

## Verify end-to-end

1. **Notifiers** — set a real Discord/generic URL, enable it, run `--test-notify`,
   confirm the rich embed (image + nearby stores + pre-order banner) lands.
2. **Discovery** — `--discovery-only --once` (the first run downloads all product sitemaps once),
   then inspect the DB:
   ```sh
   sqlite3 restocker.db "SELECT count(*) FROM products;"               # all sitemap TCG products
   sqlite3 restocker.db "SELECT count(*) FROM products WHERE brand IS NOT NULL;"  # browse-enriched
   sqlite3 restocker.db "SELECT distributor,product_id,name,is_preorder,fulfilment_channel,tracked FROM products LIMIT 10;"
   ```
   Sitemap-only products (not in the browse group) keep a URL-derived name and defaults
   (`tracked=1`); browse-enriched rows get the clean Constructor name + pre-order/fulfilment/region.
3. **Inventory** — `--inventory-only --once`; a known OOS keycode (e.g. `43519781`) writes
   `available=0` rows into `inventory_state`.
4. **Restock simulation** — drop a stored value and re-run:
   ```sh
   sqlite3 restocker.db "UPDATE inventory_state SET available=0 WHERE keycode='<kc>';"
   ./build/RestockerApp --inventory-only --once --dry-run   # preview what would fire
   ./build/RestockerApp --inventory-only --once             # fire for real
   sqlite3 restocker.db "SELECT * FROM alerts ORDER BY id DESC LIMIT 5;"
   ```
5. **Loop/shutdown** — run normally, watch both loops tick in the logs, then `Ctrl-C`
   for a clean shutdown.

## Schema

- `products(product_id, distributor, name, url, brand, image_url, price, is_preorder, preorder_release_date, tracked, fulfilment_channel, first_seen, last_seen)` — PK `(distributor, product_id)`; `distributor` 1 = Kmart, 2 = BigW; `tracked=0` when out of stock in `kmart.target_state`; `fulfilment_channel` 2/3/5 (0 = unknown).
- `inventory_state(distributor, keycode, channel, location_id, available, updated_at)` — PK `(distributor, keycode, channel, location_id)`; `location_id=''` for national Home Delivery / CnC total.
- `alerts(id, distributor, keycode, channel, location_id, prev_available, new_available, fired_at)`
- `app_meta(k, v)` — `schema_version`, persistent anon Constructor session id + sequence counter, and per-retailer harvested cookie/UA.

See [Database.md](Database.md) for the full schema, the multi-distributor migration, and the access map.

## Notes

- The Constructor browse API is rate-limited (`x-ratelimit-limit: 201`); discovery reads the
  remaining-quota header and backs off. The browse group is only swept on a new product or every
  `browse_refresh_seconds`, so its footprint is tiny.
- The Kmart product sitemaps are large (~4.5 MB × 13) and send `Cache-Control: no-store`, so
  conditional GET (`If-None-Match`/`If-Modified-Since`) is **not** honoured. The discovery loop
  instead issues a cheap **HEAD** per sitemap each cycle and only full-GETs the ones whose
  ETag/Last-Modified changed — steady state is ~14 bodyless requests every 30s. (The in-memory
  validator cache is per-process, so the first sweep after a restart downloads all sitemaps once.)
- The inventory loop's single batched GraphQL call keeps its footprint small.
- Realistic browser headers (incl. `sec-ch-ua` / `sec-fetch-*`) and a reusable anon session
  id are sent.

### Defeating Akamai Bot Manager (inventory tier)

The inventory-tier GraphQL gateway (`api.kmart.com.au/gateway/graphql`) is fronted by
**Akamai Bot Manager**, which validates the request against an **Akamai cookie set**
(`_abck`, `bm_sz`, `bm_s`, `bm_sv`, `ak_bmsc`, …) the sensor JS solves in a browser. Once you
have a valid cookie jar, a **plain HTTP POST passes (200)** — the bot challenge is in the
cookies, not (only) the TLS fingerprint. So the default transport replays a captured cookie
jar over the curl-impersonate HTTP client; the browser is used only to (re)harvest cookies.

**Default `kmart.transport="http"` (cookie replay).** Every request is a **plain HTTP POST** (no
curl-impersonation) carrying `Cookie: <jar>`, a `User-Agent` matching the client that produced the
jar, and — only if set — `Authorization: Bearer <kmart.auth_token>`, plus `accept-language: en-AU`,
`priority: u=3, i`, `origin`/`referer`, `sec-fetch-*`. **No `sec-ch-ua*` headers.** This is the exact
shape of the working `curl`/Bruno call. The cookie + UA (+ optional token) must come as a **matched
set** — Akamai validates the cookies against the client.

The credential set is swappable and **self-healing**:

- **Seed** from `kmart.cookie` + `kmart.user_agent` (+ optional `kmart.auth_token`) — e.g. a captured
  mobile-app request — or from the last harvested set persisted in the DB.
- **Re-harvest** after `kmart.harvest_after_failures` consecutive failures: a real browser navigates
  `kmart.com.au`, harvests a fresh cookie jar via `Network.getAllCookies` **plus its own
  User-Agent**, and the transport adopts that pair (no token — **cookies alone suffice**), persists
  it to `app_meta` (`kmart_cookie` + `kmart_user_agent`, so it survives restarts), and retries. A
  short harvest→replay loop tolerates Akamai needing a beat to validate a freshly issued `_abck`. No
  login is required — a plain browser session receives the Akamai cookies.

**Cookie harvest / `kmart.transport="browser"` fallback** — both use a real headless-or-headful
browser via the Chrome DevTools Protocol (CDP):

1. Launch Chrome/Edge with remote debugging (`--remote-allow-origins=* --remote-debugging-port=0`).
2. Connect to its CDP WebSocket (driven by the libcurl we already link — `ws`/`wss` support).
3. Navigate to `kmart.com.au` so Akamai sets its cookies and the page has the right origin.
4. Either read the cookie jar back (`Network.getAllCookies`, http transport's harvester) or, in
   `"browser"` mode, run the GraphQL query **in the page context** as a
   `fetch(..., {credentials:'include'})` so it carries the genuine browser cookies and returns **200**.

Two details that make the browser path reliable:
- **Headful, not headless.** Akamai serves *"Access Denied"* to `--headless` Chrome, so the
  browser runs as a real (visible) window by default (`browser.headless=false`). On a headless
  Linux host, run under a virtual display, e.g. `xvfb-run ./RestockerApp`.
- **GET, not POST.** In `"browser"` mode the query is sent as a GraphQL **GET** (params in the
  query string), a CORS-"simple" request — so it triggers **no preflight `OPTIONS`**, which Akamai
  intermittently blocks. `CdpClient` converts the POST payload to a GET internally.

Implemented in [`src/KmartHttpTransport.cpp`](src/KmartHttpTransport.cpp) (cookie replay) and
[`src/CdpClient.cpp`](src/CdpClient.cpp) (browser harvest/fallback). The browser is auto-detected
(Chrome, then Edge; Linux searches `PATH` for `google-chrome`/`chromium`). No extra dependency —
the CDP WebSocket reuses libcurl, and process spawning uses OS APIs.

Config:
- `kmart.transport` — `"http"` (default, cookie replay with browser re-harvest) or `"browser"`
  (route every call through CDP).
- `kmart.cookie` — a captured Akamai cookie jar (`"name=value; name=value; ..."`) seeding the
  transport. Leave empty to harvest one via the browser on first use. `config.json` is gitignored, so
  the secret stays local.
- `kmart.auth_token` — optional bearer token; sent as `Authorization: Bearer <token>` when set
  (cookies alone generally suffice, and harvested credentials carry no token).
- `kmart.user_agent` — `User-Agent` paired with `kmart.cookie`; must match the client that produced
  the cookie (defaults to the Kmart iPhone app UA). Harvested cookies use the harvest browser's own UA.
- `kmart.harvest_after_failures` — consecutive failures before a browser cookie re-harvest (default
  `3`; lower it to self-heal sooner, raise it to tolerate transient blips before launching a browser).
- `kmart.extra_headers` — optional extra headers on the gateway POST (most captured tracing headers
  are not required).
- `browser.executable_path` — path to Chrome/Edge/Chromium; empty = auto-detect.
- `browser.headless` — `false` by default (headful evades Akamai); `true` only with a stealth setup.
- `browser.nav_url` / `browser.page_settle_ms` — origin to load and how long to let Akamai's
  sensor JS validate cookies before fetching.
- `browser.cdp_timeout_ms` — per-CDP-command timeout.

The **discovery tier** (the Kmart product sitemap **and** the Constructor.io browse endpoint) uses
the curl-impersonate HTTP transport
([`cmake/FetchCurlImpersonate.cmake`](cmake/FetchCurlImpersonate.cmake), lexiforest `v1.5.6`,
auto-downloaded + SHA-256 verified at configure time; `http.impersonate*` config) — neither is
behind Bot Manager, so both work directly (HEAD and GET).

- This tool is for personal stock monitoring of a public storefront. Respect Kmart's terms
  and keep poll intervals reasonable.
