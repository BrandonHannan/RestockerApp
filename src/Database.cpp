#include "Database.h"

#include <chrono>
#include <cstdlib>
#include <random>

#include <SQLiteCpp/SQLiteCpp.h>

namespace restocker {
namespace {

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// RFC-4122-ish v4 UUID. Good enough for an anonymous client identifier.
std::string makeUuid() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> hex(0, 15);
    std::uniform_int_distribution<int> var(8, 11);
    const char* digits = "0123456789abcdef";
    std::string u(36, '-');
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        if (i == 14) {
            u[i] = '4';
        } else if (i == 19) {
            u[i] = digits[var(rng)];
        } else {
            u[i] = digits[hex(rng)];
        }
    }
    return u;
}

}  // namespace

Database::Database(const std::string& path) {
    db_ = std::make_unique<SQLite::Database>(
        path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    db_->exec("PRAGMA journal_mode=WAL");
    db_->exec("PRAGMA busy_timeout=5000");
    db_->exec("PRAGMA foreign_keys=ON");
    initSchema();
}

Database::~Database() = default;

namespace {

// Does `table` currently have a column named `column`? Used to decide whether the
// legacy (pre-distributor) schema is still in place.
bool tableHasColumn(SQLite::Database& db, const std::string& table,
                    const std::string& column) {
    SQLite::Statement q(db, "PRAGMA table_info(" + table + ")");
    while (q.executeStep()) {
        if (q.getColumn(1).getString() == column) return true;
    }
    return false;
}

}  // namespace

void Database::initSchema() {
    // app_meta first: it holds the schema_version key the migration is gated on.
    db_->exec("CREATE TABLE IF NOT EXISTS app_meta (k TEXT PRIMARY KEY, v TEXT)");

    // Final (multi-distributor) shapes. On a fresh DB these create the new schema
    // directly; on a legacy DB the old same-named tables already exist so these are
    // no-ops and the rebuild below converts them.
    db_->exec(
        "CREATE TABLE IF NOT EXISTS products ("
        " product_id TEXT NOT NULL, distributor INTEGER NOT NULL DEFAULT 1,"
        " name TEXT, url TEXT, brand TEXT, image_url TEXT, price REAL,"
        " is_preorder INTEGER DEFAULT 0,"
        " preorder_release_date TEXT,"
        " tracked INTEGER DEFAULT 1,"
        " fulfilment_channel INTEGER DEFAULT 0,"
        " first_seen INTEGER, last_seen INTEGER,"
        " PRIMARY KEY (distributor, product_id))");

    // Legacy column migrations for pre-distributor DBs, so the table reaches the
    // full pre-rebuild shape before the rebuild copies it. No-ops once present.
    try {
        db_->exec("ALTER TABLE products ADD COLUMN image_url TEXT");
    } catch (const std::exception&) {
        // column already present
    }
    try {
        db_->exec("ALTER TABLE products ADD COLUMN fulfilment_channel INTEGER DEFAULT 0");
    } catch (const std::exception&) {
        // column already present
    }

    db_->exec(
        "CREATE TABLE IF NOT EXISTS inventory_state ("
        " distributor INTEGER NOT NULL DEFAULT 1,"
        " keycode TEXT, channel TEXT, location_id TEXT DEFAULT '',"
        " available INTEGER, updated_at INTEGER,"
        " PRIMARY KEY (distributor, keycode, channel, location_id))");

    db_->exec(
        "CREATE TABLE IF NOT EXISTS alerts ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " distributor INTEGER NOT NULL DEFAULT 1,"
        " keycode TEXT, channel TEXT, location_id TEXT,"
        " prev_available INTEGER, new_available INTEGER, fired_at INTEGER)");

    migrateToV1();
}

// Rename variation_id -> product_id and add the distributor dimension on an
// existing Kmart database. SQLite cannot alter a primary key in place, so
// products and inventory_state are rebuilt; alerts only needs a new column.
// Gated on schema_version < 1 AND the legacy `products.variation_id` column still
// existing, and committed atomically with the version bump so a crash mid-rebuild
// rolls back to clean legacy state.
void Database::migrateToV1() {
    std::string ver = getMeta("schema_version");
    if (!ver.empty() && std::strtol(ver.c_str(), nullptr, 10) >= 1) {
        return;  // already migrated
    }

    // Fresh DB (new shape created above, no legacy column): just stamp the version.
    if (!tableHasColumn(*db_, "products", "variation_id")) {
        setMeta("schema_version", "1");
        return;
    }

    db_->exec("BEGIN TRANSACTION");
    try {
        // products: rebuild with composite PK, backfilling distributor = Kmart (1).
        db_->exec("ALTER TABLE products RENAME TO products_old");
        db_->exec(
            "CREATE TABLE products ("
            " product_id TEXT NOT NULL, distributor INTEGER NOT NULL DEFAULT 1,"
            " name TEXT, url TEXT, brand TEXT, image_url TEXT, price REAL,"
            " is_preorder INTEGER DEFAULT 0,"
            " preorder_release_date TEXT,"
            " tracked INTEGER DEFAULT 1,"
            " fulfilment_channel INTEGER DEFAULT 0,"
            " first_seen INTEGER, last_seen INTEGER,"
            " PRIMARY KEY (distributor, product_id))");
        db_->exec(
            "INSERT INTO products"
            " (product_id, distributor, name, url, brand, image_url, price,"
            "  is_preorder, preorder_release_date, tracked, fulfilment_channel,"
            "  first_seen, last_seen)"
            " SELECT variation_id, 1, name, url, brand, image_url, price,"
            "  is_preorder, preorder_release_date, tracked, fulfilment_channel,"
            "  first_seen, last_seen FROM products_old");
        db_->exec("DROP TABLE products_old");

        // inventory_state: rebuild with distributor in the PK, backfilling Kmart (1).
        db_->exec("ALTER TABLE inventory_state RENAME TO inventory_state_old");
        db_->exec(
            "CREATE TABLE inventory_state ("
            " distributor INTEGER NOT NULL DEFAULT 1,"
            " keycode TEXT, channel TEXT, location_id TEXT DEFAULT '',"
            " available INTEGER, updated_at INTEGER,"
            " PRIMARY KEY (distributor, keycode, channel, location_id))");
        db_->exec(
            "INSERT INTO inventory_state"
            " (distributor, keycode, channel, location_id, available, updated_at)"
            " SELECT 1, keycode, channel, location_id, available, updated_at"
            " FROM inventory_state_old");
        db_->exec("DROP TABLE inventory_state_old");

        // alerts: surrogate id PK, so a plain column add suffices.
        db_->exec("ALTER TABLE alerts ADD COLUMN distributor INTEGER NOT NULL DEFAULT 1");

        setMeta("schema_version", "1");
        db_->exec("COMMIT");
    } catch (const std::exception&) {
        db_->exec("ROLLBACK");
        throw;
    }
}

bool Database::insertProductIfAbsent(int distributor, const std::string& product_id,
                                     const std::string& url, const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx_);
    const std::int64_t now = nowSeconds();

    // INSERT OR IGNORE: only a genuinely new (distributor, product_id) inserts a
    // row. changes() then reports 1 for an insert, 0 when the row already existed.
    SQLite::Statement ins(
        *db_,
        "INSERT OR IGNORE INTO products (distributor, product_id, name, url, first_seen, last_seen)"
        " VALUES (?, ?, ?, ?, ?, ?)");
    ins.bind(1, distributor);
    ins.bind(2, product_id);
    ins.bind(3, name);
    ins.bind(4, url);
    ins.bind(5, now);
    ins.bind(6, now);
    ins.exec();
    return db_->getChanges() > 0;
}

void Database::updateProductStatus(const Product& p) {
    std::lock_guard<std::mutex> lock(mtx_);
    const std::int64_t now = nowSeconds();

    // Enrichment-only update. Leaves `url` (canonical sitemap URL) untouched, and
    // keeps the existing name when the browse value is empty (COALESCE on NULLIF).
    SQLite::Statement upd(
        *db_,
        "UPDATE products SET"
        " name = COALESCE(NULLIF(?, ''), name),"
        " brand=?, image_url=?, price=?, is_preorder=?, preorder_release_date=?,"
        " tracked=?, fulfilment_channel=?, last_seen=?"
        " WHERE distributor=? AND product_id=?");
    upd.bind(1, p.name);
    upd.bind(2, p.brand);
    upd.bind(3, p.image_url);
    upd.bind(4, p.price);
    upd.bind(5, p.is_preorder ? 1 : 0);
    upd.bind(6, p.preorder_release_date);
    upd.bind(7, p.tracked ? 1 : 0);
    upd.bind(8, p.fulfilment_channel);
    upd.bind(9, now);
    upd.bind(10, p.distributor);
    upd.bind(11, p.product_id);
    upd.exec();
}

std::vector<std::string> Database::getTrackedOOSKeycodes(int distributor) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> out;
    // A product qualifies when it belongs to this distributor, is tracked, and the
    // best available stock we have on record across every channel/location is 0
    // (or unknown).
    SQLite::Statement q(
        *db_,
        "SELECT p.product_id FROM products p"
        " WHERE p.distributor = ? AND p.tracked = 1"
        " AND COALESCE((SELECT MAX(i.available) FROM inventory_state i"
        "               WHERE i.distributor = p.distributor"
        "                 AND i.keycode = p.product_id), 0) = 0");
    q.bind(1, distributor);
    while (q.executeStep()) {
        out.emplace_back(q.getColumn(0).getString());
    }
    return out;
}

std::vector<std::string> Database::getTrackedKeycodes(int distributor) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> out;
    SQLite::Statement q(
        *db_,
        "SELECT product_id FROM products WHERE distributor = ? AND tracked = 1");
    q.bind(1, distributor);
    while (q.executeStep()) {
        out.emplace_back(q.getColumn(0).getString());
    }
    return out;
}

std::optional<Product> Database::getProduct(int distributor, const std::string& product_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement q(
        *db_,
        "SELECT product_id, distributor, name, url, brand, image_url, price, is_preorder,"
        " preorder_release_date, tracked, fulfilment_channel"
        " FROM products WHERE distributor = ? AND product_id = ?");
    q.bind(1, distributor);
    q.bind(2, product_id);
    if (!q.executeStep()) return std::nullopt;
    Product p;
    p.product_id = q.getColumn(0).getString();
    p.distributor = q.getColumn(1).getInt();
    p.name = q.getColumn(2).getString();
    p.url = q.getColumn(3).getString();
    p.brand = q.getColumn(4).getString();
    p.image_url = q.getColumn(5).getString();
    p.price = q.getColumn(6).getDouble();
    p.is_preorder = q.getColumn(7).getInt() != 0;
    p.preorder_release_date = q.getColumn(8).getString();
    p.tracked = q.getColumn(9).getInt() != 0;
    p.fulfilment_channel = q.getColumn(10).getInt();
    return p;
}

std::optional<int> Database::getStock(int distributor, const std::string& keycode,
                                      const std::string& channel,
                                      const std::string& location_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement q(
        *db_,
        "SELECT available FROM inventory_state"
        " WHERE distributor=? AND keycode=? AND channel=? AND location_id=?");
    q.bind(1, distributor);
    q.bind(2, keycode);
    q.bind(3, channel);
    q.bind(4, location_id);
    if (!q.executeStep()) return std::nullopt;
    return q.getColumn(0).getInt();
}

void Database::setStock(const ChannelStock& s) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement up(
        *db_,
        "INSERT INTO inventory_state (distributor, keycode, channel, location_id, available, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(distributor, keycode, channel, location_id)"
        " DO UPDATE SET available=excluded.available, updated_at=excluded.updated_at");
    up.bind(1, s.distributor);
    up.bind(2, s.keycode);
    up.bind(3, s.channel);
    up.bind(4, s.location_id);
    up.bind(5, s.available);
    up.bind(6, nowSeconds());
    up.exec();
}

void Database::recordAlert(const RestockEvent& e) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement ins(
        *db_,
        "INSERT INTO alerts (distributor, keycode, channel, location_id, prev_available,"
        " new_available, fired_at) VALUES (?, ?, ?, ?, ?, ?, ?)");
    ins.bind(1, e.distributor);
    ins.bind(2, e.keycode);
    ins.bind(3, e.channel);
    ins.bind(4, e.location_id);
    ins.bind(5, e.previous);
    ins.bind(6, e.available);
    ins.bind(7, e.timestamp ? e.timestamp : nowSeconds());
    ins.exec();
}

std::string Database::getMeta(const std::string& key) {
    SQLite::Statement q(*db_, "SELECT v FROM app_meta WHERE k=?");
    q.bind(1, key);
    if (q.executeStep()) return q.getColumn(0).getString();
    return {};
}

void Database::setMeta(const std::string& key, const std::string& value) {
    SQLite::Statement up(
        *db_,
        "INSERT INTO app_meta (k, v) VALUES (?, ?)"
        " ON CONFLICT(k) DO UPDATE SET v=excluded.v");
    up.bind(1, key);
    up.bind(2, value);
    up.exec();
}

std::string Database::getOrCreateSessionId() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::string id = getMeta("constructor_session_id");
    if (id.empty()) {
        id = makeUuid();
        setMeta("constructor_session_id", id);
    }
    return id;
}

long Database::nextSessionSeq() {
    std::lock_guard<std::mutex> lock(mtx_);
    long seq = 1;
    std::string cur = getMeta("constructor_session_seq");
    if (!cur.empty()) {
        seq = std::strtol(cur.c_str(), nullptr, 10) + 1;
    }
    setMeta("constructor_session_seq", std::to_string(seq));
    return seq;
}

std::string Database::getKmartCookie() {
    std::lock_guard<std::mutex> lock(mtx_);
    return getMeta("kmart_cookie");
}

void Database::setKmartCookie(const std::string& cookie) {
    std::lock_guard<std::mutex> lock(mtx_);
    setMeta("kmart_cookie", cookie);
}

std::string Database::getKmartUserAgent() {
    std::lock_guard<std::mutex> lock(mtx_);
    return getMeta("kmart_user_agent");
}

void Database::setKmartUserAgent(const std::string& user_agent) {
    std::lock_guard<std::mutex> lock(mtx_);
    setMeta("kmart_user_agent", user_agent);
}

std::string Database::getBigWCookie() {
    std::lock_guard<std::mutex> lock(mtx_);
    return getMeta("bigw_cookie");
}

void Database::setBigWCookie(const std::string& cookie) {
    std::lock_guard<std::mutex> lock(mtx_);
    setMeta("bigw_cookie", cookie);
}

std::string Database::getBigWUserAgent() {
    std::lock_guard<std::mutex> lock(mtx_);
    return getMeta("bigw_user_agent");
}

void Database::setBigWUserAgent(const std::string& user_agent) {
    std::lock_guard<std::mutex> lock(mtx_);
    setMeta("bigw_user_agent", user_agent);
}

}  // namespace restocker
