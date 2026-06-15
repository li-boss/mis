-- =============================================================================
-- WMS 供应链智能仓储管理系统 — 建表脚本 (Oracle)
-- 依据 ER 图 + 任务分配方案确认项：
--   - 保留销售订单 orders / order_details
--   - 新增 inbound_orders / inbound_order_lines（采购入库）
--   - 新增 audit_logs（库存审计）
--   - 库存独立 inventory 表（products 不含 stock）
--   - 暂不建 addresses
--   - users 含 role 字段
-- =============================================================================

-- -----------------------------------------------------------------------------
-- 1. 用户表
-- -----------------------------------------------------------------------------
CREATE TABLE users (
    user_id       NUMBER(10)       NOT NULL,
    username      VARCHAR2(64)     NOT NULL,
    password_hash VARCHAR2(256)    NOT NULL,
    real_name     VARCHAR2(64),
    phone         VARCHAR2(20),
    role          VARCHAR2(20)     NOT NULL,
    created_at    TIMESTAMP        DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_users PRIMARY KEY (user_id),
    CONSTRAINT uk_users_username UNIQUE (username),
    CONSTRAINT ck_users_role CHECK (role IN ('admin', 'keeper', 'purchaser', 'data_manager',
                                              'ADMIN', 'OPERATOR', 'CUSTOMER'))
);

COMMENT ON TABLE users IS '系统用户（含管理员、操作员、客户）';
COMMENT ON COLUMN users.role IS 'ADMIN=管理员, OPERATOR=仓储操作员, CUSTOMER=客户';

-- -----------------------------------------------------------------------------
-- 2. 分类表（支持父子分类）
-- -----------------------------------------------------------------------------
CREATE TABLE categories (
    category_id        NUMBER(10)    NOT NULL,
    category_name      VARCHAR2(100) NOT NULL,
    parent_category_id NUMBER(10),
    CONSTRAINT pk_categories PRIMARY KEY (category_id),
    CONSTRAINT fk_categories_parent
        FOREIGN KEY (parent_category_id) REFERENCES categories (category_id)
);

COMMENT ON TABLE categories IS '商品分类（树形结构）';

-- -----------------------------------------------------------------------------
-- 3. 商品表（SKU 主数据，库存见 inventory）
-- -----------------------------------------------------------------------------
CREATE TABLE products (
    product_id    NUMBER(10)     NOT NULL,
    product_name  VARCHAR2(200)  NOT NULL,
    sku_code      VARCHAR2(64),
    category_id   NUMBER(10)     NOT NULL,
    unit          VARCHAR2(20)   DEFAULT '件',
    unit_price    NUMBER(12, 2)  DEFAULT 0 NOT NULL,
    status        VARCHAR2(20)   DEFAULT 'active' NOT NULL,
    created_at    TIMESTAMP      DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_products PRIMARY KEY (product_id),
    CONSTRAINT fk_products_category
        FOREIGN KEY (category_id) REFERENCES categories (category_id),
    CONSTRAINT ck_products_status CHECK (status IN ('active', 'disabled'))
);

COMMENT ON TABLE products IS '商品/SKU 主数据';

CREATE INDEX idx_products_category ON products (category_id);

-- -----------------------------------------------------------------------------
-- 3.5. 仓库表
-- -----------------------------------------------------------------------------
CREATE TABLE warehouses (
    warehouse_id   NUMBER(10)    NOT NULL,
    warehouse_code VARCHAR2(20)  NOT NULL,
    warehouse_name VARCHAR2(100) NOT NULL,
    address        VARCHAR2(200),
    status         VARCHAR2(20)  DEFAULT 'active' NOT NULL,
    created_at     TIMESTAMP     DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_warehouses PRIMARY KEY (warehouse_id),
    CONSTRAINT uk_warehouses_code UNIQUE (warehouse_code),
    CONSTRAINT ck_warehouses_status CHECK (status IN ('active', 'disabled'))
);

-- -----------------------------------------------------------------------------
-- 4. 库存表（独立管理，支持乐观锁）
-- -----------------------------------------------------------------------------
CREATE TABLE inventory (
    inventory_id NUMBER(10)    NOT NULL,
    product_id   NUMBER(10)    NOT NULL,
    warehouse_id NUMBER(10)    DEFAULT 1 NOT NULL,
    quantity     NUMBER(12, 3) DEFAULT 0 NOT NULL,
    safety_stock NUMBER(10)    DEFAULT 0,
    version      NUMBER(10)    DEFAULT 0 NOT NULL,
    updated_at   TIMESTAMP     DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_inventory PRIMARY KEY (inventory_id),
    CONSTRAINT uk_inventory_product_wh UNIQUE (product_id, warehouse_id),
    CONSTRAINT fk_inventory_product
        FOREIGN KEY (product_id) REFERENCES products (product_id),
    CONSTRAINT fk_inventory_warehouse
        FOREIGN KEY (warehouse_id) REFERENCES warehouses (warehouse_id),
    CONSTRAINT ck_inventory_quantity CHECK (quantity >= 0)
);

COMMENT ON TABLE inventory IS '商品库存（与 products 1:1）';
COMMENT ON COLUMN inventory.version IS '乐观锁版本号，入库/出库时递增';

-- -----------------------------------------------------------------------------
-- 5. 供应商表
-- -----------------------------------------------------------------------------
CREATE TABLE suppliers (
    supplier_id   NUMBER(10)    NOT NULL,
    supplier_name VARCHAR2(200) NOT NULL,
    supplier_code VARCHAR2(64),
    contact_name  VARCHAR2(64),
    contact_phone VARCHAR2(20),
    rating        VARCHAR2(10)  DEFAULT 'B',
    status        VARCHAR2(20)  DEFAULT 'active' NOT NULL,
    address       VARCHAR2(200),
    remark        VARCHAR2(500),
    CONSTRAINT pk_suppliers PRIMARY KEY (supplier_id),
    CONSTRAINT ck_suppliers_status CHECK (status IN ('active', 'paused'))
);

COMMENT ON TABLE suppliers IS '供应商';

-- -----------------------------------------------------------------------------
-- 6. 商品-供应商关联表（多对多 + 供货价）
-- -----------------------------------------------------------------------------
CREATE TABLE product_suppliers (
    ps_id         NUMBER(10)    NOT NULL,
    product_id    NUMBER(10)    NOT NULL,
    supplier_id   NUMBER(10)    NOT NULL,
    supply_price  NUMBER(12, 2) NOT NULL,
    CONSTRAINT pk_product_suppliers PRIMARY KEY (ps_id),
    CONSTRAINT uk_product_suppliers UNIQUE (product_id, supplier_id),
    CONSTRAINT fk_ps_product
        FOREIGN KEY (product_id) REFERENCES products (product_id),
    CONSTRAINT fk_ps_supplier
        FOREIGN KEY (supplier_id) REFERENCES suppliers (supplier_id),
    CONSTRAINT ck_ps_supply_price CHECK (supply_price >= 0)
);

COMMENT ON TABLE product_suppliers IS '商品与供应商供货关系';

CREATE INDEX idx_ps_supplier ON product_suppliers (supplier_id);

-- -----------------------------------------------------------------------------
-- 7. 销售订单表（ER 图订单，关联客户用户）
-- -----------------------------------------------------------------------------
CREATE TABLE orders (
    order_id    NUMBER(10)    NOT NULL,
    user_id     NUMBER(10)    NOT NULL,
    total_price NUMBER(14, 2) DEFAULT 0 NOT NULL,
    status      VARCHAR2(20)  NOT NULL,
    order_time  TIMESTAMP     DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_orders PRIMARY KEY (order_id),
    CONSTRAINT fk_orders_user
        FOREIGN KEY (user_id) REFERENCES users (user_id),
    CONSTRAINT ck_orders_status CHECK (
        status IN ('PENDING', 'PAID', 'SHIPPED', 'COMPLETED', 'CANCELLED')
    )
);

COMMENT ON TABLE orders IS '销售订单（面向 CUSTOMER 用户）';

CREATE INDEX idx_orders_user ON orders (user_id);
CREATE INDEX idx_orders_status ON orders (status);

-- -----------------------------------------------------------------------------
-- 8. 销售订单明细表
-- -----------------------------------------------------------------------------
CREATE TABLE order_details (
    detail_id  NUMBER(10)   NOT NULL,
    order_id   NUMBER(10)   NOT NULL,
    product_id NUMBER(10)   NOT NULL,
    quantity   NUMBER(12, 3) NOT NULL,
    unit_price NUMBER(12, 2) NOT NULL,
    CONSTRAINT pk_order_details PRIMARY KEY (detail_id),
    CONSTRAINT fk_od_order
        FOREIGN KEY (order_id) REFERENCES orders (order_id),
    CONSTRAINT fk_od_product
        FOREIGN KEY (product_id) REFERENCES products (product_id),
    CONSTRAINT ck_od_quantity CHECK (quantity > 0)
);

COMMENT ON TABLE order_details IS '销售订单明细';

CREATE INDEX idx_od_order ON order_details (order_id);
CREATE INDEX idx_od_product ON order_details (product_id);

-- -----------------------------------------------------------------------------
-- 9. 采购入库单（关联供应商，区别于销售 orders）
-- -----------------------------------------------------------------------------
CREATE TABLE inbound_orders (
    inbound_id   NUMBER(10)   NOT NULL,
    supplier_id  NUMBER(10)   NOT NULL,
    status       VARCHAR2(20) NOT NULL,
    created_by   NUMBER(10),
    created_at   TIMESTAMP    DEFAULT SYSTIMESTAMP NOT NULL,
    received_at  TIMESTAMP,
    remark       VARCHAR2(500),
    CONSTRAINT pk_inbound_orders PRIMARY KEY (inbound_id),
    CONSTRAINT fk_inbound_supplier
        FOREIGN KEY (supplier_id) REFERENCES suppliers (supplier_id),
    CONSTRAINT fk_inbound_created_by
        FOREIGN KEY (created_by) REFERENCES users (user_id),
    CONSTRAINT ck_inbound_status CHECK (
        status IN ('DRAFT', 'SUBMITTED', 'PARTIAL', 'RECEIVED', 'CANCELLED')
    )
);

COMMENT ON TABLE inbound_orders IS '采购入库单';
COMMENT ON COLUMN inbound_orders.status IS 'DRAFT→SUBMITTED→PARTIAL/RECEIVED';

CREATE INDEX idx_inbound_supplier ON inbound_orders (supplier_id);
CREATE INDEX idx_inbound_status ON inbound_orders (status);

-- -----------------------------------------------------------------------------
-- 10. 采购入库明细
-- -----------------------------------------------------------------------------
CREATE TABLE inbound_order_lines (
    line_id            NUMBER(10)    NOT NULL,
    inbound_id         NUMBER(10)    NOT NULL,
    product_id         NUMBER(10)    NOT NULL,
    quantity_ordered   NUMBER(12, 3) NOT NULL,
    quantity_received  NUMBER(12, 3) DEFAULT 0 NOT NULL,
    unit_price         NUMBER(12, 2),
    CONSTRAINT pk_inbound_order_lines PRIMARY KEY (line_id),
    CONSTRAINT fk_iol_inbound
        FOREIGN KEY (inbound_id) REFERENCES inbound_orders (inbound_id),
    CONSTRAINT fk_iol_product
        FOREIGN KEY (product_id) REFERENCES products (product_id),
    CONSTRAINT ck_iol_ordered CHECK (quantity_ordered > 0),
    CONSTRAINT ck_iol_received CHECK (quantity_received >= 0)
);

COMMENT ON TABLE inbound_order_lines IS '采购入库明细行';

CREATE INDEX idx_iol_inbound ON inbound_order_lines (inbound_id);

-- -----------------------------------------------------------------------------
-- 11. 审计日志表（配合 trg_stock_audit 触发器）
-- -----------------------------------------------------------------------------
CREATE TABLE audit_logs (
    log_id      NUMBER(19)    NOT NULL,
    table_name  VARCHAR2(64)  NOT NULL,
    record_id   NUMBER(19)    NOT NULL,
    action      VARCHAR2(10)  NOT NULL,
    old_value   CLOB,
    new_value   CLOB,
    changed_by  NUMBER(10),
    changed_at  TIMESTAMP     DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_audit_logs PRIMARY KEY (log_id),
    CONSTRAINT fk_audit_changed_by
        FOREIGN KEY (changed_by) REFERENCES users (user_id),
    CONSTRAINT ck_audit_action CHECK (action IN ('INSERT', 'UPDATE', 'DELETE'))
);

COMMENT ON TABLE audit_logs IS '数据变更审计日志（库存变更由触发器写入）';

CREATE INDEX idx_audit_table_record ON audit_logs (table_name, record_id);
CREATE INDEX idx_audit_changed_at ON audit_logs (changed_at);
