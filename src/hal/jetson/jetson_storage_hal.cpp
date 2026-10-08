#include "jetson_storage_hal.h"
#include <iostream>

namespace edge_ai { namespace hal {

JetsonStorageHAL::JetsonStorageHAL() = default;

JetsonStorageHAL::~JetsonStorageHAL() {
    close();
}

bool JetsonStorageHAL::open(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mtx_);
    close();

#if defined(ENABLE_SQLITE3)
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }
    // Enable WAL mode for high concurrency
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    opened_ = true;
    return true;
#else
    (void)db_path;
    opened_ = true;
    return true;
#endif
}

void JetsonStorageHAL::close() {
    std::lock_guard<std::mutex> lock(mtx_);
#if defined(ENABLE_SQLITE3)
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
#endif
    opened_ = false;
}

bool JetsonStorageHAL::isOpened() const {
    return opened_;
}

bool JetsonStorageHAL::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!opened_) return false;

#if defined(ENABLE_SQLITE3)
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) {
            std::cerr << "[SQLite Error] " << errmsg << " in SQL: " << sql << std::endl;
            sqlite3_free(errmsg);
        }
        return false;
    }
    return true;
#else
    (void)sql;
    return true;
#endif
}

SqlResultSet JetsonStorageHAL::query(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx_);
    SqlResultSet results;
    if (!opened_) return results;

#if defined(ENABLE_SQLITE3)
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SqlRow row;
        for (int i = 0; i < cols; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            const unsigned char* val = sqlite3_column_text(stmt, i);
            row[name ? name : ""] = val ? reinterpret_cast<const char*>(val) : "";
        }
        results.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
#else
    (void)sql;
#endif
    return results;
}

bool JetsonStorageHAL::beginTransaction() {
    return execute("BEGIN TRANSACTION;");
}

bool JetsonStorageHAL::commitTransaction() {
    return execute("COMMIT;");
}

bool JetsonStorageHAL::rollbackTransaction() {
    return execute("ROLLBACK;");
}

int64_t JetsonStorageHAL::lastInsertRowId() const {
    std::lock_guard<std::mutex> lock(mtx_);
#if defined(ENABLE_SQLITE3)
    return db_ ? sqlite3_last_insert_rowid(db_) : 0;
#else
    return 0;
#endif
}

}} // namespace edge_ai::hal

