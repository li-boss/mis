// =============================================================================
// AuthService — 用户鉴权服务
// Oracle 优先 + 内存降级
// =============================================================================

#include "services/AuthService.hpp"

#ifdef MIS_HAS_ORACLE
#include "dao/OracleConnector.hpp"
#endif

#include <algorithm>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace mis::services {

// ---- 角色映射 ----
static const std::unordered_map<std::string, std::string> ROLE_NAMES = {
    {"admin",        "系统管理员"},
    {"keeper",       "库管员"},
    {"purchaser",    "采购员"},
    {"data_manager", "数据管理员"}
};

std::string AuthService::roleToName(const std::string& role)
{
    auto it = ROLE_NAMES.find(role);
    return it != ROLE_NAMES.end() ? it->second : role;
}

// ---- Oracle 可用性检测 ----
static bool oracleAvailable()
{
#ifdef MIS_HAS_ORACLE
    static bool checked = false;
    static bool available = false;
    if (checked) return available;
    checked = true;

    try {
        mis::dao::DbSessionGuard db;
        mis::dao::oracle().query("SELECT 1 FROM DUAL");
        available = true;
        std::cout << "[AUTH] Oracle 检测通过，使用 Oracle 存储\n";
    } catch (const std::exception& ex) {
        std::cerr << "[AUTH] Oracle 不可用: " << ex.what() << "，降级内存存储\n";
        available = false;
    } catch (...) {
        std::cerr << "[AUTH] Oracle 不可用: 未知异常，降级内存存储\n";
        available = false;
    }
    return available;
#else
    return false;
#endif
}

static AuthService::AuthUser rowToUser(
    const std::unordered_map<std::string, std::string>& r)
{
    AuthService::AuthUser u;
    u.userId = std::stoi(r.at("USER_ID"));
    u.username = r.at("USERNAME");
    u.password = r.at("PASSWORD_HASH");
    u.realName = r.count("REAL_NAME") && !r.at("REAL_NAME").empty()
                 ? r.at("REAL_NAME") : u.username;
    u.role = r.at("ROLE");
    u.roleName = AuthService::roleToName(u.role);
    return u;
}

// =========================================================================
// Oracle 模式
// =========================================================================
#ifdef MIS_HAS_ORACLE

static AuthService::AuthUser oracleLogin(const std::string& username,
                                          const std::string& password)
{
    mis::dao::DbSessionGuard db;

    auto rows = mis::dao::oracle().query(
        "SELECT user_id, username, password_hash, real_name, role "
        "FROM users WHERE username = :uname",
        {{"uname", username}}
    );

    if (rows.empty())
        throw std::runtime_error("用户名或密码错误");

    const auto& r = rows[0];
    std::string dbPassword = r.at("PASSWORD_HASH");

    // 演示阶段明文比较（生产应使用 bcrypt）
    if (dbPassword != password)
        throw std::runtime_error("用户名或密码错误");

    return rowToUser(r);
}

static AuthService::AuthUser oracleRegister(const std::string& username,
                                             const std::string& password,
                                             const std::string& realName)
{
    mis::dao::DbSessionGuard db;

    // 检查是否已存在
    auto existing = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM users WHERE username = :uname",
        {{"uname", username}}
    );
    if (!existing.empty() && std::stoi(existing[0].at("CNT")) > 0)
        throw std::runtime_error("用户名已存在");

    // 获取新 ID
    auto seqRow = mis::dao::oracle().query(
        "SELECT seq_users.NEXTVAL AS ID FROM DUAL");
    int newId = std::stoi(seqRow[0].at("ID"));

    std::string rname = realName.empty() ? username : realName;

    mis::dao::oracle().execute(
        "INSERT INTO users (user_id, username, password_hash, real_name, role) "
        "VALUES (:id, :uname, :pwd, :rname, 'keeper')",
        {{"id", std::to_string(newId)},
         {"uname", username},
         {"pwd", password},
         {"rname", rname}}
    );
    mis::dao::oracle().commit();

    AuthService::AuthUser u;
    u.userId = newId;
    u.username = username;
    u.password = password;
    u.realName = rname;
    u.role = "keeper";
    u.roleName = AuthService::roleToName("keeper");
    return u;
}

static AuthService::AuthUser oracleGetById(int userId)
{
    mis::dao::DbSessionGuard db;

    auto rows = mis::dao::oracle().query(
        "SELECT user_id, username, password_hash, real_name, role "
        "FROM users WHERE user_id = :uid",
        {{"uid", std::to_string(userId)}}
    );

    if (rows.empty())
        throw std::runtime_error("用户不存在");

    return rowToUser(rows[0]);
}

static std::vector<AuthService::AuthUser> oracleListAll()
{
    mis::dao::DbSessionGuard db;

    auto rows = mis::dao::oracle().query(
        "SELECT user_id, username, password_hash, real_name, role "
        "FROM users ORDER BY user_id"
    );

    std::vector<AuthService::AuthUser> result;
    for (const auto& r : rows)
        result.push_back(rowToUser(r));
    return result;
}

static void oracleUpdateRole(int userId, const std::string& newRole)
{
    mis::dao::DbSessionGuard db;

    auto rows = mis::dao::oracle().query(
        "SELECT COUNT(*) AS CNT FROM users WHERE user_id = :uid",
        {{"uid", std::to_string(userId)}}
    );
    if (rows.empty() || std::stoi(rows[0].at("CNT")) == 0)
        throw std::runtime_error("用户不存在");

    mis::dao::oracle().execute(
        "UPDATE users SET role = :role WHERE user_id = :uid",
        {{"role", newRole}, {"uid", std::to_string(userId)}}
    );
    mis::dao::oracle().commit();
}

#endif // MIS_HAS_ORACLE

// =========================================================================
// 内存存储（降级模式）
// =========================================================================

namespace {

struct MemUser {
    int id;
    std::string username;
    std::string password;
    std::string realName;
    std::string role;
};

std::vector<MemUser> memUsers;
std::mutex memMutex;
int memNextId = 1;

void initMemUsers()
{
    if (!memUsers.empty()) return;
    memUsers.push_back({memNextId++, "admin",    "123456", "管理员",     "admin"});
    memUsers.push_back({memNextId++, "keeper",   "123456", "库管员",     "keeper"});
    memUsers.push_back({memNextId++, "buyer",    "123456", "采购员",     "purchaser"});
    memUsers.push_back({memNextId++, "data_mgr", "123456", "数据管理员", "data_manager"});
}

AuthService::AuthUser memToUser(const MemUser& m)
{
    AuthService::AuthUser u;
    u.userId = m.id;
    u.username = m.username;
    u.password = m.password;
    u.realName = m.realName;
    u.role = m.role;
    u.roleName = AuthService::roleToName(m.role);
    return u;
}

} // namespace

static AuthService::AuthUser memLogin(const std::string& username,
                                       const std::string& password)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemUsers();

    auto it = std::find_if(memUsers.begin(), memUsers.end(),
        [&](const MemUser& u) {
            return u.username == username && u.password == password;
        });
    if (it == memUsers.end())
        throw std::runtime_error("用户名或密码错误");

    return memToUser(*it);
}

static AuthService::AuthUser memRegister(const std::string& username,
                                          const std::string& password,
                                          const std::string& realName)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemUsers();

    auto it = std::find_if(memUsers.begin(), memUsers.end(),
        [&](const MemUser& u) { return u.username == username; });
    if (it != memUsers.end())
        throw std::runtime_error("用户名已存在");

    MemUser m;
    m.id = memNextId++;
    m.username = username;
    m.password = password;
    m.realName = realName.empty() ? username : realName;
    m.role = "keeper";
    memUsers.push_back(m);
    return memToUser(m);
}

static AuthService::AuthUser memGetById(int userId)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemUsers();

    auto it = std::find_if(memUsers.begin(), memUsers.end(),
        [userId](const MemUser& u) { return u.id == userId; });
    if (it == memUsers.end())
        throw std::runtime_error("用户不存在");

    return memToUser(*it);
}

static std::vector<AuthService::AuthUser> memListAll()
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemUsers();

    std::vector<AuthService::AuthUser> result;
    for (const auto& m : memUsers)
        result.push_back(memToUser(m));
    return result;
}

static void memUpdateRole(int userId, const std::string& newRole)
{
    std::lock_guard<std::mutex> lock(memMutex);
    initMemUsers();

    auto it = std::find_if(memUsers.begin(), memUsers.end(),
        [userId](const MemUser& u) { return u.id == userId; });
    if (it == memUsers.end())
        throw std::runtime_error("用户不存在");

    it->role = newRole;
}

// =========================================================================
// 统一接口（自动选择 Oracle / 内存）
// =========================================================================

#ifdef MIS_HAS_ORACLE
#define TRY_AUTH_ORACLE(call, fallback) \
    do { \
        if (oracleAvailable()) { \
            try { return oracle##call; } catch (const std::exception& ex) { \
                std::cerr << "[AUTH] Oracle 操作失败: " << ex.what() << "，降级内存\n"; \
            } \
        } \
        return mem##call; \
    } while(0)

#define TRY_AUTH_ORACLE_VOID(call, fallback) \
    do { \
        if (oracleAvailable()) { \
            try { oracle##call; return; } catch (const std::exception& ex) { \
                std::cerr << "[AUTH] Oracle 操作失败: " << ex.what() << "，降级内存\n"; \
            } \
        } \
        mem##call; \
    } while(0)
#else
#define TRY_AUTH_ORACLE(call, fallback) return mem##call;
#define TRY_AUTH_ORACLE_VOID(call, fallback) mem##call;
#endif

AuthService::AuthUser AuthService::login(const std::string& username,
                                          const std::string& password)
{
    TRY_AUTH_ORACLE(Login(username, password), Login(username, password));
}

AuthService::AuthUser AuthService::registerUser(const std::string& username,
                                                 const std::string& password,
                                                 const std::string& realName)
{
    TRY_AUTH_ORACLE(Register(username, password, realName),
                    Register(username, password, realName));
}

AuthService::AuthUser AuthService::getById(int userId)
{
    TRY_AUTH_ORACLE(GetById(userId), GetById(userId));
}

std::vector<AuthService::AuthUser> AuthService::listAll()
{
    TRY_AUTH_ORACLE(ListAll(), ListAll());
}

void AuthService::updateRole(int userId, const std::string& newRole)
{
    TRY_AUTH_ORACLE_VOID(UpdateRole(userId, newRole), UpdateRole(userId, newRole));
}

AuthService makeAuthService()
{
    return AuthService{};
}

} // namespace mis::services
