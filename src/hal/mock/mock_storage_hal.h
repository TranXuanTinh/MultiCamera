#pragma once

#include "hal/i_storage_hal.h"
#include <string>
#include <vector>

namespace edge_ai { namespace hal {

class MockStorageHAL : public IStorageHAL {
public:
    MockStorageHAL() = default;
    ~MockStorageHAL() override = default;

    bool open(const std::string& /*db_path*/) override {
        opened_ = true;
        return true;
    }

    void close() override {
        opened_ = false;
    }

    bool isOpened() const override {
        return opened_;
    }

    bool execute(const std::string& sql) override {
        if (!opened_) return false;
        executed_sqls_.push_back(sql);
        ++insert_count_;
        return true;
    }

    SqlResultSet query(const std::string& /*sql*/) override {
        return {};
    }

    bool beginTransaction() override { return true; }
    bool commitTransaction() override { return true; }
    bool rollbackTransaction() override { return true; }

    int64_t lastInsertRowId() const override { return insert_count_; }

    size_t getExecutedSqlCount() const { return executed_sqls_.size(); }

private:
    bool opened_{false};
    int64_t insert_count_{0};
    std::vector<std::string> executed_sqls_;
};

}} // namespace edge_ai::hal

