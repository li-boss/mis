// =============================================================================
// warehouse.js — 仓库 API
// =============================================================================

import request, { isMockEnabled, mockWait } from './request';

const mockWarehouses = [
  { id: 1, code: 'DEFAULT', name: '默认主仓库', address: '深圳市龙岗区', status: 'active' },
  { id: 2, code: 'SH-HUB', name: '上海分仓', address: '上海市松江区', status: 'active' },
  { id: 3, code: 'BJ-HUB', name: '北京分仓', address: '北京市顺义区', status: 'active' },
];

export async function getWarehouseList() {
  if (isMockEnabled) {
    return mockWait({ list: mockWarehouses, total: mockWarehouses.length });
  }
  const res = await request.get('/inventory/warehouses');
  return res.data || res;
}

export default { getWarehouseList };
