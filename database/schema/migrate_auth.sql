-- =============================================================================
-- WMS — Auth 迁移到 Oracle 的数据库变更
-- 在已有 tables.sql 的数据库上运行
-- =============================================================================

-- 1. 扩展角色约束：同时接受小写（代码用）和大写（Oracle 惯例）
ALTER TABLE users DROP CONSTRAINT ck_users_role;
ALTER TABLE users ADD CONSTRAINT ck_users_role
    CHECK (role IN ('admin', 'keeper', 'purchaser', 'data_manager',
                    'ADMIN', 'OPERATOR', 'CUSTOMER'));

-- 2. 添加 real_name 列（用于 JWT claims）
ALTER TABLE users ADD (real_name VARCHAR2(64));

-- 3. 更新已有种子用户的 role 为小写（与代码对齐）
UPDATE users SET role = 'admin'        WHERE username = 'admin';
UPDATE users SET role = 'keeper'       WHERE username = 'keeper';
UPDATE users SET role = 'purchaser'    WHERE username = 'purchaser';
UPDATE users SET real_name = username  WHERE real_name IS NULL;

COMMIT;
