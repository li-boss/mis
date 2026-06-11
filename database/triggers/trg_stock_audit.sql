-- =============================================================================
-- WMS — 库存审计触发器 (Oracle)
-- 
-- 触发器：trg_stock_audit
-- 目标表：inventory
-- 时机：  AFTER INSERT OR UPDATE OR DELETE，FOR EACH ROW
-- 写入：  audit_logs（每条库存变更一条记录）
--
-- 记录内容：
--   - INSERT：new_value = 初始库存信息
--   - UPDATE：old_value = 变更前；new_value = 变更后
--   - DELETE：old_value = 被删除的库存记录
--
-- 协作契约：
--   - 入库/出库/盘点操作均由存储过程或应用层触发 inventory 变更
--   - 本触发器自动捕获变更，无需应用层额外调用
--   - changed_by 通过 SYS_CONTEXT 传递操作人 ID（需应用层 SET_CONTEXT）
-- =============================================================================

-- -----------------------------------------------------------------------------
-- 可选：创建应用上下文（用于传递操作人 ID）
-- 应用层在执行入库前调用：
--   DBMS_SESSION.SET_CONTEXT('WMS_CTX', 'operator_id', 2);
-- -----------------------------------------------------------------------------
-- CREATE OR REPLACE CONTEXT WMS_CTX USING wms_ctx_pkg ACCESSED GLOBALLY;

CREATE OR REPLACE TRIGGER trg_stock_audit
    AFTER INSERT OR UPDATE OR DELETE ON inventory
    FOR EACH ROW
DECLARE
    v_action     audit_logs.action%TYPE;
    v_record_id  audit_logs.record_id%TYPE;
    v_old_json   audit_logs.old_value%TYPE;
    v_new_json   audit_logs.new_value%TYPE;
    v_operator   audit_logs.changed_by%TYPE;
BEGIN
    -- ===== 确定操作类型 =====
    IF INSERTING THEN
        v_action := 'INSERT';
        v_record_id := :NEW.inventory_id;
    ELSIF UPDATING THEN
        v_action := 'UPDATE';
        v_record_id := :NEW.inventory_id;
    ELSIF DELETING THEN
        v_action := 'DELETE';
        v_record_id := :OLD.inventory_id;
    END IF;

    -- ===== 构造 old_value JSON =====
    IF v_action IN ('UPDATE', 'DELETE') THEN
        v_old_json := '{"inventory_id":'   || :OLD.inventory_id
                   || ',"product_id":'      || :OLD.product_id
                   || ',"quantity":'        || :OLD.quantity
                   || ',"version":'         || :OLD.version
                   || ',"updated_at":"'     || TO_CHAR(:OLD.updated_at, 'YYYY-MM-DD HH24:MI:SS') || '"'
                   || '}';
    END IF;

    -- ===== 构造 new_value JSON =====
    IF v_action IN ('INSERT', 'UPDATE') THEN
        v_new_json := '{"inventory_id":'   || :NEW.inventory_id
                   || ',"product_id":'      || :NEW.product_id
                   || ',"quantity":'        || :NEW.quantity
                   || ',"version":'         || :NEW.version
                   || ',"updated_at":"'     || TO_CHAR(:NEW.updated_at, 'YYYY-MM-DD HH24:MI:SS') || '"'
                   || '}';
    END IF;

    -- ===== 获取操作人（优先从应用上下文取，否则 NULL） =====
    BEGIN
        v_operator := TO_NUMBER(SYS_CONTEXT('WMS_CTX', 'operator_id'));
    EXCEPTION
        WHEN OTHERS THEN
            v_operator := NULL;
    END;

    -- ===== 写入审计日志 =====
    INSERT INTO audit_logs (
        log_id, table_name, record_id, action,
        old_value, new_value, changed_by, changed_at
    ) VALUES (
        seq_audit_logs.NEXTVAL,
        'INVENTORY',
        v_record_id,
        v_action,
        v_old_json,
        v_new_json,
        v_operator,
        SYSTIMESTAMP
    );
END trg_stock_audit;
/

-- =============================================================================
-- 辅助：应用上下文管理包（供前后端设置操作人标识）
-- =============================================================================
CREATE OR REPLACE PACKAGE wms_ctx_pkg AS
    -- 设置当前操作人 ID
    -- 调用示例：
    --   EXEC wms_ctx_pkg.set_operator(2);
    PROCEDURE set_operator(p_user_id IN NUMBER);

    -- 清除上下文（用户登出时调用）
    PROCEDURE clear_operator;

    -- 获取当前操作人 ID
    FUNCTION get_operator RETURN NUMBER;
END wms_ctx_pkg;
/

CREATE OR REPLACE PACKAGE BODY wms_ctx_pkg AS
    PROCEDURE set_operator(p_user_id IN NUMBER) IS
    BEGIN
        DBMS_SESSION.SET_CONTEXT('WMS_CTX', 'operator_id', p_user_id);
    END set_operator;

    PROCEDURE clear_operator IS
    BEGIN
        DBMS_SESSION.SET_CONTEXT('WMS_CTX', 'operator_id', NULL);
    END clear_operator;

    FUNCTION get_operator RETURN NUMBER IS
    BEGIN
        RETURN TO_NUMBER(SYS_CONTEXT('WMS_CTX', 'operator_id'));
    EXCEPTION
        WHEN OTHERS THEN
            RETURN NULL;
    END get_operator;
END wms_ctx_pkg;
/

-- =============================================================================
-- 验证测试（上线前删除）
-- =============================================================================
/*
-- 1. 创建测试数据
INSERT INTO inventory (inventory_id, product_id, quantity, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1, 100, 0, SYSTIMESTAMP);
COMMIT;

-- 2. 更新库存 → 应自动生成 audit_logs 记录
UPDATE inventory SET quantity = 150, version = version + 1 WHERE product_id = 1;
COMMIT;

-- 3. 查看审计记录
SELECT log_id, table_name, action, old_value, new_value, changed_at
FROM audit_logs
WHERE table_name = 'INVENTORY'
ORDER BY changed_at DESC;

-- 4. 清理测试数据
DELETE FROM audit_logs WHERE table_name = 'INVENTORY';
DELETE FROM inventory WHERE product_id = 1;
COMMIT;
*/
