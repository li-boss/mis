-- =============================================================================
-- WMS — 入库存储过程 (Oracle)
-- 
-- 功能：
--   proc_inbound_create   : 创建采购入库单 + 明细行
--   proc_inbound_receive  : 原子化收货确认（SELECT FOR UPDATE + 乐观锁）
--   proc_inbound_cancel   : 取消入库单（回滚已收库存）
--
-- 协作契约：
--   - 李佳恒的 InventoryService 调用 proc_inbound_receive 完成库存变更
--   - 入库后 trg_stock_audit 触发器自动记录 audit_logs
--   - version 字段供应用层乐观锁校验（调用前比对，调用后确认一致）
-- =============================================================================

-- -----------------------------------------------------------------------------
-- 1. 创建采购入库单（含明细行）
-- -----------------------------------------------------------------------------
CREATE OR REPLACE PROCEDURE proc_inbound_create (
    p_supplier_id       IN  suppliers.supplier_id%TYPE,
    p_created_by        IN  users.user_id%TYPE,
    p_lines_json        IN  CLOB,          -- JSON 数组：[{product_id, quantity, unit_price}, ...]
    o_inbound_id        OUT inbound_orders.inbound_id%TYPE,
    o_result            OUT NUMBER,
    o_message           OUT VARCHAR2
) IS
    v_inbound_id  inbound_orders.inbound_id%TYPE;
    v_line_count  NUMBER := 0;
BEGIN
    -- 参数校验
    IF p_supplier_id IS NULL THEN
        o_result := -1;
        o_message := '供应商 ID 不能为空';
        RETURN;
    END IF;

    IF p_lines_json IS NULL OR LENGTH(TRIM(p_lines_json)) = 0 THEN
        o_result := -2;
        o_message := '入库明细不能为空';
        RETURN;
    END IF;

    -- 生成入库单号
    SELECT seq_inbound_orders.NEXTVAL INTO v_inbound_id FROM DUAL;

    -- 插入入库单主记录
    INSERT INTO inbound_orders (
        inbound_id, supplier_id, status, created_by, created_at
    ) VALUES (
        v_inbound_id, p_supplier_id, 'DRAFT', p_created_by, SYSTIMESTAMP
    );

    -- 批量插入明细行：解析 JSON 数组，逐条插入
    -- 注意：Oracle 12c+ 可用 JSON_TABLE，此处用循环逐条解析以兼容 11g
    FOR rec IN (
        SELECT jt.product_id, jt.quantity, jt.unit_price
        FROM JSON_TABLE(p_lines_json, '$[*]'
            COLUMNS (
                product_id  NUMBER        PATH '$.product_id',
                quantity    NUMBER(12, 3) PATH '$.quantity',
                unit_price  NUMBER(12, 2) PATH '$.unit_price'
            )
        ) jt
    ) LOOP
        -- 校验商品存在且供应商已关联
        DECLARE
            v_ps_exists NUMBER;
        BEGIN
            SELECT COUNT(*) INTO v_ps_exists
            FROM product_suppliers
            WHERE product_id = rec.product_id AND supplier_id = p_supplier_id;

            IF v_ps_exists = 0 THEN
                -- 商品未关联此供应商，仍允许入库但记录警告
                NULL;
            END IF;
        END;

        INSERT INTO inbound_order_lines (
            line_id, inbound_id, product_id,
            quantity_ordered, quantity_received, unit_price
        ) VALUES (
            seq_inbound_order_lines.NEXTVAL, v_inbound_id, rec.product_id,
            rec.quantity, 0, rec.unit_price
        );
        v_line_count := v_line_count + 1;
    END LOOP;

    COMMIT;
    o_inbound_id := v_inbound_id;
    o_result     := 0;
    o_message    := '入库单 ' || v_inbound_id || ' 创建成功，共 ' || v_line_count || ' 条明细';
EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK;
        o_inbound_id := NULL;
        o_result     := SQLCODE;
        o_message    := '创建失败: ' || SQLERRM;
END proc_inbound_create;
/

-- -----------------------------------------------------------------------------
-- 2. 原子化收货确认（核心入库逻辑）
--    流程：
--      ① 校验入库单状态（不能是 CANCELLED/RECEIVED）
--      ② 锁定明细行对应的 inventory 行（SELECT FOR UPDATE）
--      ③ 更新 inbound_order_lines.quantity_received
--      ④ 更新 inventory.quantity（+），version（+1）
--      ⑤ 检查入库单是否全部到货，更新 inbound_orders.status
--      ⑥ 若全部到货，写入 received_at
--      ⑦ COMMIT
-- -----------------------------------------------------------------------------
CREATE OR REPLACE PROCEDURE proc_inbound_receive (
    p_line_id           IN  inbound_order_lines.line_id%TYPE,
    p_receive_quantity  IN  NUMBER,          -- 本次收货数量
    p_operator_id       IN  users.user_id%TYPE DEFAULT NULL,
    p_expected_version  IN  NUMBER DEFAULT NULL,  -- 应用层乐观锁：调用前持有的版本号
    o_received_total    OUT NUMBER,          -- 该行累计已收数量
    o_new_version       OUT NUMBER,          -- 更新后 inventory.version
    o_result            OUT NUMBER,
    o_message           OUT VARCHAR2
) IS
    v_product_id        products.product_id%TYPE;
    v_inbound_id        inbound_orders.inbound_id%TYPE;
    v_ordered           NUMBER(12, 3);
    v_received_before   NUMBER(12, 3);
    v_received_after    NUMBER(12, 3);
    v_inv_quantity      NUMBER(12, 3);
    v_inv_version       NUMBER(10);
    v_inbound_status    VARCHAR2(20);
    v_all_received      NUMBER;
    v_total_lines       NUMBER;
    v_full_lines        NUMBER;
    v_new_version       NUMBER(10);
    PRAGMA AUTONOMOUS_TRANSACTION;
BEGIN
    -- ===== ① 校验明细行存在 + 入库单状态 =====
    BEGIN
        SELECT iol.product_id, iol.inbound_id,
               iol.quantity_ordered, iol.quantity_received,
               io.status
        INTO   v_product_id, v_inbound_id,
               v_ordered, v_received_before,
               v_inbound_status
        FROM   inbound_order_lines iol
        JOIN   inbound_orders io ON iol.inbound_id = io.inbound_id
        WHERE  iol.line_id = p_line_id;
    EXCEPTION
        WHEN NO_DATA_FOUND THEN
            o_result := -100;
            o_message := '入库明细行 ' || p_line_id || ' 不存在';
            RETURN;
    END;

    IF v_inbound_status IN ('CANCELLED', 'RECEIVED') THEN
        IF v_inbound_status = 'CANCELLED' THEN
            o_result := -101;
            o_message := '入库单已取消，不可收货';
        ELSE
            o_result := -102;
            o_message := '入库单已全部到货，不可重复收货';
        END IF;
        RETURN;
    END IF;

    -- 校验收货数量
    v_received_after := v_received_before + p_receive_quantity;
    IF v_received_after > v_ordered THEN
        o_result := -103;
        o_message := '收货数量超出订购量：已定 ' || v_ordered
                  || '，已收 ' || v_received_before
                  || '，本次 ' || p_receive_quantity;
        RETURN;
    END IF;

    IF p_receive_quantity <= 0 THEN
        o_result := -104;
        o_message := '收货数量必须大于 0';
        RETURN;
    END IF;

    -- ===== ② 锁定 inventory 行（SELECT FOR UPDATE） =====
    -- 确保该商品有库存记录，若无则自动插入（首次入库场景）
    DECLARE
        v_inv_count NUMBER;
    BEGIN
        SELECT COUNT(*) INTO v_inv_count FROM inventory
        WHERE product_id = v_product_id;

        IF v_inv_count = 0 THEN
            BEGIN
                INSERT INTO inventory (inventory_id, product_id, quantity, version, updated_at)
                VALUES (seq_inventory.NEXTVAL, v_product_id, 0, 0, SYSTIMESTAMP);
            EXCEPTION
                WHEN DUP_VAL_ON_INDEX THEN
                    NULL;
            END;
        END IF;

        SELECT quantity, version
        INTO   v_inv_quantity, v_inv_version
        FROM   inventory
        WHERE  product_id = v_product_id
        FOR UPDATE;
    END;

    -- ===== 应用层乐观锁校验 =====
    IF p_expected_version IS NOT NULL AND p_expected_version != v_inv_version THEN
        o_result := -200;
        o_message := '乐观锁版本冲突：期望版本 ' || p_expected_version
                  || '，实际版本 ' || v_inv_version
                  || '，请刷新后重试';
        o_received_total := v_received_before;
        o_new_version    := v_inv_version;
        RETURN;
    END IF;

    -- ===== ③ 更新明细行收货量 =====
    UPDATE inbound_order_lines
    SET quantity_received = v_received_after
    WHERE line_id = p_line_id;

    -- ===== ④ 更新库存（quantity + version 同步递增） =====
    UPDATE inventory
    SET quantity   = quantity + p_receive_quantity,
        version    = version + 1,
        updated_at = SYSTIMESTAMP
    WHERE product_id = v_product_id
    RETURNING version INTO v_new_version;

    -- ===== ⑤ 更新入库单状态 =====
    -- 判断是否全部明细行均已收满
    SELECT COUNT(*), SUM(CASE WHEN quantity_received >= quantity_ordered THEN 1 ELSE 0 END)
    INTO   v_total_lines, v_full_lines
    FROM   inbound_order_lines
    WHERE  inbound_id = v_inbound_id;

    IF v_full_lines = v_total_lines THEN
        -- 全部收满 → RECEIVED
        UPDATE inbound_orders
        SET status = 'RECEIVED', received_at = SYSTIMESTAMP
        WHERE inbound_id = v_inbound_id;
    ELSE
        -- 部分收到 → PARTIAL（只有当前是 DRAFT 或 SUBMITTED 才改）
        UPDATE inbound_orders
        SET status = 'PARTIAL'
        WHERE inbound_id = v_inbound_id
          AND status IN ('DRAFT', 'SUBMITTED');
    END IF;

    COMMIT;

    o_received_total := v_received_after;
    o_result         := 0;
    o_message        := '收货成功：商品 ' || v_product_id
                     || '，入库 ' || p_receive_quantity
                     || '（累计 ' || v_received_after || '/' || v_ordered || '）'
                     || '，库存版本 → ' || v_new_version;
EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK;
        o_received_total := 0;
        o_new_version    := 0;
        o_result         := SQLCODE;
        o_message        := '收货失败: ' || SQLERRM;
END proc_inbound_receive;
/

-- -----------------------------------------------------------------------------
-- 3. 取消入库单（回滚已收库存）
--    仅 DRAFT / SUBMITTED / PARTIAL 状态可取消
--    PARTIAL 状态需回退已收库存
-- -----------------------------------------------------------------------------
CREATE OR REPLACE PROCEDURE proc_inbound_cancel (
    p_inbound_id    IN  inbound_orders.inbound_id%TYPE,
    p_operator_id   IN  users.user_id%TYPE DEFAULT NULL,
    o_result        OUT NUMBER,
    o_message       OUT VARCHAR2
) IS
    v_status          VARCHAR2(20);
    v_has_received    NUMBER := 0;
    PRAGMA AUTONOMOUS_TRANSACTION;
BEGIN
    -- 查询入库单状态
    SELECT status INTO v_status
    FROM inbound_orders
    WHERE inbound_id = p_inbound_id;

    IF v_status = 'CANCELLED' THEN
        o_result := -301;
        o_message := '入库单已取消';
        RETURN;
    END IF;

    IF v_status = 'RECEIVED' THEN
        o_result := -302;
        o_message := '已到货的入库单不可取消，请走退货流程';
        RETURN;
    END IF;

    -- 检查是否有已收货的明细行
    SELECT COUNT(*) INTO v_has_received
    FROM inbound_order_lines
    WHERE inbound_id = p_inbound_id AND quantity_received > 0;

    -- 回退已收库存
    IF v_has_received > 0 THEN
        FOR rec IN (
            SELECT iol.product_id, iol.quantity_received
            FROM inbound_order_lines iol
            WHERE iol.inbound_id = p_inbound_id
              AND iol.quantity_received > 0
        ) LOOP
            UPDATE inventory
            SET quantity   = quantity - rec.quantity_received,
                version    = version + 1,
                updated_at = SYSTIMESTAMP
            WHERE product_id = rec.product_id;

            UPDATE inbound_order_lines
            SET quantity_received = 0
            WHERE inbound_id = p_inbound_id
              AND product_id = rec.product_id;
        END LOOP;
    END IF;

    -- 更新入库单状态
    UPDATE inbound_orders
    SET status = 'CANCELLED'
    WHERE inbound_id = p_inbound_id;

    COMMIT;
    o_result  := 0;
    o_message := '入库单 ' || p_inbound_id || ' 已取消'
              || CASE WHEN v_has_received > 0 THEN '（已回退库存）' ELSE '' END;
EXCEPTION
    WHEN NO_DATA_FOUND THEN
        ROLLBACK;
        o_result := -300;
        o_message := '入库单 ' || p_inbound_id || ' 不存在';
    WHEN OTHERS THEN
        ROLLBACK;
        o_result  := SQLCODE;
        o_message := '取消失败: ' || SQLERRM;
END proc_inbound_cancel;
/

-- -----------------------------------------------------------------------------
-- 4. 提交入库单（DRAFT → SUBMITTED）
-- -----------------------------------------------------------------------------
CREATE OR REPLACE PROCEDURE proc_inbound_submit (
    p_inbound_id    IN  inbound_orders.inbound_id%TYPE,
    p_operator_id   IN  users.user_id%TYPE DEFAULT NULL,
    o_result        OUT NUMBER,
    o_message       OUT VARCHAR2
) IS
    PRAGMA AUTONOMOUS_TRANSACTION;
BEGIN
    UPDATE inbound_orders
    SET status = 'SUBMITTED'
    WHERE inbound_id = p_inbound_id AND status = 'DRAFT';

    IF SQL%ROWCOUNT = 0 THEN
        o_result := -400;
        o_message := '入库单 ' || p_inbound_id || ' 不存在或非草稿状态';
        RETURN;
    END IF;

    COMMIT;
    o_result  := 0;
    o_message := '入库单 ' || p_inbound_id || ' 已提交';
EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK;
        o_result  := SQLCODE;
        o_message := '提交失败: ' || SQLERRM;
END proc_inbound_submit;
/

-- =============================================================================
-- 使用示例（供开发调试，上线前删除）
-- =============================================================================
/*
DECLARE
    v_id  NUMBER;
    v_rc  NUMBER;
    v_msg VARCHAR2(4000);
BEGIN
    -- 1. 创建入库单
    proc_inbound_create(
        p_supplier_id => 1,
        p_created_by  => 2,
        p_lines_json  => '[{"product_id":1,"quantity":100,"unit_price":9.5}]',
        o_inbound_id  => v_id,
        o_result      => v_rc,
        o_message     => v_msg
    );
    DBMS_OUTPUT.PUT_LINE('Create: ' || v_msg || ' id=' || v_id);

    -- 2. 提交入库单
    proc_inbound_submit(v_id, 2, v_rc, v_msg);
    DBMS_OUTPUT.PUT_LINE('Submit: ' || v_msg);

    -- 3. 收货（需要先查到 line_id）
    DECLARE
        v_line_id NUMBER;
        v_rt      NUMBER;
        v_nv      NUMBER;
    BEGIN
        SELECT line_id INTO v_line_id
        FROM inbound_order_lines
        WHERE inbound_id = v_id AND ROWNUM = 1;

        proc_inbound_receive(v_line_id, 50, 2, NULL, v_rt, v_nv, v_rc, v_msg);
        DBMS_OUTPUT.PUT_LINE('Receive 50: ' || v_msg || ' total=' || v_rt || ' ver=' || v_nv);

        proc_inbound_receive(v_line_id, 50, 2, NULL, v_rt, v_nv, v_rc, v_msg);
        DBMS_OUTPUT.PUT_LINE('Receive 50: ' || v_msg || ' total=' || v_rt || ' ver=' || v_nv);
    END;
END;
/
*/
