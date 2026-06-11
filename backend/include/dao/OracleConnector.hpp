#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

struct OciConnection;

namespace mis::dao {

class OracleConnector {
public:
    static OracleConnector& instance();

    void initialize(const std::string& connectionString,
                    const std::string& username,
                    const std::string& password);

    std::vector<std::unordered_map<std::string, std::string>> query(
        const std::string& sql,
        const std::unordered_map<std::string, std::string>& bindValues = {});

    void execute(const std::string& sql,
                 const std::unordered_map<std::string, std::string>& bindValues = {});

    void callProcedure(const std::string& procedureName,
                       const std::unordered_map<std::string, std::string>& bindValues);

    void commit();
    void rollback();

    void acquireForCurrentThread();
    void releaseForCurrentThread();

private:
    OracleConnector() = default;
    ~OracleConnector();
    void ensureInitialized() const;

    OciConnection* createConnection();
    void freeConnection(OciConnection* conn);

    bool initialized_{false};
    std::string connectionString_;
    std::string username_;
    std::string password_;

    std::vector<OciConnection*> pool_;
    std::mutex poolMutex_;
};

OracleConnector& oracle();

struct DbSessionGuard {
    DbSessionGuard() {
        oracle().acquireForCurrentThread();
    }
    ~DbSessionGuard() {
        oracle().releaseForCurrentThread();
    }
};

} // namespace mis::dao
