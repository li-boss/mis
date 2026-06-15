-- =============================================================================
-- WMS — PowerDesigner 逆向工程专用脚本 (Oracle 19c)
-- 用法: File > Reverse Engineer > Database > Using script files
-- 说明: 已去除 COMMENT ON，保留表/约束/索引，便于 PD 解析
-- =============================================================================

CREATE TABLE users (
    user_id       NUMBER(10)       NOT NULL,
    username      VARCHAR2(64)     NOT NULL,
    password_hash VARCHAR2(256)    NOT NULL,
    phone         VARCHAR2(20),
    role          VARCHAR2(20)     NOT NULL,
    created_at    TIMESTAMP        DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_users PRIMARY KEY (user_id),
    CONSTRAINT uk_users_username UNIQUE (username),
    CONSTRAINT ck_users_role CHECK (role IN ('ADMIN', 'OPERATOR', 'CUSTOMER'))
);

CREATE TABLE categories (
    category_id        NUMBER(10)    NOT NULL,
    category_name      VARCHAR2(100) NOT NULL,
    parent_category_id NUMBER(10),
    CONSTRAINT pk_categories PRIMARY KEY (category_id),
    CONSTRAINT fk_categories_parent
        FOREIGN KEY (parent_category_id) REFERENCES categories (category_id)
);

CREATE TABLE products (
    product_id    NUMBER(10)     NOT NULL,
    product_name  VARCHAR2(200)  NOT NULL,
    category_id   NUMBER(10)     NOT NULL,
    unit_price    NUMBER(12, 2)  DEFAULT 0 NOT NULL,
    created_at    TIMESTAMP      DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_products PRIMARY KEY (product_id),
    CONSTRAINT fk_products_category
        FOREIGN KEY (category_id) REFERENCES categories (category_id)
);

CREATE INDEX idx_products_category ON products (category_id);

CREATE TABLE inventory (
    inventory_id NUMBER(10)    NOT NULL,
    product_id   NUMBER(10)    NOT NULL,
    quantity     NUMBER(12, 3) DEFAULT 0 NOT NULL,
    version      NUMBER(10)    DEFAULT 0 NOT NULL,
    updated_at   TIMESTAMP     DEFAULT SYSTIMESTAMP NOT NULL,
    CONSTRAINT pk_inventory PRIMARY KEY (inventory_id),
    CONSTRAINT uk_inventory_product UNIQUE (product_id),
    CONSTRAINT fk_inventory_product
        FOREIGN KEY (product_id) REFERENCES products (product_id),
    CONSTRAINT ck_inventory_quantity CHECK (quantity >= 0)
);

CREATE TABLE suppliers (
    supplier_id   NUMBER(10)    NOT NULL,
    supplier_name VARCHAR2(200) NOT NULL,
    contact_name  VARCHAR2(64),
    contact_phone VARCHAR2(20),
    CONSTRAINT pk_suppliers PRIMARY KEY (supplier_id)
);

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

CREATE INDEX idx_ps_supplier ON product_suppliers (supplier_id);

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

CREATE INDEX idx_orders_user ON orders (user_id);
CREATE INDEX idx_orders_status ON orders (status);

CREATE TABLE order_details (
    detail_id  NUMBER(10)    NOT NULL,
    order_id   NUMBER(10)    NOT NULL,
    product_id NUMBER(10)    NOT NULL,
    quantity   NUMBER(12, 3) NOT NULL,
    unit_price NUMBER(12, 2) NOT NULL,
    CONSTRAINT pk_order_details PRIMARY KEY (detail_id),
    CONSTRAINT fk_od_order
        FOREIGN KEY (order_id) REFERENCES orders (order_id),
    CONSTRAINT fk_od_product
        FOREIGN KEY (product_id) REFERENCES products (product_id),
    CONSTRAINT ck_od_quantity CHECK (quantity > 0)
);

CREATE INDEX idx_od_order ON order_details (order_id);
CREATE INDEX idx_od_product ON order_details (product_id);

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

CREATE INDEX idx_inbound_supplier ON inbound_orders (supplier_id);
CREATE INDEX idx_inbound_status ON inbound_orders (status);

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

CREATE INDEX idx_iol_inbound ON inbound_order_lines (inbound_id);

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

CREATE INDEX idx_audit_table_record ON audit_logs (table_name, record_id);
CREATE INDEX idx_audit_changed_at ON audit_logs (changed_at);
