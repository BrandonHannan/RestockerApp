# Store API Guide — Kmart, Big W & JB Hi-Fi (Pokémon TCG)

A deep reference for the retailer APIs RestockerApp uses to discover Pokémon TCG products,
resolve their URLs, and read availability / pre-order / release information — **including how to
find products before the storefront publishes them**.

Every Kmart and Big W detail here is grounded in this repo's client code (file references
inline). JB Hi-Fi is documented from captured Bruno requests plus the Algolia REST API, and is
**not yet implemented in code** — its claims are labelled accordingly. Anything not confirmed
against code or a live response is explicitly marked **(unverified — confirm against a live
response)** so nothing reads as fact that isn't.

Last verified: **2026-07-05** (Kmart Constructor endpoint checked live; see §2.1).

---

## 1. Overview & mental model

### 1.1 The three-step pattern (same for every store)

1. **Discovery** — find *which product IDs exist* for the Pokémon TCG category.
2. **Resolution** — turn a product ID into a product **URL**.
3. **Availability** — turn a product ID into **stock / pre-order / release** info, optionally per
   location.

Each store exposes IDs under a different name, but they play the same role:

| Store | ID field | Example | Where the ID lives in the URL |
|-------|----------|---------|-------------------------------|
| Kmart | `variation_id` ("keycode") | `43781645` | trailing digits: `.../mega-greninja-ex-premium-collection-43781645/` |
| Big W | `articleId` | `6073200` | segment after `/p/`: `.../p/6073200` |
| JB Hi-Fi | `sku` (+ Shopify `handle`) | `880062` | slug/handle: `/products/{handle}` |

### 1.2 Two front-ends per retailer — and why one leaks the future

Every retailer runs **two** queryable surfaces:

- **A search / catalogue index** — Kmart uses **Constructor.io**, Big W a **first-party search
  API**, JB Hi-Fi uses **Algolia**. These are fast, filterable, and updated in near-real-time by
  the retailer's catalog pipeline as products are onboarded. Crucially, they carry the product
  **URL/slug/handle** *inside each record*, and they hold records for products that are staged
  but not yet purchasable.
- **A storefront + sitemap** — the human-facing website and its `sitemap.xml`. The sitemap is a
  **cached, regenerated-on-a-schedule artifact**: it only lists already-published pages, and it
  **lags** the index.

**The load-bearing consequence:** if you want a product *before* release, query the index, not
the sitemap. The index has the URL and the release date while the sitemap still 404s. (This is
also why RestockerApp's current sitemap-based URL resolution is too slow for pre-release — see
§5.8.)

### 1.3 What actually hides upcoming products

Each surface applies a "this is live / in stock" gate. Removing or sidestepping that gate is the
whole trick, and the gate is **different per store** (full table in §5.0):

| Store | The gate | Where it is applied |
|-------|----------|---------------------|
| Kmart | `stateOOS` (per-state OOS), client-side display of pre-orders | In the **response** / client UI, *not* the request — so nothing is server-side-filtered |
| Big W | `filter.inStock: true` | In the **request** body |
| JB Hi-Fi | `product_published = 1 AND availability.displayProduct = 1` | In the **request** `filters` string |

### 1.4 Placeholders & auth conventions

Values you can safely hard-code (client-side-public) are shown inline. Session-bound secrets are
shown as placeholders you must capture yourself:

- `<AKAMAI_COOKIE>` — the Akamai Bot Manager cookie jar (`_abck`, `bm_sz`, `ak_bmsc`, plus
  `AWSELB*`/`JSESSIONID`/`X-Correlation-ID` for Big W). Session-scoped, expires quickly.
- `<BEARER_TOKEN>` — optional OAuth bearer (Kmart), if cookies alone are insufficient.
- `<SESSION_ID>` — a client-generated UUID (`i=` for Constructor); any UUID works.

**Bot protection:** Kmart's GraphQL gateway (`api.kmart.com.au`) and Big W's `api.bigw.com.au`
sit behind **Akamai Bot Manager** — they require a valid cookie jar *and* a TLS/JA3 fingerprint
that matches a real browser/app (this repo uses curl-impersonate; see
[HttpConfig](../src/Config.h)). By contrast, **Constructor.io, Algolia, and the sitemaps are
open** — no bot cookie needed (Constructor was fetched live for this guide with no secrets).

---

## 2. Kmart

Kmart splits cleanly: **Constructor.io** for discovery (catalogue), **sitemap** as a lagging
cross-check, and a **GraphQL gateway** for availability.

Source: [ConstructorClient.cpp](../src/ConstructorClient.cpp),
[SitemapClient.cpp](../src/SitemapClient.cpp),
[KmartGraphQLClient.cpp](../src/KmartGraphQLClient.cpp),
config `ConstructorConfig`/`KmartConfig` in [Config.h](../src/Config.h).

### 2.1 Constructor.io catalogue — the primary, sitemap-independent source

Two endpoints, same response shape:

```
GET https://ac.cnstrc.com/browse/group_id/{group_id}?<params>
GET https://ac.cnstrc.com/search/{term}?<params>
```

`browse` returns an entire category by `group_id`; `search` runs a text query. For Pokémon TCG,
use **browse** on the TCG group.

**Query parameters**

| Param | Example / default | What it does |
|-------|-------------------|--------------|
| `c` | `ciojs-client-2.77.1` | Constructor client-version tag. Public, stable. |
| `key` | `key_GZTqlLr41FS2p7AY` | Public API key for Kmart's index. Client-side; not a secret. |
| `i` | `<SESSION_ID>` (any UUID) | Client session id (analytics). Any UUID works. |
| `s` | `1` | Request sequence number (analytics). Increment or leave at 1. |
| `page` | `1` | 1-based page number. Combine with `num_results_per_page` to paginate. |
| `num_results_per_page` | `60` | Results per page. Repo default 60; up to a couple hundred works. |
| `sort_by` | `relevance` **or** `numberOfDaysSinceStartDate` | Sort key. **`numberOfDaysSinceStartDate` + `sort_order=ascending` = newest products first** — the key to catching new releases as they are indexed. Repo's `browse_sort_by` default is `relevance`. |
| `sort_order` | `ascending` / `descending` | Direction for `sort_by`. |
| `_dt` | epoch-ms | Cache-buster timestamp. Any current value. |

**The group id**

```
group_id = abfdf5b2d48e682ca75bfe87a0ecba17   # = the Pokémon TCG category
```

To find other categories' group ids, open the category page on kmart.com.au with dev-tools
Network open and read the `group_id` in the outgoing `ac.cnstrc.com/browse/group_id/...`
request, or read `results[].data.group_ids` on any product.

**Response shape**

```
response.total_num_results          # int — size of the category (was 83 on 2026-07-05)
response.results[]                  # array of products
response.results[].value            # product name
response.results[].data.{...}       # the product record (fields below)
```

**`results[].data` fields that matter** (full set observed live 2026-07-05):

| Field | Meaning / use |
|-------|---------------|
| `variation_id` | **The keycode / product id** (e.g. `43781645`). Feeds the GraphQL availability call. |
| `url` | **Canonical product URL path** (e.g. `/product/pokemon-trading-card-game:-mega-greninja-ex-premium-collection-43781645/`). This is the URL — no sitemap needed. |
| `value` (on the result, not `data`) | Product name. |
| `Brand` | Brand string. |
| `image_url` / `altImages` | Imagery. |
| `price` / `prices` | Price (number) / price object. |
| `isPreOrderActive` | Boolean — pre-order currently open. |
| `preOrderReleaseDate` | Date string (e.g. `2026-07-03`) — announced release date. `2000-01-01` is a placeholder/none. |
| `saleEffectiveDateTime` | **Epoch seconds** — when the product becomes purchasable. A forward-looking release timestamp; the single best pre-release signal Kmart exposes. |
| `nationalInventory` | Boolean — whether national inventory is present. |
| `FulfilmentChannel` | Fulfilment policy code (drives notification routing in this repo). |
| `stateOOS` | Object keyed by state (`{ "WA": ..., "NT": ... }`) — the **per-state out-of-stock gate**. If your state is a key, it's OOS there. Informational in the response; not a request filter. |
| `apn` | **Barcode / EAN** — maps to distributor sheets and cross-store matching. |
| `clearance` | Clearance flag. |
| Others | `id, uri, video, badges, arEnabled, FreeShipping, MerchClassName, MerchDepartment, AssortedProducts, primaryCategoryId, FreeShippingMetro, ratings, flatRateBigBulkyMetro, badgesMarketplace, group_ids, Size, Seller, Colour, SecondaryColour, is_default, variant_video, variant_badges` |

Parsing reference: [ConstructorClient.cpp](../src/ConstructorClient.cpp) `parseConstructorResults`.
Note the repo applies a client-side prefix filter `url_prefix_filter = /product/pokemon-trading-card-game:` to drop non-TCG rows.

**Rate limits:** the response carries an `x-ratelimit-remaining` header; the repo backs off when
it drops below `min_ratelimit_remaining` (20).

**Live example** (open endpoint — runnable as-is):

```bash
curl 'https://ac.cnstrc.com/browse/group_id/abfdf5b2d48e682ca75bfe87a0ecba17?c=ciojs-client-2.77.1&key=key_GZTqlLr41FS2p7AY&i=00000000-0000-0000-0000-000000000000&s=1&page=1&num_results_per_page=60&sort_by=numberOfDaysSinceStartDate&sort_order=ascending&_dt=1751700000000'
```

### 2.2 Sitemap — secondary / cross-check only (KNOWN TO LAG)

```
https://www.kmart.com.au/sitemap-index.xml     # index of child sitemaps
  → keep <loc> entries containing "product-sitemap"
  → in each child, keep <loc> containing "/product/pokemon-trading-card-game:"
  → keycode = trailing digits of the URL
```

Cheap polling: HEAD each child sitemap and diff `ETag` / `Last-Modified`; only GET the ones that
changed. Reference: [SitemapClient.cpp](../src/SitemapClient.cpp) (`sweep`, `extractLocUrls`,
`extractKeycodeFromUrl`).

**Do not use the sitemap to catch pre-release products.** It is a cached artifact regenerated on
the retailer's schedule; a new TCG product appears in the Constructor index (§2.1) *before* the
sitemap regenerates. The sitemap is useful only as a completeness cross-check against the index.

### 2.3 GraphQL gateway — availability (per product / per store)

```
POST https://api.kmart.com.au/gateway/graphql
Content-Type: application/json
User-Agent: <must match the client that produced the cookie; default is the Kmart app UA>
Cookie: <AKAMAI_COOKIE>
Authorization: Bearer <BEARER_TOKEN>     # optional; omit if cookies suffice
```

Two operations. Exact query strings live in
[KmartGraphQLClient.cpp](../src/KmartGraphQLClient.cpp) (`kAvailabilityQuery`,
`kFindInStoreQuery`).

**`getProductAvailability`** — request body:

```json
{
  "operationName": "getProductAvailability",
  "variables": {
    "input": {
      "country": "AU",
      "postcode": "4221",
      "products": [
        { "keycode": "43781645", "quantity": 1, "isNationalInventory": true, "isClickAndCollectOnly": false }
      ],
      "fulfilmentMethods": ["HOME_DELIVERY", "CLICK_AND_COLLECT", "IN_STORE"],
      "amendNearestInStockCnc": true,
      "limit": 8
    }
  },
  "query": "query getProductAvailability($input: ProductAvailabilityQueryInput!) { ... }"
}
```

**Input parameters and what each does**

| Field | What changing it does |
|-------|-----------------------|
| `products[].keycode` | **Selects the product.** This is the `variation_id` from Constructor / trailing digits of the URL. Batch many in one call (repo batches up to `batch_size = 30`). |
| `products[].quantity` | Quantity you're checking availability for (default 1). |
| `products[].isNationalInventory` | `true` = include national HOME_DELIVERY pool. |
| `products[].isClickAndCollectOnly` | `false` = don't restrict to C&C. |
| `postcode` | **The location anchor** — determines which nearby stores are returned for C&C / IN_STORE. |
| `country` | `AU`. |
| `fulfilmentMethods` | Which channels to return: `HOME_DELIVERY`, `CLICK_AND_COLLECT`, `IN_STORE`. |
| `amendNearestInStockCnc` | `true` = substitute the nearest in-stock C&C store. |
| `limit` | **How many nearby stores** to return per product (repo `instore_max`, default 8). |

**Response mapping** (see `parseAvailability`):

- `data.getProductAvailability.availability.HOME_DELIVERY[].stock.available` — national HD count.
- `...CLICK_AND_COLLECT[].stock.totalAvailable` — C&C national total; `...locations[].fulfilment.{locationId, stock.available}` — per-store C&C.
- `...IN_STORE[].locations[].{location.locationId, fulfilment.stock.available}` — per-store in-store.

**`getFindInStore`** — follow-up enrichment for a single keycode; returns
`findInStores[].inventory[].{locationName, locationId, stockLevel, phoneNumber}`. Request input:
`{ postcode, country, keycodes: ["<keycode>"] }`.

**Pre-release note:** because availability is keyed by `keycode`, this endpoint answers for a
product **even if it is not in the sitemap or the browse listing yet** — so a guessed/known
keycode can be probed directly (see §5.4).

**Auth / Akamai:** a stale or missing cookie jar returns **HTTP 403**. Capture a fresh
`<AKAMAI_COOKIE>` from the Kmart mobile app or a real browser session (this repo can re-harvest
via a headless-ish browser after `harvest_after_failures` consecutive 403s — see
[Config.h](../src/Config.h) `KmartConfig`). The `User-Agent` **must match** the client that
produced the cookie.

---

## 3. Big W

Big W is entirely first-party: a **search API** (discovery), a **product availability API**, a
**stores API**, and a lagging **sitemap** used only for ID→URL.

Source: [BigWSearchClient.cpp](../src/BigWSearchClient.cpp),
[BigWAvailabilityClient.cpp](../src/BigWAvailabilityClient.cpp),
[BigWStoresClient.cpp](../src/BigWStoresClient.cpp),
[BigWSitemapResolver.cpp](../src/BigWSitemapResolver.cpp),
config `BigWConfig` in [Config.h](../src/Config.h).

### 3.1 Search — the primary, sitemap-independent source

```
POST https://api.bigw.com.au/search/v1/search
Content-Type: application/json
User-Agent: BigwApp/4.45.4 (ios - 26.5)
Cookie: <AKAMAI_COOKIE>
```

**Request body**

```json
{
  "text": "pokemon tcg",
  "sort": "relevance",
  "filter": { "inStock": true, "soldBy": ["BIG W"] },
  "storeId": "0284",
  "state": "QLD",
  "zone": "DDBURLEIGHHEADS",
  "page": 0,
  "perPage": 48,
  "format": "1",
  "clientId": "mobile"
}
```

**Parameters and what each does**

| Param | What changing it does |
|-------|-----------------------|
| `text` | The search query (`"pokemon tcg"`). |
| `sort` | Result ordering (`relevance`). Other sorts (e.g. newest) exist on the site — inspect the app to capture the exact tokens *(unverified)*. |
| `filter.inStock` | **`true` hides not-yet-purchasable products; `false` (or omitted) reveals them.** This is Big W's pre-release unlock (§5.0). |
| `filter.soldBy` | Scope to sellers, e.g. `["BIG W"]` for first-party (excludes marketplace). |
| `storeId` | Store context for pickup/fulfilment (`0284`). Choose from the stores API (§3.3). |
| `state` | **Pricing/fulfilment state** — also selects which `prices.<STATE>` block is populated. |
| `zone` | Delivery zone (`DDBURLEIGHHEADS`) — scopes fulfilment. |
| `page` | 0-based page index. |
| `perPage` | Results per page (repo default 48; `max_pages` 10). |
| `format` | Response format flag (`"1"`). |
| `clientId` | Client tag (`mobile`). |

**Response** (`organic.results[]`, see `parseBigWSearch`):

| Field | Meaning |
|-------|---------|
| `identifiers.articleId` | **The product id** (feeds availability + `/p/{id}` URL). |
| `information.name` | Product name. |
| `information.brand.name` | Brand. |
| `information.specifications[]` | Spec list. The repo uses `EssentialSafety = "For ages 6+"` as the **genuine-TCG filter** to drop accessories. |
| `prices.<STATE>.price.cents` | Price in cents (÷100). Keyed by state. |
| `fulfilment.delivery.{standard,express,priority}` / `fulfilment.collection.pickup` | Fulfilment policy → channel code (2 pickup, 5 delivery, 3 both). |
| `attributes.lifecycleStatus` | Pre-order signal: `PR` / `PREORDER` *(unverified — confirm against a live pre-order article)*. |

**URL without the sitemap:** the repo currently leaves `url` empty and resolves it from the
sitemap (§3.4), which lags. To get the URL from the index instead, dump the **full** search
response and locate the URL/slug field it already contains — likely candidates: `url`,
`seoToken`, `slug`, or `information.seo`. Failing that, construct
`https://www.bigw.com.au/product/{slugified-name}/p/{articleId}` and confirm `/p/{articleId}`
resolves regardless of the slug. **(unverified — confirm with one live Akamai-authenticated
call.)**

### 3.2 Availability — per product, per location

```
GET https://api.bigw.com.au/api/availability/v0/product/{articleId}?storeId={storeId}&deliveryPostcode={postcode}&deliverySuburb={suburb}
Cookie: <AKAMAI_COOKIE>
User-Agent: BigwApp/4.45.4 (ios - 26.5)
```

Example: `.../product/6073200?storeId=0284&deliveryPostcode=4221&deliverySuburb=ELANORA`

| Param | What changing it does |
|-------|-----------------------|
| `{articleId}` (path) | **Selects the product.** |
| `storeId` | The store checked for `instore` + `pickup` availability. |
| `deliveryPostcode` | Postcode for the delivery quote. |
| `deliverySuburb` | Suburb for the delivery quote. |

**Response** (`parseBigWAvailability`): `products.{articleId}.{instore,pickup,delivery}`. Each
channel is read as a **boolean** `available` (the quantity field is documented as unreliable):

- `instore[storeId].available` → IN_STORE at that store.
- `pickup[storeId].available` → CLICK_AND_COLLECT.
- `delivery.{method}.available` (any method) → HOME_DELIVERY.

**Pre-release note:** like Kmart, this is keyed by `articleId`, so it answers for products not
yet listed — probe a known/guessed id directly (§5.4).

### 3.3 Stores list

```
GET https://api.bigw.com.au/api/stores/v0/list
Cookie: <AKAMAI_COOKIE>
```

Response is a top-level map `storeId → { id, name, phoneNumber }` (see `parseBigWStores`). Use it
to pick a `storeId`/`zone` and to enrich alerts with store name/phone. Cached ~daily
(`stores_refresh_seconds = 86400`).

### 3.4 Sitemap — secondary / cross-check only (KNOWN TO LAG)

```
https://www.bigw.com.au/sitemap.xml
  → child sitemaps containing "product-en-aud" (product-en-aud-N.xml)
  → product id = the segment after "/p/"
```

Reference: [BigWSitemapResolver.cpp](../src/BigWSitemapResolver.cpp) (`extractBigWProductId`,
`refresh`, `resolve`). Same caveat as Kmart: it trails the search index, so it is **not** the
pre-release path — the search response (§3.1) is.

### 3.5 Auth

Akamai Bot Manager, same pattern as Kmart: `User-Agent: BigwApp/4.45.4 (ios - 26.5)`, a valid
`<AKAMAI_COOKIE>` jar (`_abck`, `bm_sz`, `ak_bmsc`, `AWSELB*`, `JSESSIONID`, `X-Correlation-ID`),
and a browser-matching TLS fingerprint. Stale cookies → 403; re-harvest from the app/browser.

---

## 4. JB Hi-Fi (Algolia)

JB Hi-Fi's storefront is powered by **Algolia**. Everything below is from captured Bruno requests
plus the Algolia REST API; **there is no JB Hi-Fi code in this repo yet**, so treat structural
claims as **(unverified — confirm against a live response)** unless quoted verbatim from the
captures.

### 4.1 Application & keys

```
Host (DSN):     https://vtvkm5urpx-dsn.algolia.net
Application id: VTVKM5URPX
Index (products): shopify_products_families
Index (stores):   shopify_store_locations
```

**Public search API keys** (client-side, safe to include — Algolia search keys are meant to ship
in the browser; they are read-only and scoped):

- Search / multi-query: `1d989f0839a992bbece9099e1b091f07`
- Store locations query: `a0c0108d737ad5ab54a0e2da900bf040`

These are passed either as query params (`x-algolia-api-key`, `x-algolia-application-id`,
`x-algolia-agent`) or as `X-Algolia-*` headers. No Akamai cookie is needed — Algolia is open.

### 4.2 Multi-query search (`queries`)

```
POST https://vtvkm5urpx-dsn.algolia.net/1/indexes/*/queries
     ?x-algolia-agent=...&x-algolia-api-key=1d989f0839a992bbece9099e1b091f07&x-algolia-application-id=VTVKM5URPX
Content-Type: application/json
```

Body is `{ "requests": [ { ... }, ... ] }`; each request targets `indexName:
"shopify_products_families"`. Captured params and what they do:

| Param | What it does |
|-------|--------------|
| `indexName` | Target index (`shopify_products_families`). |
| `query` | Free-text query (`""` for pure filter/browse). |
| `filters` | **The SQL-like filter string** — the main lever (see below). |
| `facets` | Which facets to compute counts for (e.g. `facets.Expansion`, `facets.Sold by`, `banner_tags.label`, `all_calculated_product_tags`, `onPromotion`). |
| `facetFilters` | Restrict to specific facet values (array form). |
| `numericFilters` | Numeric constraints (e.g. `price>0`). |
| `hitsPerPage` | Page size (captured 36). |
| `page` | 0-based page. |
| `distinct` | De-duplicate product families. |
| `maxValuesPerFacet` | Cap facet values returned (captured 300). |
| `highlightPreTag` / `highlightPostTag` | Highlight markers. |
| `clickAnalytics` / `analytics` / `analyticsTags` | Analytics toggles/labels. |
| `userToken` | Anonymous user id for analytics/personalization. |

**Captured category filter** (the Pokémon TCG selector):

```
("facets.Game type": "Trading card games" OR "category_hierarchy":"Trading card games")
  AND "facets.Brands": "Pokemon TCG"
  OR SKU: 780096
  AND (price > 0 AND product_published = 1 AND availability.displayProduct = 1)
```

The captured multi-query also fires promo variants appending
`AND onPromotion:true`, `AND banner_tags.label:Clearance`, and
`AND all_calculated_product_tags:"Red Hot Deal"` — these are how the site builds its
promo/clearance rails.

### 4.3 Browse (full-index dump / lookup by SKU)

```
POST https://vtvkm5urpx-dsn.algolia.net/1/indexes/shopify_products_families/browse
     ?x-algolia-agent=...&x-algolia-api-key=1d989f0839a992bbece9099e1b091f07&x-algolia-application-id=VTVKM5URPX
Content-Type: application/json
```

Captured body:

```json
{
  "hitsPerPage": 20,
  "filters": "(sku:880062 OR skus:880062) AND (price > 0 AND product_published = 1 AND availability.displayProduct = 1)",
  "analytics": true
}
```

- **Changing `sku` selects the product.** `browse` paginates the entire index (via a `cursor`)
  rather than ranked search — ideal for enumeration.
- **Dropping `product_published = 1 AND availability.displayProduct = 1` reveals unpublished /
  upcoming records** — JB Hi-Fi's pre-release unlock (§5.0).

**Response record fields of interest** *(unverified — confirm against a live response)*:
`handle` (Shopify slug → product URL), `sku`/`skus`, `price`, `product_published`,
`availability.displayProduct`, availability/date fields, banner/promo tags.

### 4.4 Store locations

```
POST https://vtvkm5urpx-dsn.algolia.net/1/indexes/shopify_store_locations/query
     ?x-algolia-agent=...&x-algolia-api-key=a0c0108d737ad5ab54a0e2da900bf040&x-algolia-application-id=VTVKM5URPX
Content-Type: application/json
```

Captured body:

```json
{
  "query": "",
  "aroundLatLng": "-28.129853, 153.451456",
  "aroundRadius": 200000,
  "hitsPerPage": 5,
  "filters": "displayOnWeb:p"
}
```

| Param | What it does |
|-------|--------------|
| `aroundLatLng` | Geo centre for the search. |
| `aroundRadius` | Radius in metres (200 km here). |
| `hitsPerPage` | Number of stores to return. |
| `filters: displayOnWeb:p` | Only web-visible stores. |

### 4.5 URL construction

From a record's Shopify `handle`, the product URL is (typically)
`https://www.jbhifi.com.au/products/{handle}`. **(unverified — confirm the live slug pattern; the
`handle` is present in the index the moment the product is created in Shopify, ahead of any
sitemap.)**

---

## 5. Finding products *before* release (the core methodology)

**Why the sitemap fails here:** the sitemap lags — products discovered pre-release are not yet in
any store's sitemap. Query the **search / catalogue index** instead. The retailer's catalog
pipeline populates it in near-real-time as products are onboarded, *ahead of* sitemap
regeneration, and each record already contains the product URL/slug/handle **and** its
release-timing fields. So the index alone yields **URL + release date + pre-order status** before
the storefront shows anything.

### 5.0 The exact per-store request change (the mechanisms differ)

Each store gates differently — one needs **no** filter change, the other two each need a
specific one:

| Store | Endpoint | Filter change required? | Exact change | How to spot the upcoming item in the response |
|-------|----------|-------------------------|--------------|-----------------------------------------------|
| **Kmart** | Constructor `browse` group | **No.** The request carries no stock/publish filter — the TCG group already returns pre-release products. **Live-verified 2026-07-05.** | None. Add `sort_by=numberOfDaysSinceStartDate&sort_order=ascending` to surface newest first. | `preOrderReleaseDate` / `saleEffectiveDateTime` in the future, and/or `isPreOrderActive:true`. There is **no** "coming soon" boolean. |
| **Big W** | `POST /search/v1/search` | **Yes.** Default `filter.inStock:true` hides unreleased items. | Set `"filter": { "inStock": false, ... }` (or omit `inStock`). *(unverified — confirm with one live POST)* | `attributes.lifecycleStatus` = `PR`/`PREORDER` *(unverified)*; the article appearing only when `inStock:false`. |
| **JB Hi-Fi** | Algolia `browse` / `queries` | **Yes.** `filters` enforces `product_published = 1 AND availability.displayProduct = 1`. | Drop `product_published = 1` (and `availability.displayProduct = 1`) from the `filters` string; optionally facet on pre-order/"coming soon" tags (`banner_tags.label`, `all_calculated_product_tags`). *(unverified — confirm with one live POST)* | Records that appear **only** with the filters removed; `handle` present. |

**Plainly:** Kmart needs no filter — just sort + read the date fields; Big W needs
`inStock:false`; JB Hi-Fi needs the `product_published`/`displayProduct` clause dropped. Same
goal, three different request changes.

### 5.1 Step 1 — poll the index (newest-first / filtered), not the sitemap

- **Kmart:** Constructor browse with `sort_by=numberOfDaysSinceStartDate&sort_order=ascending`;
  take `data.url` directly; watch `saleEffectiveDateTime` / `preOrderReleaseDate` for future
  dates.
- **JB Hi-Fi:** Algolia `queries`/`browse` on `shopify_products_families`, ranked/faceted by
  recency or filtered on pre-order/"coming soon" facet values; take `handle` directly.
- **Big W:** `POST /search/v1/search` with `filter.inStock:false`; take the URL/slug field from
  the full response.

### 5.2 Step 2 — bypass the publish/stock gate (per §5.0)

Big W `inStock:false`; JB Hi-Fi drop the `product_published`/`displayProduct` clause; Kmart read
the record regardless of `stateOOS` and rely on `isPreOrderActive`/`saleEffectiveDateTime`.

### 5.3 Step 3 — ID → URL, all from the index record (no sitemap)

- Kmart: `data.url` (already includes the trailing-digit keycode).
- JB Hi-Fi: `https://www.jbhifi.com.au/products/{handle}` *(confirm slug pattern)*.
- Big W: the search-response slug field, or constructed `/product/{slug}/p/{articleId}` *(confirm)*.

### 5.4 Step 4 — direct-probe by ID for products not in *any* listing yet

The availability endpoints are keyed by ID and answer even for unlisted products:

- Kmart: GraphQL `getProductAvailability` with the `keycode`.
- Big W: `GET /availability/v0/product/{articleId}`.
- JB Hi-Fi: Algolia `browse` filtered by `sku`.

Enumerate sequential / nearby IDs (Kmart keycodes and Big W articleIds are numeric and roughly
sequential within a range) and probe — a valid-but-unlisted ID returns data (name, URL/slug,
dates) before it ever appears in a listing.

### 5.5 Step 5 — predict IDs / slugs from the release calendar

The Pokémon Company announces set names, product line-ups, and release dates weeks ahead. Use
them to:

- Predict slugs and poll the constructed URL for the 404→200 flip.
- Match a distributor barcode/EAN to a store SKU — Kmart exposes `apn` (barcode) on every
  Constructor record, so a barcode from a distributor sheet can be reverse-matched to a keycode.

### 5.6 Step 6 — read release date / pre-order

- Kmart: `saleEffectiveDateTime` (epoch seconds, purchasable-from) + `preOrderReleaseDate` +
  `isPreOrderActive`.
- Big W: `attributes.lifecycleStatus` *(unverified)*.
- JB Hi-Fi: availability / published-date fields on the record *(unverified)*.

### 5.7 Step 7 — detect the release moment

High-frequency polling + diffing of **index membership and the index's own fields** — a new
`variation_id` / `handle` / `articleId` appearing, or `saleEffectiveDateTime` / release date
passing — plus the 404→200 flip on the already-known URL. This is strictly faster than diffing
the sitemap.

### 5.8 Gap note for RestockerApp

The current code resolves product URLs from the **sitemap** ([SitemapClient.cpp](../src/SitemapClient.cpp),
[BigWSitemapResolver.cpp](../src/BigWSitemapResolver.cpp)), which is exactly what introduces the
lag. To match the pre-release capability, discovery should read the URL straight from the index
record (Constructor `data.url` for Kmart; the search-response slug for Big W; the Algolia
`handle` for JB Hi-Fi) and diff index membership rather than the sitemap. (Documented here only;
no code change is part of this guide.)

### 5.9 Courtesy / legitimacy

These are all public endpoints queried without the storefront's own filters — no authentication
is bypassed. Be a good citizen: honour `x-ratelimit-remaining` (Constructor), keep poll
intervals reasonable, add jitter, and don't hammer the Akamai-gated endpoints (they will 403 and
you'll burn your cookie jar).

---

## 6. Quick reference

### 6.1 Endpoint cheat-sheet

| Store | Purpose | Method & endpoint | Key params | Returns |
|-------|---------|-------------------|------------|---------|
| Kmart | Discovery (catalogue) | `GET ac.cnstrc.com/browse/group_id/{group_id}` | `key`, `sort_by`, `page` | products incl. `data.url`, `variation_id`, dates |
| Kmart | ID→URL cross-check | `GET www.kmart.com.au/sitemap-index.xml` | — | product URLs (lagging) |
| Kmart | Availability | `POST api.kmart.com.au/gateway/graphql` | `keycode`, `postcode`, `limit` | HD/C&C/IN_STORE stock |
| Kmart | Store enrichment | `POST api.kmart.com.au/gateway/graphql` (`getFindInStore`) | `keycodes`, `postcode` | store name/level/phone |
| Big W | Discovery | `POST api.bigw.com.au/search/v1/search` | `text`, `filter.inStock`, `state`, `storeId` | `articleId`, name, price, fulfilment |
| Big W | Availability | `GET api.bigw.com.au/api/availability/v0/product/{id}` | `storeId`, `deliveryPostcode`, `deliverySuburb` | instore/pickup/delivery booleans |
| Big W | Stores | `GET api.bigw.com.au/api/stores/v0/list` | — | storeId → {name, phone} |
| Big W | ID→URL cross-check | `GET www.bigw.com.au/sitemap.xml` | — | `/p/{id}` URLs (lagging) |
| JB Hi-Fi | Discovery (search) | `POST vtvkm5urpx-dsn.algolia.net/1/indexes/*/queries` | `filters`, `facets`, `hitsPerPage` | products incl. `handle`, `sku` |
| JB Hi-Fi | Browse / by-SKU | `POST .../1/indexes/shopify_products_families/browse` | `filters` (`sku:...`) | full records, `handle` |
| JB Hi-Fi | Stores | `POST .../1/indexes/shopify_store_locations/query` | `aroundLatLng`, `aroundRadius` | nearby stores |

### 6.2 Runnable examples

Open endpoints (no secrets) — Kmart Constructor, newest-first:

```bash
curl 'https://ac.cnstrc.com/browse/group_id/abfdf5b2d48e682ca75bfe87a0ecba17?c=ciojs-client-2.77.1&key=key_GZTqlLr41FS2p7AY&i=00000000-0000-0000-0000-000000000000&s=1&page=1&num_results_per_page=60&sort_by=numberOfDaysSinceStartDate&sort_order=ascending&_dt=1751700000000'
```

JB Hi-Fi browse — reveal unpublished by dropping the publish clause (public Algolia key):

```bash
curl -X POST 'https://vtvkm5urpx-dsn.algolia.net/1/indexes/shopify_products_families/browse?x-algolia-api-key=1d989f0839a992bbece9099e1b091f07&x-algolia-application-id=VTVKM5URPX' \
  -H 'Content-Type: application/json' \
  --data '{"hitsPerPage":50,"filters":"(\"facets.Brands\": \"Pokemon TCG\") AND price > 0","analytics":true}'
```

Kmart availability (needs a fresh Akamai cookie):

```bash
curl -X POST 'https://api.kmart.com.au/gateway/graphql' \
  -H 'Content-Type: application/json' \
  -H 'User-Agent: Mozilla/5.0 (iPhone; CPU iPhone OS 18_7 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Mobile/15E148 KmartApp/3.4.3' \
  -H 'Cookie: <AKAMAI_COOKIE>' \
  --data '{"operationName":"getProductAvailability","variables":{"input":{"country":"AU","postcode":"4221","products":[{"keycode":"43781645","quantity":1,"isNationalInventory":true,"isClickAndCollectOnly":false}],"fulfilmentMethods":["HOME_DELIVERY","CLICK_AND_COLLECT","IN_STORE"],"amendNearestInStockCnc":true,"limit":8}},"query":"query getProductAvailability($input: ProductAvailabilityQueryInput!) { getProductAvailability(input: $input) { availability { HOME_DELIVERY { keycode stock { available } } CLICK_AND_COLLECT { keycode stock { totalAvailable } locations { fulfilment { locationId stock { available } } } } IN_STORE { keycode locations { location { locationId } fulfilment { stock { available } } } } } } }"}'
```

Big W search — reveal pre-release with `inStock:false` (needs a fresh Akamai cookie):

```bash
curl -X POST 'https://api.bigw.com.au/search/v1/search' \
  -H 'Content-Type: application/json' \
  -H 'User-Agent: BigwApp/4.45.4 (ios - 26.5)' \
  -H 'Cookie: <AKAMAI_COOKIE>' \
  --data '{"text":"pokemon tcg","sort":"relevance","filter":{"inStock":false,"soldBy":["BIG W"]},"storeId":"0284","state":"QLD","zone":"DDBURLEIGHHEADS","page":0,"perPage":48,"format":"1","clientId":"mobile"}'
```

---

## 7. Field glossary

| Term | Store | Meaning |
|------|-------|---------|
| keycode / `variation_id` | Kmart | Numeric product id; trailing digits of the URL. |
| `articleId` | Big W | Numeric product id; segment after `/p/`. |
| `sku` / `handle` | JB Hi-Fi | Numeric SKU / Shopify slug. |
| `saleEffectiveDateTime` | Kmart | Epoch seconds; when the product becomes purchasable. |
| `preOrderReleaseDate` / `isPreOrderActive` | Kmart | Announced release date / pre-order open flag. |
| `stateOOS` | Kmart | Per-state out-of-stock map (response-side gate). |
| `lifecycleStatus` | Big W | Product lifecycle; `PR`/`PREORDER` = pre-order *(unverified)*. |
| `filter.inStock` | Big W | Request gate; `false` reveals pre-release. |
| `product_published` / `availability.displayProduct` | JB Hi-Fi | Request-side publish gate; drop to reveal pre-release. |
| Akamai cookie | Kmart / Big W | `_abck`/`bm_sz`/`ak_bmsc` (+ AWS/session) bot-manager jar. |

> **Verification status:** Kmart Constructor fields, URL-in-record, newest-first sort, and the
> "no filter needed" finding were confirmed against the live open endpoint on 2026-07-05. Kmart
> GraphQL and all Big W details are grounded in this repo's client code. All JB Hi-Fi structural
> details and the Big W pre-order/URL-field specifics are marked **(unverified)** and should be
> confirmed with one live request before you rely on them.
