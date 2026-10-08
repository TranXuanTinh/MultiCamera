#pragma once

#include "hal/i_storage_hal.h"
#include <mutex>
#include <string>

#if defined(ENABLE_SQLITE3)
#include <sqlite3.h>
#endif

namespace edge_ai { namespace hal {

class JetsonStorageHAL : public IStorageHAL {
public:
    JetsonStorageHAL();
    ~JetsonStorageHAL() override;

    bool open(const std::string& db_path) override;
    void close() override;
    bool isOpened() const override;

    bool execute(const std::string& sql) override;
    SqlResultSet query(const std::string& sql) override;

    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;

    int64_t lastInsertRowId() const override;

private:
#if defined(ENABLE_SQLITE3)
    sqlite3* db_{nullptr};
#endif
    bool opened_{false};
    mutable std::mutex mtx_;
};

}} // namespace edge_ai::hal

