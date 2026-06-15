import request, { isMockEnabled, mockWait } from './request';

let mockSkus = [
  {
    id: 1001,
    skuCode: 'SKU-RF-001',
    name: '手持扫码终端',
    category: '设备',
    unit: '台',
    supplierName: '华东智造供应链',
    currentStock: 128,
    safetyStock: 30,
    status: 'active',
    updatedAt: '2026-05-20'
  },
  {
    id: 1002,
    skuCode: 'SKU-PK-018',
    name: '标准周转箱',
    category: '耗材',
    unit: '箱',
    supplierName: '青禾包装',
    currentStock: 22,
    safetyStock: 40,
    status: 'active',
    updatedAt: '2026-05-21'
  },
  {
    id: 1003,
    skuCode: 'SKU-LB-206',
    name: '防水标签纸',
    category: '耗材',
    unit: '卷',
    supplierName: '北辰纸业',
    currentStock: 480,
    safetyStock: 120,
    status: 'active',
    updatedAt: '2026-05-22'
  },
  {
    id: 1004,
    skuCode: 'SKU-PT-066',
    name: '轻型托盘',
    category: '仓储',
    unit: '个',
    supplierName: '启明仓储设备',
    currentStock: 0,
    safetyStock: 20,
    status: 'disabled',
    updatedAt: '2026-05-16'
  }
];

function filterSkus(params = {}) {
  const keyword = params.keyword?.trim().toLowerCase();

  return mockSkus.filter((item) => {
    const matchKeyword =
      !keyword ||
      item.skuCode.toLowerCase().includes(keyword) ||
      item.name.toLowerCase().includes(keyword) ||
      item.supplierName.toLowerCase().includes(keyword);
    const matchCategory = !params.category || item.category === params.category;
    const matchStatus = !params.status || item.status === params.status;

    return matchKeyword && matchCategory && matchStatus;
  });
}

export async function getSkuList(params = {}) {
  if (isMockEnabled) {
    const list = filterSkus(params);
    return mockWait({ list, total: list.length, page: Number(params.page || 1), pageSize: Number(params.pageSize || 20) });
  }

  // 从 localStorage 取当前仓库
  try {
    const wh = JSON.parse(localStorage.getItem('wms_warehouse'));
    if (wh?.code) params.warehouseCode = wh.code;
  } catch { /* ignore */ }

  const res = await request.get('/skus', { params });
  return res.data || res;
}

export async function getSkuDetail(id) {
  if (isMockEnabled) {
    const detail = mockSkus.find((item) => item.id === Number(id));
    return mockWait({ detail });
  }

  const res = await request.get(`/skus/${id}`);
  return (res.data || res).detail || (res.data || res);
}

export async function createSku(payload) {
  if (isMockEnabled) {
    const next = { id: Date.now(), currentStock: Number(payload.currentStock || 0), safetyStock: Number(payload.safetyStock || 0), status: payload.status || 'active', updatedAt: new Date().toISOString().slice(0, 10), ...payload };
    mockSkus = [next, ...mockSkus];
    return mockWait({ detail: next });
  }

  const res = await request.post('/skus', payload);
  return res.data || res;
}

export async function updateSku(id, payload) {
  if (isMockEnabled) {
    let detail = null;
    mockSkus = mockSkus.map((item) => {
      if (item.id !== Number(id)) return item;
      detail = { ...item, ...payload, currentStock: Number(payload.currentStock ?? item.currentStock), safetyStock: Number(payload.safetyStock ?? item.safetyStock), updatedAt: new Date().toISOString().slice(0, 10) };
      return detail;
    });
    return mockWait({ detail });
  }

  const res = await request.put(`/skus/${id}`, payload);
  return res.data || res;
}

export async function deleteSku(id) {
  if (isMockEnabled) {
    mockSkus = mockSkus.filter((item) => item.id !== Number(id));
    return mockWait({ success: true });
  }

  const res = await request.delete(`/skus/${id}`);
  return res.data || res;
}

export async function updateSkuStatus(id, status) {
  if (isMockEnabled) {
    return updateSku(id, { status });
  }

  const res = await request.patch(`/skus/${id}/status`, { status });
  return res.data || res;
}

export default {
  getSkuList,
  getSkuDetail,
  createSku,
  updateSku,
  deleteSku,
  updateSkuStatus
};
