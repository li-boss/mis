# PowerDesigner 导入说明

WMS 数据库模型可视化文件位于本目录：

| 文件 | 用途 |
|------|------|
| `WMS.pdm` | **直接打开**（推荐）— 含 11 张表、外键关系、ER 图布局 |
| `tables_powerdesigner.sql` | **逆向工程** — 从 SQL 脚本生成/更新 PDM |
| `tables.sql` | 完整 Oracle 建表脚本（含 COMMENT） |
| `sequences.sql` | 序列定义 |

---

## 方式一：直接打开 PDM（推荐）

1. 启动 PowerDesigner（建议 16.x，与 `ORACLE Version 19c` 模板兼容）
2. **File → Open**
3. 选择 `database/schema/WMS.pdm`
4. 左侧展开 **PhysicalDiagram → WMS_ER** 查看 ER 图
5. 若连线重叠：**Layout → Automatic Layout** 重新排版

---

## 方式二：从 SQL 逆向工程

适用于需要与最新 `tables.sql` 同步时：

1. **File → Reverse Engineer → Database…**
2. Model name：`WMS`，DBMS 选 **Oracle 19c**（或 11g/12c）
3. 在 **Selection** 页选 **Using script files**
4. 添加 `tables_powerdesigner.sql`
5. 点击 **OK** 生成 PDM

> 使用 `tables_powerdesigner.sql` 而非 `tables.sql`，因已去除 `COMMENT ON`，PD 解析更稳定。

---

## 模型内容

- **11 张表**：users, categories, products, inventory, suppliers, product_suppliers, orders, order_details, inbound_orders, inbound_order_lines, audit_logs
- **13 条外键**（含 categories 自关联）
- **11 个序列**（SEQ_*）
- **未包含**：addresses（按设计暂不建）

---

## 重新生成 PDM

修改表结构后，可运行：

```bash
python scripts/generate_wms_pdm.py
```

脚本基于 `D:\develop\dbex\PhysicalDataModel_1.pdm` 模板生成 `WMS.pdm`。

---

*生成日期：2026-05-31*
