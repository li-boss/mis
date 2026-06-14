import request, { isMockEnabled, mockWait } from './request';

let mockSuppliers = [
  {
    id: 501,
    supplierCode: 'SUP-HD-001',
    name: '华东智造供应链',
    contactName: '陈经理',
    phone: '13800001234',
    rating: 'A',
    status: 'active',
    address: '上海市浦东新区',
    remark: '设备类长期合作'
  },
  {
    id: 502,
    supplierCode: 'SUP-QH-014',
    name: '青禾包装',
    contactName: '刘主管',
    phone: '13900005678',
    rating: 'B',
    status: 'active',
    address: '苏州市工业园区',
    remark: '包装耗材月结'
  },
  {
    id: 503,
    supplierCode: 'SUP-QM-036',
    name: '启明仓储设备',
    contactName: '王工',
    phone: '13700007890',
    rating: 'A',
    status: 'paused',
    address: '南京市江宁区',
    remark: '托盘与货架'
  }
];

function filterSuppliers(params = {}) {
  const keyword = params.keyword?.trim().toLowerCase();

  return mockSuppliers.filter((item) => {
    const matchKeyword =
      !keyword ||
      item.supplierCode.toLowerCase().includes(keyword) ||
      item.name.toLowerCase().includes(keyword) ||
      item.contactName.toLowerCase().includes(keyword);
    const matchRating = !params.rating || item.rating === params.rating;
    const matchStatus = !params.status || item.status === params.status;

    return matchKeyword && matchRating && matchStatus;
  });
}

export async function getSupplierList(params = {}) {
  if (isMockEnabled) {
    const list = filterSuppliers(params);
    return mockWait({ list, total: list.length, page: Number(params.page || 1), pageSize: Number(params.pageSize || 20) });
  }

  const res = await request.get('/suppliers', { params });
  return res.data || res;
}

export async function getSupplierDetail(id) {
  if (isMockEnabled) {
    const detail = mockSuppliers.find((item) => item.id === Number(id));
    return mockWait({ detail });
  }

  const res = await request.get(`/suppliers/${id}`);
  return (res.data || res).detail || (res.data || res);
}

export async function createSupplier(payload) {
  if (isMockEnabled) {
    const detail = { id: Date.now(), status: payload.status || 'active', rating: payload.rating || 'B', ...payload };
    mockSuppliers = [detail, ...mockSuppliers];
    return mockWait({ detail });
  }

  const res = await request.post('/suppliers', payload);
  return res.data || res;
}

export async function updateSupplier(id, payload) {
  if (isMockEnabled) {
    let detail = null;
    mockSuppliers = mockSuppliers.map((item) => {
      if (item.id !== Number(id)) return item;
      detail = { ...item, ...payload };
      return detail;
    });
    return mockWait({ detail });
  }

  const res = await request.put(`/suppliers/${id}`, payload);
  return res.data || res;
}

export async function deleteSupplier(id) {
  if (isMockEnabled) {
    mockSuppliers = mockSuppliers.filter((item) => item.id !== Number(id));
    return mockWait({ success: true });
  }

  const res = await request.delete(`/suppliers/${id}`);
  return res.data || res;
}

export async function updateSupplierStatus(id, status) {
  if (isMockEnabled) {
    return updateSupplier(id, { status });
  }

  const res = await request.patch(`/suppliers/${id}/status`, { status });
  return res.data || res;
}

export default {
  getSupplierList,
  getSupplierDetail,
  createSupplier,
  updateSupplier,
  deleteSupplier,
  updateSupplierStatus
};
