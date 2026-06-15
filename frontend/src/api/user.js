// =============================================================================
// user.js — 用户管理 API（仅 admin 可调用）
// =============================================================================

import request, { isMockEnabled, mockWait } from './request';

const mockUsers = [
  { userId: 1, username: 'admin', realName: '管理员', role: 'admin', roleName: '系统管理员' },
  { userId: 2, username: 'keeper', realName: '库管员', role: 'keeper', roleName: '库管员' },
  { userId: 3, username: 'buyer', realName: '采购员', role: 'purchaser', roleName: '采购员' },
  { userId: 4, username: 'data_mgr', realName: '数据管理员', role: 'data_manager', roleName: '数据管理员' }
];

export async function listUsers() {
  if (isMockEnabled) {
    return mockWait({ list: [...mockUsers] });
  }

  const res = await request.get('/users');
  return res.data || res;
}

export async function updateUserRole(userId, role) {
  if (isMockEnabled) {
    const user = mockUsers.find((u) => u.userId === userId);
    if (user) {
      user.role = role;
      user.roleName = { admin: '系统管理员', keeper: '库管员', purchaser: '采购员', data_manager: '数据管理员' }[role] || role;
    }
    return mockWait({ user: { ...user } });
  }

  const res = await request.put(`/users/${userId}/role`, { role });
  return res.data || res;
}

export default { listUsers, updateUserRole };
