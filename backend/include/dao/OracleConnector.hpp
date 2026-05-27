#pragma once

#include <string>
#include <unordered_map>

namespace mis::dao {

class OracleConnector {
public:
    static OracleConnector& instance();

    void initialize(const std::string& connectionString,
                    const std::string& username,
                    const std::string& password);

    void execute(const std::string& sql,
                 const std::unordered_map<std::string, std::string>& bindValues = {});

    void callProcedure(const std::string& procedureName,
                       const std::unordered_map<std::string, std::string>& bindValues);

    void commit();
    void rollback();

private:
    OracleConnector() = default;
    void ensureInitialized() const;

    bool initialized_{false};
    std::string connectionString_;
    std::string username_;
    std::string password_;
};

OracleConnector& oracle();

} // namespace mis::dao
