# WMS — REST API 接口约定

> 版本：v1.0  
> 更新：2026-06-02  
> 协作参考：李佳恒（Service/Controller）、白沁禾（frontend）、李展鸿（DAO）

---

## 通用约定

| 项 | 约定 |
|----|------|
| Base URL | `http://localhost:8080/api/v1` |
| 请求格式 | `Content-Type: application/json` |
| 响应格式 | `{ "code": 0, "message": "ok", "data": {...} }` |
| 认证方式 | *(待定，暂用 operator_id 参数传递)* |
| 时间格式 | ISO 8601：`2026-06-02T14:30:00` |
| 分页参数 | `?limit=50&offset=0` |

### 响应结构

```json
{
  "code": 0,
  "message": "操作成功",
  "data": { }
}
```

- `code = 0`：成功
- `code < 0`：失败（错误码见 [错误码表](#错误码对照)）

---

## 1. 入库管理（Inbound）

### 1.1 创建入库单

```
POST /api/v1/inbound
```

**请求体：**

```json
{
  "supplier_id": 1,
  "created_by": 2,
  "lines": [
    { "product_id": 1, "quantity": 100, "unit_price": 9.50 },
    { "product_id": 2, "quantity": 50,  "unit_price": 12.00 }
  ]
}
```

**成功响应（201）：**

```json
{
  "code": 0,
  "message": "入库单 1 创建成功，共 2 条明细",
  "data": { "inbound_id": 1 }
}
```

**错误响应：**

| code | 说明 |
|------|------|
| -1 | supplier_id 不能为空 |
| -2 | lines 不能为空 |

---

### 1.2 提交入库单

```
POST /api/v1/inbound/{inbound_id}/submit
```

**请求体：**

```json
{ "operator_id": 2 }
```

**成功响应：**

```json
{
  "code": 0,
  "message": "入库单 1 已提交"
}
```

**错误响应：**

| code | 说明 |
|------|------|
| -400 | 入库单不存在或非 DRAFT 状态 |

---

### 1.3 收货确认（按明细行）

```
POST /api/v1/inbound/lines/{line_id}/receive
```

**请求体：**

```json
{
  "receive_quantity": 50,
  "operator_id": 2,
  "expected_version": null
}
```

> `expected_version`：乐观锁版本号。前端首次调用传 `null`，后续收货传上一次返回的 `new_version`。

**成功响应：**

```json
{
  "code": 0,
  "message": "收货成功：商品 1，入库 50（累计 50/100），库存版本 → 1",
  "data": {
    "received_total": 50,
    "new_version": 1
  }
}
```

**错误响应：**

| code | 说明 |
|------|------|
| -100 | 明细行不存在 |
| -101 | 入库单已取消 |
| -102 | 入库单已全部到货 |
| -103 | 收货量超出订购量 |
| -104 | 收货量 ≤ 0 |
| -200 | 乐观锁版本冲突（请刷新后重试） |

---

### 1.4 取消入库单

```
POST /api/v1/inbound/{inbound_id}/cancel
```

**请求体：**

```json
{ "operator_id": 2 }
```

**成功响应：**

```json
{
  "code": 0,
  "message": "入库单 1 已取消（已回退库存）"
}
```

**错误响应：**

| code | 说明 |
|------|------|
| -300 | 入库单不存在 |
| -301 | 入库单已取消 |
| -302 | 已到货的入库单不可取消 |

---

### 1.5 查询入库单详情

```
GET /api/v1/inbound/{inbound_id}
```

**成功响应：**

```json
{
  "code": 0,
  "data": {
    "inbound_id": 1,
    "supplier_id": 1,
    "status": "PARTIAL",
    "created_by": 2,
    "created_at": "2026-06-02T10:00:00",
    "received_at": null,
    "remark": "紧急补货",
    "lines": [
      {
        "line_id": 1,
        "product_id": 1,
        "quantity_ordered": 100,
        "quantity_received": 50,
        "unit_price": 9.50
      }
    ]
  }
}
```

> 不存在时返回 `{ "code": 0, "data": null }`

---

### 1.6 入库单列表

```
GET /api/v1/inbound?status={status}&supplier_id={sid}&limit=50&offset=0
```

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| status | string | 否 | DRAFT / SUBMITTED / PARTIAL / RECEIVED / CANCELLED |
| supplier_id | int | 否 | 供应商 ID |
| limit | int | 否 | 每页条数，默认 50 |
| offset | int | 否 | 偏移量，默认 0 |

**成功响应：**

```json
{
  "code": 0,
  "data": [
    {
      "inbound_id": 2,
      "supplier_id": 1,
      "status": "DRAFT",
      "created_by": 2,
      "created_at": "2026-06-02T11:00:00",
      "received_at": null,
      "remark": ""
    }
  ]
}
```

---

## 2. 库存查询（Inventory）

### 2.1 按商品查库存

```
GET /api/v1/inventory/{product_id}
```

**成功响应：**

```json
{
  "code": 0,
  "data": {
    "inventory_id": 1,
    "product_id": 1,
    "quantity": 150.0,
    "version": 2,
    "updated_at": "2026-06-02T10:30:00"
  }
}
```

> 不存在时返回 `{ "code": 0, "data": null }`

---

### 2.2 库存列表

```
GET /api/v1/inventory?limit=50&offset=0
```

**成功响应：**

```json
{
  "code": 0,
  "data": [
    {
      "inventory_id": 1,
      "product_id": 1,
      "quantity": 150.0,
      "version": 2,
      "updated_at": "2026-06-02T10:30:00"
    }
  ]
}
```

---

## 错误码对照

### 入库创建（-1 ~ -2）

| Code | 常量 | 说明 |
|------|------|------|
| -1 | SUPPLIER_ID_NULL | 供应商 ID 为空 |
| -2 | LINES_JSON_EMPTY | 入库明细为空 |

### 入库收货（-100 ~ -200）

| Code | 常量 | 说明 |
|------|------|------|
| -100 | LINE_NOT_FOUND | 入库明细行不存在 |
| -101 | ORDER_CANCELLED | 入库单已取消 |
| -102 | ORDER_RECEIVED | 入库单已全部到货 |
| -103 | QUANTITY_EXCEED | 收货量超出订购量 |
| -104 | QUANTITY_NEGATIVE | 收货量 ≤ 0 |
| -200 | VERSION_CONFLICT | 乐观锁版本冲突 |

### 入库取消（-300 ~ -302）

| Code | 常量 | 说明 |
|------|------|------|
| -300 | CANCEL_NOT_FOUND | 入库单不存在 |
| -301 | CANCEL_ALREADY | 入库单已取消 |
| -302 | CANCEL_RECEIVED | 已到货不可取消 |

### 入库提交（-400）

| Code | 常量 | 说明 |
|------|------|------|
| -400 | SUBMIT_NOT_DRAFT | 非草稿状态不可提交 |

---

## 协作要点

| 角色 | 关注点 |
|------|--------|
| **李佳恒（Service/Controller）** | `InventoryDAO.hpp` 的 `IInventoryDAO` 接口 → cpp-httplib handler 注册 |
| **白沁禾（frontend）** | `frontend/src/api/inbound.js` 按本文档封装 Axios 请求 |
| **李展鸿（DAO）** | `InventoryDAO.cpp` 已实现上述全部接口，可直接调用 |

### 前端 API 封装签名参考（inbound.js）

```js
// POST /api/v1/inbound
createInbound({ supplier_id, lines, created_by }) → { code, message, data: { inbound_id } }

// POST /api/v1/inbound/{id}/submit
submitInbound(id, { operator_id })                 → { code, message }

// POST /api/v1/inbound/lines/{line_id}/receive
receiveInbound(line_id, {                          → { code, message, data: { received_total, new_version } }
  receive_quantity, operator_id, expected_version
})

// POST /api/v1/inbound/{id}/cancel
cancelInbound(id, { operator_id })                 → { code, message }

// GET /api/v1/inbound/{id}
getInbound(id)                                     → { code, data: InboundOrder }

// GET /api/v1/inbound?status=&supplier_id=&limit=&offset=
listInbound(params)                                → { code, data: InboundOrder[] }

// GET /api/v1/inventory/{product_id}
getInventory(product_id)                           → { code, data: InventoryRecord }

// GET /api/v1/inventory?limit=&offset=
listInventory(params)                              → { code, data: InventoryRecord[] }
```
