// =============================================================================
// inbound.js — 入库模块 API 封装（李展鸿 scope）
//
// 对接后端 /api/inventory/inbound 系列接口
// 响应格式：{ code: 0, message: "ok", data: {...} }
// 参考：docs/API.md
// =============================================================================

import request, { isMockEnabled, mockWait } from './request';

// ---- 状态常量 ----
export const STATUS = {
  DRAFT: 'DRAFT',
  SUBMITTED: 'SUBMITTED',
  PARTIAL: 'PARTIAL',
  RECEIVED: 'RECEIVED',
  CANCELLED: 'CANCELLED'
};

export const STATUS_LABEL = {
  [STATUS.DRAFT]: '草稿',
  [STATUS.SUBMITTED]: '已提交',
  [STATUS.PARTIAL]: '部分收货',
  [STATUS.RECEIVED]: '已收货',
  [STATUS.CANCELLED]: '已取消'
};

// ---- Mock 数据 ----
let nextId = 100;
let nextLineId = 1000;

const makeLine = (productId, qty, received = 0) => ({
  lineId: nextLineId++,
  productId,
  quantityOrdered: qty,
  quantityReceived: received,
  unitPrice: +(Math.random() * 50 + 5).toFixed(2)
});

let mockOrders = [
  {
    inboundId: 1,
    supplierId: 1,
    supplierName: '华为技术有限公司',
    status: STATUS.PARTIAL,
    createdBy: 2,
    createdAt: '2026-06-02T10:00:00',
    receivedAt: null,
    remark: '紧急补货',
    lines: [makeLine(1001, 100, 50), makeLine(1002, 200, 0)]
  },
  {
    inboundId: 2,
    supplierId: 2,
    supplierName: '中兴通讯股份有限公司',
    status: STATUS.DRAFT,
    createdBy: 2,
    createdAt: '2026-06-03T14:30:00',
    receivedAt: null,
    remark: '',
    lines: [makeLine(1003, 80, 0)]
  },
  {
    inboundId: 3,
    supplierId: 3,
    supplierName: '小米供应链管理有限公司',
    status: STATUS.RECEIVED,
    createdBy: 2,
    createdAt: '2026-06-01T09:00:00',
    receivedAt: '2026-06-02T16:00:00',
    remark: '已完成',
    lines: [makeLine(1001, 50, 50)]
  },
  {
    inboundId: 4,
    supplierId: 4,
    supplierName: '京东物流供应商',
    status: STATUS.CANCELLED,
    createdBy: 2,
    createdAt: '2026-06-04T11:00:00',
    receivedAt: null,
    remark: '供应商取消',
    lines: [makeLine(1002, 120, 0)]
  },
  {
    inboundId: 5,
    supplierId: 1,
    supplierName: '华为技术有限公司',
    status: STATUS.SUBMITTED,
    createdBy: 2,
    createdAt: '2026-06-10T08:00:00',
    receivedAt: null,
    remark: 'SKU收货演示订单',
    lines: [makeLine(1001, 60, 0), makeLine(1003, 150, 0)]
  }
];

// ---- 1. 入库单列表 ----
export async function listInboundOrders(params = {}) {
  if (isMockEnabled) {
    let filtered = [...mockOrders];

    if (params.status) {
      filtered = filtered.filter((o) => o.status === params.status);
    }
    if (params.supplierId) {
      filtered = filtered.filter((o) => o.supplierId === Number(params.supplierId));
    }

    const limit = Number(params.limit) || 50;
    const offset = Number(params.offset) || 0;
    const page = offset / limit + 1;

    const list = filtered.slice(offset, offset + limit);

    return mockWait({
      code: 0,
      message: 'ok',
      data: { list, total: filtered.length, page, pageSize: limit }
    });
  }

  return request.get('/inventory/inbound', { params });
}

// ---- 2. 入库单详情 ----
export async function getInboundOrder(id) {
  if (isMockEnabled) {
    const order = mockOrders.find((o) => o.inboundId === Number(id)) || null;
    return mockWait({ code: 0, message: 'ok', data: order });
  }

  return request.get(`/inventory/inbound/${id}`);
}

// ---- 3. 创建入库单 ----
// 兼容两种调用方式：
//   a) Dashboard 快捷入库：{ warehouseCode, skuCode, quantity }
//   b) OrderForm 完整创建：{ supplierId, lines, createdBy, remark }
export async function createInboundOrder(payload) {
  if (isMockEnabled) {
    let lines;

    if (payload.lines) {
      // 完整格式（OrderForm）
      lines = payload.lines.map((l) => makeLine(l.productId, l.quantity, 0));
    } else {
      // 快捷格式（Dashboard）
      lines = [makeLine(Number(payload.skuCode) || payload.skuCode, payload.quantity || 1, 0)];
    }

    const order = {
      inboundId: nextId++,
      supplierId: payload.supplierId || 1,
      supplierName: ['华为技术有限公司', '中兴通讯股份有限公司', '小米供应链管理有限公司', '京东物流供应商'][(payload.supplierId || 1) - 1] || '未知供应商',
      status: STATUS.DRAFT,
      createdBy: payload.createdBy || 1,
      createdAt: new Date().toISOString().replace('T', ' ').slice(0, 19),
      receivedAt: null,
      remark: payload.remark || '',
      lines
    };

    mockOrders.unshift(order);

    return mockWait({
      code: 0,
      message: `入库单 ${order.inboundId} 创建成功，共 ${lines.length} 条明细`,
      data: { inboundId: order.inboundId }
    });
  }

  return request.post('/inventory/inbound', payload);
}

// ---- 4. 提交入库单 ----
export async function submitInboundOrder(id) {
  if (isMockEnabled) {
    const order = mockOrders.find((o) => o.inboundId === Number(id));
    if (!order) {
      return mockWait({ code: -400, message: '入库单不存在' });
    }
    if (order.status !== STATUS.DRAFT) {
      return mockWait({ code: -400, message: '非草稿状态不可提交' });
    }
    order.status = STATUS.SUBMITTED;
    return mockWait({ code: 0, message: `入库单 ${id} 已提交` });
  }

  return request.post(`/inventory/inbound/${id}/submit`);
}

// ---- 5. 收货确认 ----
export async function receiveInbound({ lineId, receiveQuantity }) {
  if (isMockEnabled) {
    for (const order of mockOrders) {
      const line = order.lines?.find((l) => l.lineId === Number(lineId));
      if (!line) continue;

      if (order.status === STATUS.CANCELLED) {
        return mockWait({ code: -101, message: '入库单已取消' });
      }
      if (line.quantityReceived >= line.quantityOrdered) {
        return mockWait({ code: -102, message: '该明细已全部到货' });
      }
      if (receiveQuantity <= 0 || receiveQuantity > line.quantityOrdered - line.quantityReceived) {
        return mockWait({ code: -103, message: '收货量超出订购量' });
      }

      line.quantityReceived += Number(receiveQuantity);

      // 更新订单状态
      const allReceived = order.lines.every((l) => l.quantityReceived >= l.quantityOrdered);
      order.status = allReceived ? STATUS.RECEIVED : STATUS.PARTIAL;
      if (allReceived) order.receivedAt = new Date().toISOString().replace('T', ' ').slice(0, 19);

      return mockWait({
        code: 0,
        message: `收货成功：商品 ${line.productId}，累计 ${line.quantityReceived}/${line.quantityOrdered}`,
        data: { receivedTotal: line.quantityReceived, newVersion: 1 }
      });
    }

    return mockWait({ code: -100, message: '明细行不存在' });
  }

  return request.post(`/inventory/inbound/lines/${lineId}/receive`, { receiveQuantity });
}

// ---- 6. 取消入库单 ----
export async function cancelInboundOrder(id) {
  if (isMockEnabled) {
    const order = mockOrders.find((o) => o.inboundId === Number(id));
    if (!order) {
      return mockWait({ code: -300, message: '入库单不存在' });
    }
    if (order.status === STATUS.CANCELLED) {
      return mockWait({ code: -301, message: '入库单已取消' });
    }
    if (order.status === STATUS.RECEIVED) {
      return mockWait({ code: -302, message: '已到货的入库单不可取消' });
    }
    order.status = STATUS.CANCELLED;
    return mockWait({ code: 0, message: `入库单 ${id} 已取消（已回退库存）` });
  }

  return request.post(`/inventory/inbound/${id}/cancel`);
}

// ---- 7. SKU 收货（快速扫码入库） ----
export async function receiveBySku(payload) {
  if (isMockEnabled) {
    const { skuCode, productId, quantity } = payload;

    // 解析 productId
    let pid = productId || 0;
    if (!pid && skuCode) {
      const num = Number(skuCode);
      pid = Number.isNaN(num) ? 0 : num;
    }

    // 查找待收货的明细行（FIFO）
    let remaining = quantity || 0;
    let totalReceived = 0;
    const affectedOrders = [];

    for (const order of mockOrders) {
      if (remaining <= 0) break;
      if (order.status !== STATUS.SUBMITTED && order.status !== STATUS.PARTIAL) continue;

      for (const line of order.lines || []) {
        if (remaining <= 0) break;
        if (line.productId !== pid) continue;
        if (line.quantityReceived >= line.quantityOrdered) continue;

        const canReceive = line.quantityOrdered - line.quantityReceived;
        const toReceive = Math.min(remaining, canReceive);
        line.quantityReceived += toReceive;
        totalReceived += toReceive;
        remaining -= toReceive;

        if (!affectedOrders.includes(order.inboundId)) {
          affectedOrders.push(order.inboundId);
        }
      }

      // 更新订单状态
      const allReceived = (order.lines || []).every(l => l.quantityReceived >= l.quantityOrdered);
      order.status = allReceived ? STATUS.RECEIVED : STATUS.PARTIAL;
      if (allReceived) order.receivedAt = new Date().toISOString().replace('T', ' ').slice(0, 19);
    }

    if (totalReceived === 0) {
      return mockWait({
        code: 0,
        message: '没有找到该商品对应的待收货入库单（需要先创建并提交采购入库单）',
        data: { success: false, totalReceived: 0, orderIds: [] }
      });
    }

    let msg = `收货成功：共 ${totalReceived} 件`;
    if (remaining > 0) {
      msg = `部分收货成功：已收 ${totalReceived}，剩余 ${remaining} 无可收明细`;
    }

    return mockWait({
      code: 0,
      message: msg,
      data: { success: true, totalReceived, orderIds: affectedOrders }
    });
  }

  return request.post('/inventory/inbound/receive-by-sku', payload);
}

// ---- 兼容旧版 Inbound.vue（后续可移除） ----
export async function getInboundOrders(params = {}) {
  if (isMockEnabled) {
    const result = await listInboundOrders(params);
    // 兼容旧字段名
    const list = result.data.list.map((o) => ({
      id: o.inboundId,
      orderNo: `IN-${String(o.inboundId).padStart(3, '0')}`,
      skuCode: o.lines?.[0]?.productId ? `SKU-${o.lines[0].productId}` : '-',
      supplierName: o.supplierName || '-',
      quantity: o.lines?.reduce((sum, l) => sum + l.quantityOrdered, 0) || 0,
      status: o.status === STATUS.DRAFT || o.status === STATUS.SUBMITTED ? 'pending'
        : o.status === STATUS.RECEIVED || o.status === STATUS.PARTIAL ? 'arrived'
        : 'cancelled',
      createdAt: o.createdAt?.slice(0, 10) || ''
    }));
    return mockWait({ list, total: list.length });
  }

  return request.get('/inventory/inbound', { params });
}

export default {
  STATUS,
  STATUS_LABEL,
  listInboundOrders,
  getInboundOrder,
  createInboundOrder,
  submitInboundOrder,
  receiveInbound,
  cancelInboundOrder,
  receiveBySku,
  getInboundOrders
};
