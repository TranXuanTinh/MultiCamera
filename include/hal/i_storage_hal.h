#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace edge_ai { namespace hal {

using SqlRow = std::map<std::string, std::string>;
using SqlResultSet = std::vector<SqlRow>;

class IStorageHAL {
public:
    virtual ~IStorageHAL() = default;

    virtual bool open(const std::string& db_path) = 0;
    virtual void close() = 0;
    virtual bool isOpened() const = 0;

    virtual bool execute(const std::string& sql) = 0;
    virtual SqlResultSet query(const std::string& sql) = 0;

    virtual bool beginTransaction() = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;

    virtual int64_t lastInsertRowId() const = 0;
};

}} // namespace edge_ai::hal

