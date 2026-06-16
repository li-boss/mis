# WMS — 供应链智能仓储管理系统

基于 **C++17 + Vue3** 的轻量级仓储管理系统，支持采购入库、SKU 管理、供应商管理、库存看板、角色权限划分与 JWT 鉴权。

## 技术栈

| 层级 | 技术 |
|------|------|
| 后端 | C++17, [cpp-httplib](https://github.com/yhirose/cpp-httplib), [nlohmann/json](https://github.com/nlohmann/json), Oracle OCI |
| 前端 | Vue 3 (Composition API), Element Plus, Pinia, Axios, Vite |
| 数据库 | Oracle 23ai Free |
| 鉴权 | JWT (HS256) |

## 前置环境

- **Oracle 23ai Free** — 需提前安装并创建 PDB
- **CMake ≥ 3.20** + **GCC / MinGW-w64**（C++17）— 后端编译
- **Node.js ≥ 18** — 前端运行

Windows 推荐：Oracle Database 23ai Free + MSYS2/MinGW-w64 + Node.js LTS。

---

## 一、数据库初始化

### 1.1 创建数据库用户

在 Oracle 中创建一个用户并赋权（用户名可自行命名，不一定是 `wms`）：

```sql
-- 以 SYSDBA 身份连接
sqlplus sys/你的Sys密码@localhost:1521/orclpdb as sysdba

-- 创建用户（可自行改名）
CREATE USER wms IDENTIFIED BY 你的密码;
GRANT CONNECT, RESOURCE, UNLIMITED TABLESPACE TO wms;
```

### 1.2 修改后端连接信息

打开 **`backend/src/main.cpp`**，修改第 52 行的数据库连接参数为你的实际值：

```cpp
mis::dao::oracle().initialize("你的IP:端口/PDB名", "数据库用户名", "密码");
// 示例：
mis::dao::oracle().initialize("localhost:1521/orclpdb", "wms", "wms");
```

### 1.3 执行建库脚本

用 sqlplus 以你刚创建的用户身份连接，依次执行：

```bash
sqlplus wms/你的密码@localhost:1521/orclpdb
```

```sql
-- 推荐：一键重建（仅开发环境！会清空已有表和数据）
@路径/database/schema/reset_all.sql

-- 或 分步执行：
@路径/database/schema/tables.sql
@路径/database/schema/sequences.sql
@路径/database/schema/seed.sql
@路径/database/schema/migrate_auth.sql
```

> 脚本使用 `user_tables` / `user_sequences` 操作当前用户 schema，不写死用户名，连谁就建在谁的名下。

### 1.4 默认账户

| 用户名 | 密码 | 角色 | 说明 |
|--------|------|------|------|
| `admin` | `123456` | 管理员 | 全部权限 |
| `keeper` | `123456` | 库管员 | 入库操作 |
| `purchaser` | `123456` | 采购员 | 采购下单 |
| `data_manager` | `123456` | 数据管理员 | 商品/SKU与供应商数据管理 |

---

## 二、后端编译与启动

### 2.1 修改路径

**`backend/CMakeLists.txt`** 中两处路径需按你的环境修改：

```cmake
# 第 11 行 — cpp-httplib 克隆到的目录
find_path(HTTPLIB_INCLUDE_DIR httplib.h
    HINTS "D:/dev/db"              # ← 改为你的路径
)

# 第 27 行 — Oracle 23ai Free 安装目录
set(ORACLE_HOME "D:/tools/26ai/dbhomeFree" CACHE PATH "Oracle 23ai Free install dir")
                                       # ↑ 改为你的 Oracle Home
```

### 2.2 编译

```bash
cd backend
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

编译产物：`build/mis_backend.exe`

### 2.3 无 Oracle 模式

如果未安装 Oracle，CMake 找不到 OCI 库时会自动降级为 **内存存储模式**（数据不持久化）。无需额外配置。

### 2.4 启动

```bash
./mis_backend.exe
```

默认监听 `http://0.0.0.0:8080`。

---

## 三、前端启动

### 3.1 安装依赖

```bash
cd frontend
npm install
```

### 3.2 配置环境变量

在 `frontend/` 下创建 `.env` 文件：

```env
VITE_API_BASE_URL=http://localhost:8080/api
VITE_USE_MOCK=false
```

- `VITE_API_BASE_URL` — 后端 API 地址
- `VITE_USE_MOCK=true` — 使用前端 mock 数据，不需要后端

### 3.3 启动

```bash
npm run dev
```

前端默认 `http://localhost:5173`。生产构建：`npm run build`（产物在 `frontend/dist/`）。

---

## 四、API 概览

| 路径 | 方法 | 说明 | 鉴权 |
|------|------|------|------|
| `/api/auth/register` | POST | 注册 | 无 |
| `/api/auth/login` | POST | 登录，返回 JWT | 无 |
| `/api/inventory/dashboard` | GET | 库存看板 | Bearer |
| `/api/inventory/inbound` | GET/POST | 入库单列表 / 创建 | Bearer |
| `/api/inventory/inbound/:id/submit` | POST | 提交入库单 | Bearer |
| `/api/sku/list` | GET | SKU 列表 | Bearer |
| `/api/supplier/list` | GET | 供应商列表 | Bearer |
| `/api/warehouse/list` | GET | 仓库列表 | Bearer |

> 除 `/api/auth/*` 外，所有 `/api/*` 请求需携带 `Authorization: Bearer <token>`。

---

## 五、项目结构

```
mis/
├── backend/
│   ├── CMakeLists.txt          # CMake 构建（需修改 ORACLE_HOME、httplib 路径）
│   ├── include/
│   │   ├── controllers/        # 路由控制器
│   │   ├── services/           # 业务服务
│   │   ├── dao/                # Oracle 数据访问层
│   │   └── utils/              # JWT、编码工具
│   └── src/
│       ├── main.cpp            # 启动入口（数据库连接参数在此修改）
│       ├── controllers/
│       ├── services/
│       └── utils/
├── database/schema/
│   ├── tables.sql              # 建表
│   ├── sequences.sql           # 序列
│   ├── seed.sql                # 种子数据
│   ├── migrate_auth.sql        # Auth 迁移
│   └── reset_all.sql           # 一键重建（开发用）
├── frontend/
│   └── src/
│       ├── api/                # Axios 接口
│       ├── components/         # 通用组件
│       ├── views/              # 页面
│       ├── stores/             # Pinia
│       └── router/             # 路由
└── README.md
```
