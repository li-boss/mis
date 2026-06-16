-- =============================================================================
-- WMS — 种子数据
-- 在 tables.sql + sequences.sql 执行后运行
-- =============================================================================

-- 1. 用户（初始密码 123456，演示用明文）
INSERT INTO users (user_id, username, password_hash, real_name, phone, role)
VALUES (seq_users.NEXTVAL, 'admin', '123456', '管理员', '13800000000', 'admin');

INSERT INTO users (user_id, username, password_hash, real_name, phone, role)
VALUES (seq_users.NEXTVAL, 'keeper', '123456', '库管员', '13800000001', 'keeper');

INSERT INTO users (user_id, username, password_hash, real_name, phone, role)
VALUES (seq_users.NEXTVAL, 'purchaser', '123456', '采购员', '13800000002', 'purchaser');

INSERT INTO users (user_id, username, password_hash, real_name, phone, role)
VALUES (seq_users.NEXTVAL, 'data_manager', '123456', '数据管理员', '13800000003', 'data_manager');

-- 2. 商品分类
INSERT INTO categories (category_id, category_name) VALUES (seq_categories.NEXTVAL, '设备');
INSERT INTO categories (category_id, category_name) VALUES (seq_categories.NEXTVAL, '耗材');
INSERT INTO categories (category_id, category_name) VALUES (seq_categories.NEXTVAL, '仓储');

-- 3. 商品/SKU（通过子查询获取 category_id）
INSERT INTO products (product_id, product_name, sku_code, category_id, unit, unit_price, status)
VALUES (1001, '手持扫码终端', 'SKU-RF-001',
    (SELECT category_id FROM categories WHERE category_name = '设备'), '台', 1500.00, 'active');

INSERT INTO products (product_id, product_name, sku_code, category_id, unit, unit_price, status)
VALUES (1002, '标准周转箱', 'SKU-PK-018',
    (SELECT category_id FROM categories WHERE category_name = '耗材'), '箱', 35.00, 'active');

INSERT INTO products (product_id, product_name, sku_code, category_id, unit, unit_price, status)
VALUES (1003, '防水标签纸', 'SKU-LB-206',
    (SELECT category_id FROM categories WHERE category_name = '耗材'), '卷', 12.50, 'active');

-- 4. 供应商
INSERT INTO suppliers (supplier_id, supplier_name, supplier_code, contact_name, contact_phone, rating, status, address, remark)
VALUES (seq_suppliers.NEXTVAL, '华为技术有限公司', 'SUP-HD-001', '张经理', '13900000001', 'A', 'active', '深圳市龙岗区', '设备类长期合作');

INSERT INTO suppliers (supplier_id, supplier_name, supplier_code, contact_name, contact_phone, rating, status, address, remark)
VALUES (seq_suppliers.NEXTVAL, '中兴通讯股份有限公司', 'SUP-ZX-002', '李经理', '13900000002', 'B', 'active', '深圳市南山区', '通讯设备');

INSERT INTO suppliers (supplier_id, supplier_name, supplier_code, contact_name, contact_phone, rating, status, address, remark)
VALUES (seq_suppliers.NEXTVAL, '小米供应链管理有限公司', 'SUP-XM-003', '王经理', '13900000003', 'B', 'active', '北京市海淀区', '');

INSERT INTO suppliers (supplier_id, supplier_name, supplier_code, contact_name, contact_phone, rating, status, address, remark)
VALUES (seq_suppliers.NEXTVAL, '京东物流供应商', 'SUP-JD-004', '赵经理', '13900000004', 'A', 'active', '北京市朝阳区', '物流配送');

-- 5. 仓库
INSERT INTO warehouses (warehouse_id, warehouse_code, warehouse_name, address, status)
VALUES (seq_warehouses.NEXTVAL, 'DEFAULT', '默认主仓库', '深圳市龙岗区', 'active');
INSERT INTO warehouses (warehouse_id, warehouse_code, warehouse_name, address, status)
VALUES (seq_warehouses.NEXTVAL, 'SH-HUB', '上海分仓', '上海市松江区', 'active');
INSERT INTO warehouses (warehouse_id, warehouse_code, warehouse_name, address, status)
VALUES (seq_warehouses.NEXTVAL, 'BJ-HUB', '北京分仓', '北京市顺义区', 'active');

-- 6. 库存（3 仓库 × 3 SKU）
-- DEFAULT 主仓库
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1001, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'DEFAULT'), 2, 30, 1, SYSTIMESTAMP);
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1002, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'DEFAULT'), 6, 40, 1, SYSTIMESTAMP);
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1003, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'DEFAULT'), 10, 120, 1, SYSTIMESTAMP);

-- 上海分仓
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1001, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'SH-HUB'), 50, 30, 1, SYSTIMESTAMP);
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1002, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'SH-HUB'), 35, 40, 1, SYSTIMESTAMP);
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1003, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'SH-HUB'), 0, 120, 1, SYSTIMESTAMP);

-- 北京分仓
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1001, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'BJ-HUB'), 15, 30, 1, SYSTIMESTAMP);
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1002, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'BJ-HUB'), 8, 40, 1, SYSTIMESTAMP);
INSERT INTO inventory (inventory_id, product_id, warehouse_id, quantity, safety_stock, version, updated_at)
VALUES (seq_inventory.NEXTVAL, 1003, (SELECT warehouse_id FROM warehouses WHERE warehouse_code = 'BJ-HUB'), 25, 120, 1, SYSTIMESTAMP);

COMMIT;
