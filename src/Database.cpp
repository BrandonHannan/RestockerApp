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

void Database::initSchema() {
    db_->exec(
        "CREATE TABLE IF NOT EXISTS products ("
        " variation_id TEXT PRIMARY KEY,"
        " name TEXT, url TEXT, brand TEXT, image_url TEXT, price REAL,"
        " is_preorder INTEGER DEFAULT 0,"
        " preorder_release_date TEXT,"
        " tracked INTEGER DEFAULT 1,"
        " fulfilment_channel INTEGER DEFAULT 0,"
        " first_seen INTEGER, last_seen INTEGER)");

    // Migrations: add columns to products tables created before they existed.
    // Each throws if the column already exists, which is fine to ignore.
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
        " keycode TEXT, channel TEXT, location_id TEXT DEFAULT '',"
        " available INTEGER, updated_at INTEGER,"
        " PRIMARY KEY (keycode, channel, location_id))");

    db_->exec(
        "CREATE TABLE IF NOT EXISTS alerts ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " keycode TEXT, channel TEXT, location_id TEXT,"
        " prev_available INTEGER, new_available INTEGER, fired_at INTEGER)");

    db_->exec("CREATE TABLE IF NOT EXISTS app_meta (k TEXT PRIMARY KEY, v TEXT)");
}

bool Database::insertProductIfAbsent(const std::string& keycode, const std::string& url,
                                     const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx_);
    const std::int64_t now = nowSeconds();

    // INSERT OR IGNORE: only a genuinely new keycode inserts a row. changes()
    // then reports 1 for an insert, 0 when the row already existed.
    SQLite::Statement ins(
        *db_,
        "INSERT OR IGNORE INTO products (variation_id, name, url, first_seen, last_seen)"
        " VALUES (?, ?, ?, ?, ?)");
    ins.bind(1, keycode);
    ins.bind(2, name);
    ins.bind(3, url);
    ins.bind(4, now);
    ins.bind(5, now);
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
        " WHERE variation_id=?");
    upd.bind(1, p.name);
    upd.bind(2, p.brand);
    upd.bind(3, p.image_url);
    upd.bind(4, p.price);
    upd.bind(5, p.is_preorder ? 1 : 0);
    upd.bind(6, p.preorder_release_date);
    upd.bind(7, p.tracked ? 1 : 0);
    upd.bind(8, p.fulfilment_channel);
    upd.bind(9, now);
    upd.bind(10, p.variation_id);
    upd.exec();
}

std::vector<std::string> Database::getTrackedOOSKeycodes() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> out;
    // A keycode qualifies when it is tracked and the best available stock we
    // have on record across every channel/location is 0 (or unknown).
    SQLite::Statement q(
        *db_,
        "SELECT p.variation_id FROM products p"
        " WHERE p.tracked = 1"
        " AND COALESCE((SELECT MAX(i.available) FROM inventory_state i"
        "               WHERE i.keycode = p.variation_id), 0) = 0");
    while (q.executeStep()) {
        out.emplace_back(q.getColumn(0).getString());
    }
    return out;
}

std::optional<Product> Database::getProduct(const std::string& keycode) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement q(
        *db_,
        "SELECT variation_id, name, url, brand, image_url, price, is_preorder,"
        " preorder_release_date, tracked, fulfilment_channel"
        " FROM products WHERE variation_id = ?");
    q.bind(1, keycode);
    if (!q.executeStep()) return std::nullopt;
    Product p;
    p.variation_id = q.getColumn(0).getString();
    p.name = q.getColumn(1).getString();
    p.url = q.getColumn(2).getString();
    p.brand = q.getColumn(3).getString();
    p.image_url = q.getColumn(4).getString();
    p.price = q.getColumn(5).getDouble();
    p.is_preorder = q.getColumn(6).getInt() != 0;
    p.preorder_release_date = q.getColumn(7).getString();
    p.tracked = q.getColumn(8).getInt() != 0;
    p.fulfilment_channel = q.getColumn(9).getInt();
    return p;
}

std::optional<int> Database::getStock(const std::string& keycode,
                                      const std::string& channel,
                                      const std::string& location_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement q(
        *db_,
        "SELECT available FROM inventory_state"
        " WHERE keycode=? AND channel=? AND location_id=?");
    q.bind(1, keycode);
    q.bind(2, channel);
    q.bind(3, location_id);
    if (!q.executeStep()) return std::nullopt;
    return q.getColumn(0).getInt();
}

void Database::setStock(const ChannelStock& s) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement up(
        *db_,
        "INSERT INTO inventory_state (keycode, channel, location_id, available, updated_at)"
        " VALUES (?, ?, ?, ?, ?)"
        " ON CONFLICT(keycode, channel, location_id)"
        " DO UPDATE SET available=excluded.available, updated_at=excluded.updated_at");
    up.bind(1, s.keycode);
    up.bind(2, s.channel);
    up.bind(3, s.location_id);
    up.bind(4, s.available);
    up.bind(5, nowSeconds());
    up.exec();
}

void Database::recordAlert(const RestockEvent& e) {
    std::lock_guard<std::mutex> lock(mtx_);
    SQLite::Statement ins(
        *db_,
        "INSERT INTO alerts (keycode, channel, location_id, prev_available,"
        " new_available, fired_at) VALUES (?, ?, ?, ?, ?, ?)");
    ins.bind(1, e.keycode);
    ins.bind(2, e.channel);
    ins.bind(3, e.location_id);
    ins.bind(4, e.previous);
    ins.bind(5, e.available);
    ins.bind(6, e.timestamp ? e.timestamp : nowSeconds());
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

}  // namespace restocker
