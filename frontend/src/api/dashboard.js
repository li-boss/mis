// =============================================================================
// dashboard.js — 库存看板 API
// 后端：GET /api/inventory/dashboard
// =============================================================================

import request, { isMockEnabled, mockWait } from './request';

// Mock 看板数据
const mockOverview = {
  totalStock: 12860,
  previousTotalStock: 12434,   // totalStock - inboundToday
  inboundToday: 426,
  lowStockSku: 18,
  exceptionCount: 3,
  pendingInbound: 8,
  warehouseCode: 'DEFAULT'
};

const mockTrend = [
  { label: '周一', inbound: 128, outbound: 78 },
  { label: '周二', inbound: 184, outbound: 112 },
  { label: '周三', inbound: 146, outbound: 96 },
  { label: '周四', inbound: 236, outbound: 178 },
  { label: '周五', inbound: 211, outbound: 156 },
  { label: '周六', inbound: 274, outbound: 221 },
  { label: '周日', inbound: 336, outbound: 268 }
];

export async function getDashboardOverview(params = {}) {
  if (isMockEnabled) {
    return mockWait({ ...mockOverview, warehouseCode: params.warehouseCode || 'DEFAULT' });
  }

  // 后端统一返回 /api/inventory/dashboard
  const res = await request.get('/inventory/dashboard', { params });
  const data = res.data || {};

  return {
    totalStock: data.totalStock ?? data.total_stock ?? 0,
    previousTotalStock: data.previousTotalStock ?? data.previous_total_stock ?? 0,
    inboundToday: data.inboundToday ?? data.inbound_today ?? 0,
    lowStockSku: data.lowStockSku ?? data.low_stock_sku ?? 0,
    exceptionCount: data.exceptionCount ?? data.exception_count ?? 0,
    pendingInbound: data.pendingInbound ?? data.pending_inbound ?? 0,
    trend: data.trend || [],
    warehouseCode: params.warehouseCode || 'DEFAULT'
  };
}

export async function getStockTrend(params = {}) {
  if (isMockEnabled) {
    return mockWait({ list: mockTrend });
  }

  const res = await request.get('/inventory/dashboard', { params });
  const data = res.data || {};
  const trend = data.trend || data.stockTrend || [];

  return {
    list: trend.map((item) => ({
      date: item.date || item.label || item.day || '',
      quantity: item.quantity ?? item.inbound ?? 0,
    }))
  };
}

export async function getStockAlerts(params = {}) {
  if (isMockEnabled) {
    return mockWait({
      list: [
        { id: 1, level: 'warning', message: 'SKU-PK-018 低于安全库存' },
        { id: 2, level: 'danger', message: 'SKU-PT-066 当前无可用库存' }
      ]
    });
  }

  return request.get('/inventory/alerts', { params });
}

export default {
  getDashboardOverview,
  getStockTrend,
  getStockAlerts
};
