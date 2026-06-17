#include "dao/OracleConnector.hpp"
#include "models/InventoryModel.hpp"

#include <oci.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <vector>

struct OciConnection {
    OCIEnv* envhp{nullptr};
    OCIError* errhp{nullptr};
    OCISvcCtx* svchp{nullptr};
};

namespace {

thread_local OciConnection* tl_connection = nullptr;

void checkOciError(sword status, OCIError* errhp, const std::string& context) {
    if (status == OCI_SUCCESS || status == OCI_SUCCESS_WITH_INFO) {
        return;
    }
    text errbuf[1024] = {0};
    sb4 errcode = 0;
    if (errhp) {
        OCIErrorGet(errhp, 1, nullptr, &errcode, errbuf, sizeof(errbuf), OCI_HTYPE_ERROR);
        throw mis::models::DatabaseError(context + " failed (OCI-" + std::to_string(errcode) + "): " + std::string((char*)errbuf));
    } else {
        throw mis::models::DatabaseError(context + " failed with OCI status " + std::to_string(status));
    }
}

OciConnection* getThreadConnection() {
    if (tl_connection == nullptr) {
        throw mis::models::DatabaseError("No active database session for current thread");
    }
    return tl_connection;
}

} // namespace

namespace mis::dao {

OracleConnector::~OracleConnector()
{
    std::lock_guard<std::mutex> lock(poolMutex_);
    for (auto conn : pool_) {
        freeConnection(conn);
    }
    pool_.clear();
}

OracleConnector& OracleConnector::instance()
{
    static OracleConnector connector;
    return connector;
}

void OracleConnector::initialize(const std::string& connectionString,
                                 const std::string& username,
                                 const std::string& password)
{
    connectionString_ = connectionString;
    username_ = username;
    password_ = password;
    initialized_ = true;
}

void OracleConnector::acquireForCurrentThread()
{
    ensureInitialized();
    if (tl_connection != nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(poolMutex_);
    if (!pool_.empty()) {
        tl_connection = pool_.back();
        pool_.pop_back();
    } else {
        tl_connection = createConnection();
    }
}

void OracleConnector::releaseForCurrentThread()
{
    if (tl_connection == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(poolMutex_);
    pool_.push_back(tl_connection);
    tl_connection = nullptr;
}

OciConnection* OracleConnector::createConnection()
{
    OciConnection* conn = new OciConnection();

    sword status = OCIEnvCreate(&conn->envhp, OCI_THREADED, nullptr, nullptr, nullptr, nullptr, 0, nullptr);
    if (status != OCI_SUCCESS) {
        delete conn;
        throw models::DatabaseError("OCIEnvCreate failed");
    }

    status = OCIHandleAlloc(conn->envhp, (void**)&conn->errhp, OCI_HTYPE_ERROR, 0, nullptr);
    if (status != OCI_SUCCESS) {
        OCIHandleFree(conn->envhp, OCI_HTYPE_ENV);
        delete conn;
        throw models::DatabaseError("OCIHandleAlloc for error handle failed");
    }

    status = OCILogon2(conn->envhp, conn->errhp, &conn->svchp,
                       (text*)username_.c_str(), username_.length(),
                       (text*)password_.c_str(), password_.length(),
                       (text*)connectionString_.c_str(), connectionString_.length(),
                       OCI_DEFAULT);
    if (status != OCI_SUCCESS) {
        text errbuf[512] = {0};
        sb4 errcode = 0;
        OCIErrorGet(conn->errhp, 1, nullptr, &errcode, errbuf, sizeof(errbuf), OCI_HTYPE_ERROR);
        std::string errMsg = (char*)errbuf;

        OCIHandleFree(conn->errhp, OCI_HTYPE_ERROR);
        OCIHandleFree(conn->envhp, OCI_HTYPE_ENV);
        delete conn;
        throw models::DatabaseError("OCILogon2 failed: " + errMsg);
    }

    return conn;
}

void OracleConnector::freeConnection(OciConnection* conn)
{
    if (!conn) return;
    if (conn->svchp) {
        OCILogoff(conn->svchp, conn->errhp);
    }
    if (conn->errhp) {
        OCIHandleFree(conn->errhp, OCI_HTYPE_ERROR);
    }
    if (conn->envhp) {
        OCIHandleFree(conn->envhp, OCI_HTYPE_ENV);
    }
    delete conn;
}

std::vector<std::unordered_map<std::string, std::string>> OracleConnector::query(
    const std::string& sql,
    const std::unordered_map<std::string, std::string>& bindValues)
{
    ensureInitialized();
    OciConnection* conn = getThreadConnection();

    OCIStmt* stmthp = nullptr;
    sword status = OCIHandleAlloc(conn->envhp, (void**)&stmthp, OCI_HTYPE_STMT, 0, nullptr);
    checkOciError(status, conn->errhp, "OCIHandleAlloc for statement");

    struct StmtGuard {
        OCIEnv* env;
        OCIStmt* stmt;
        ~StmtGuard() { OCIHandleFree(stmt, OCI_HTYPE_STMT); }
    } guard{conn->envhp, stmthp};

    status = OCIStmtPrepare(stmthp, conn->errhp, (text*)sql.c_str(), sql.length(), OCI_NTV_SYNTAX, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCIStmtPrepare");

    // ---- Step 1: Describe only (no binds yet) to get column metadata ----
    //     Oracle 23ai throws ORA-01745 if binds are set before describe-only execute
    status = OCIStmtExecute(conn->svchp, stmthp, conn->errhp, 0, 0, nullptr, nullptr, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCIStmtExecute (describe)");

    ub4 numCols = 0;
    status = OCIAttrGet(stmthp, OCI_HTYPE_STMT, &numCols, nullptr, OCI_ATTR_PARAM_COUNT, conn->errhp);
    checkOciError(status, conn->errhp, "OCIAttrGet (param count)");

    struct ColumnBuffer {
        std::string name;
        std::vector<char> buffer;
        sb2 indicator{0};
        ub2 fetch_len{0};
    };
    std::vector<ColumnBuffer> cols(numCols);
    for (ub4 i = 1; i <= numCols; ++i) {
        OCIParam* parh = nullptr;
        status = OCIParamGet(stmthp, OCI_HTYPE_STMT, conn->errhp, (void**)&parh, i);
        checkOciError(status, conn->errhp, "OCIParamGet");

        text* colName = nullptr;
        ub4 colNameLen = 0;
        status = OCIAttrGet(parh, OCI_DTYPE_PARAM, &colName, &colNameLen, OCI_ATTR_NAME, conn->errhp);
        checkOciError(status, conn->errhp, "OCIAttrGet (column name)");
        cols[i-1].name = std::string((char*)colName, colNameLen);

        cols[i-1].buffer.resize(4096, 0);

        OCIDefine* defhp = nullptr;
        status = OCIDefineByPos(stmthp, &defhp, conn->errhp, i,
                                cols[i-1].buffer.data(), cols[i-1].buffer.size(),
                                SQLT_STR, &cols[i-1].indicator, &cols[i-1].fetch_len, nullptr, OCI_DEFAULT);
        checkOciError(status, conn->errhp, "OCIDefineByPos");

        OCIDescriptorFree(parh, OCI_DTYPE_PARAM);
    }

    // ---- Step 2: Bind variables and execute if there are binds ----
    //     Bind AFTER describe+define to avoid ORA-01745 in Oracle 23ai describe mode
    struct BindInfo {
        std::string name;
        std::string value;
        OCIBind* bindhp{nullptr};
    };
    std::vector<BindInfo> binds;
    binds.reserve(bindValues.size());
    for (const auto& kv : bindValues) {
        BindInfo b;
        b.name = kv.first[0] == ':' ? kv.first : ":" + kv.first;
        b.value = kv.second;
        binds.push_back(b);
    }

    for (auto& b : binds) {
        status = OCIBindByName(stmthp, &b.bindhp, conn->errhp,
                               (text*)b.name.c_str(), b.name.length(),
                               (void*)b.value.c_str(), b.value.length() + 1,
                               SQLT_STR, nullptr, nullptr, nullptr, 0, nullptr, OCI_DEFAULT);
        checkOciError(status, conn->errhp, "OCIBindByName for " + b.name);
    }

    if (!binds.empty()) {
        // Re-execute with iters=1 so Oracle actually runs the query with bind values.
        // The describe-only call above (iters=0) already validated the SQL structure.
        status = OCIStmtExecute(conn->svchp, stmthp, conn->errhp, 1, 0, nullptr, nullptr, OCI_DEFAULT);
        checkOciError(status, conn->errhp, "OCIStmtExecute (query exec)");
    }

    // ---- Step 3: Fetch rows ----
    std::vector<std::unordered_map<std::string, std::string>> results;
    while (true) {
        sword fetchStatus = OCIStmtFetch2(stmthp, conn->errhp, 1, OCI_FETCH_NEXT, 0, OCI_DEFAULT);
        if (fetchStatus == OCI_NO_DATA) {
            break;
        }
        checkOciError(fetchStatus, conn->errhp, "OCIStmtFetch2");

        std::unordered_map<std::string, std::string> row;
        for (const auto& col : cols) {
            if (col.indicator == -1) {
                row[col.name] = "";
            } else {
                row[col.name] = std::string(col.buffer.data());
            }
        }
        results.push_back(row);
    }
    return results;
}

void OracleConnector::execute(const std::string& sql,
                              const std::unordered_map<std::string, std::string>& bindValues)
{
    ensureInitialized();
    OciConnection* conn = getThreadConnection();

    OCIStmt* stmthp = nullptr;
    sword status = OCIHandleAlloc(conn->envhp, (void**)&stmthp, OCI_HTYPE_STMT, 0, nullptr);
    checkOciError(status, conn->errhp, "OCIHandleAlloc for statement");

    struct StmtGuard {
        OCIEnv* env;
        OCIStmt* stmt;
        ~StmtGuard() { OCIHandleFree(stmt, OCI_HTYPE_STMT); }
    } guard{conn->envhp, stmthp};

    status = OCIStmtPrepare(stmthp, conn->errhp, (text*)sql.c_str(), sql.length(), OCI_NTV_SYNTAX, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCIStmtPrepare");

    struct BindInfo {
        std::string name;
        std::string value;
        OCIBind* bindhp{nullptr};
    };
    std::vector<BindInfo> binds;
    binds.reserve(bindValues.size());
    for (const auto& kv : bindValues) {
        BindInfo b;
        b.name = kv.first[0] == ':' ? kv.first : ":" + kv.first;
        b.value = kv.second;
        binds.push_back(b);
    }

    for (auto& b : binds) {
        status = OCIBindByName(stmthp, &b.bindhp, conn->errhp,
                               (text*)b.name.c_str(), b.name.length(),
                               (void*)b.value.c_str(), b.value.length() + 1,
                               SQLT_STR, nullptr, nullptr, nullptr, 0, nullptr, OCI_DEFAULT);
        checkOciError(status, conn->errhp, "OCIBindByName for " + b.name);
    }

    status = OCIStmtExecute(conn->svchp, stmthp, conn->errhp, 1, 0, nullptr, nullptr, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCIStmtExecute");
}

void OracleConnector::callProcedure(const std::string& procedureName,
                                    const std::unordered_map<std::string, std::string>& bindValues)
{
    ensureInitialized();
    OciConnection* conn = getThreadConnection();

    std::stringstream sql;
    sql << "BEGIN " << procedureName << "(";
    bool first = true;
    for (const auto& kv : bindValues) {
        if (!first) sql << ", ";
        sql << kv.first << " => :" << kv.first;
        first = false;
    }
    sql << "); END;";

    std::string sqlStr = sql.str();

    OCIStmt* stmthp = nullptr;
    sword status = OCIHandleAlloc(conn->envhp, (void**)&stmthp, OCI_HTYPE_STMT, 0, nullptr);
    checkOciError(status, conn->errhp, "OCIHandleAlloc for procedure statement");

    struct StmtGuard {
        OCIEnv* env;
        OCIStmt* stmt;
        ~StmtGuard() { OCIHandleFree(stmt, OCI_HTYPE_STMT); }
    } guard{conn->envhp, stmthp};

    status = OCIStmtPrepare(stmthp, conn->errhp, (text*)sqlStr.c_str(), sqlStr.length(), OCI_NTV_SYNTAX, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCIStmtPrepare for procedure");

    struct BindInfo {
        std::string name;
        std::string value;
        OCIBind* bindhp{nullptr};
    };
    std::vector<BindInfo> binds;
    binds.reserve(bindValues.size());
    for (const auto& kv : bindValues) {
        BindInfo b;
        b.name = kv.first[0] == ':' ? kv.first : ":" + kv.first;
        b.value = kv.second;
        binds.push_back(b);
    }

    for (auto& b : binds) {
        status = OCIBindByName(stmthp, &b.bindhp, conn->errhp,
                               (text*)b.name.c_str(), b.name.length(),
                               (void*)b.value.c_str(), b.value.length() + 1,
                               SQLT_STR, nullptr, nullptr, nullptr, 0, nullptr, OCI_DEFAULT);
        checkOciError(status, conn->errhp, "OCIBindByName for " + b.name);
    }

    status = OCIStmtExecute(conn->svchp, stmthp, conn->errhp, 1, 0, nullptr, nullptr, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCIStmtExecute for procedure " + procedureName);
}

void OracleConnector::commit()
{
    ensureInitialized();
    OciConnection* conn = getThreadConnection();
    sword status = OCITransCommit(conn->svchp, conn->errhp, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCITransCommit");
}

void OracleConnector::rollback()
{
    ensureInitialized();
    OciConnection* conn = getThreadConnection();
    sword status = OCITransRollback(conn->svchp, conn->errhp, OCI_DEFAULT);
    checkOciError(status, conn->errhp, "OCITransRollback");
}

void OracleConnector::ensureInitialized() const
{
    if (!initialized_) {
        throw models::DatabaseError("OracleConnector has not been initialized");
    }
}

OracleConnector& oracle()
{
    return OracleConnector::instance();
}

} // namespace mis::dao
