// =============================================================================
// inventory.js — 库存查询 API
// =============================================================================

import request, { isMockEnabled, mockWait } from './request';

// Mock 库存数据
const mockInventory = [
  { inventoryId: 1, productId: 101, quantity: 150.0, version: 2, updatedAt: '2026-06-02T10:30:00' },
  { inventoryId: 2, productId: 102, quantity: 80.0, version: 1, updatedAt: '2026-06-01T14:00:00' },
  { inventoryId: 3, productId: 201, quantity: 300.0, version: 3, updatedAt: '2026-06-03T09:15:00' },
  { inventoryId: 4, productId: 301, quantity: 50.0, version: 1, updatedAt: '2026-05-30T16:45:00' }
];

export async function submitInbound(data) {
  if (isMockEnabled) {
    return mockWait({ code: 0, message: '入库成功', data: { inboundId: Date.now() } });
  }
  return request.post('/inventory/inbound', data);
}

export async function fetchInventoryDashboard() {
  if (isMockEnabled) {
    return mockWait({
      code: 0,
      data: {
        topItems: mockInventory.map((i) => ({ skuId: i.productId, quantity: i.quantity })),
        trend: [
          { date: '2026-06-08', quantity: 128 },
          { date: '2026-06-09', quantity: 184 },
          { date: '2026-06-10', quantity: 146 },
          { date: '2026-06-11', quantity: 236 },
          { date: '2026-06-12', quantity: 211 },
          { date: '2026-06-13', quantity: 274 },
          { date: '2026-06-14', quantity: 336 }
        ]
      }
    });
  }
  return request.get('/inventory/dashboard');
}

export async function getInventory(productId) {
  if (isMockEnabled) {
    const item = mockInventory.find((i) => i.productId === Number(productId)) || null;
    return mockWait({ code: 0, data: item });
  }
  return request.get(`/inventory/${productId}`);
}

export async function listInventory(params = {}) {
  if (isMockEnabled) {
    const limit = Number(params.limit) || 50;
    const offset = Number(params.offset) || 0;
    return mockWait({
      code: 0,
      data: mockInventory.slice(offset, offset + limit)
    });
  }
  return request.get('/inventory', { params });
}

export default {
  submitInbound,
  fetchInventoryDashboard,
  getInventory,
  listInventory
};
