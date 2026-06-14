import request, { isMockEnabled, mockWait } from './request';

const mockUser = {
  id: 3,
  username: 'baiqinhe',
  realName: '白沁禾',
  role: 'frontend',
  roleName: '前端核心',
  permissions: ['sku:read', 'sku:write', 'supplier:read', 'supplier:write']
};

export async function login(payload) {
  if (isMockEnabled) {
    if (!payload.username || !payload.password) {
      throw new Error('请输入账号和密码');
    }

    return mockWait({
      token: 'mock-wms-token-baiqinhe',
      user: {
        ...mockUser,
        username: payload.username
      }
    });
  }

  const res = await request.post('/auth/login', payload);
  return res.data || res;  // 解包 { code, data: { token, user } } → { token, user }
}

export async function register(payload) {
  if (isMockEnabled) {
    return mockWait({
      token: 'mock-wms-token-register',
      user: {
        ...mockUser,
        username: payload.username,
        realName: payload.realName || mockUser.realName
      }
    });
  }

  const res = await request.post('/auth/register', payload);
  return res.data || res;
}

export async function getCurrentUser() {
  if (isMockEnabled) {
    return mockWait({ user: mockUser });
  }

  const res = await request.get('/auth/me');
  return res.data || res;
}

export async function logout() {
  if (isMockEnabled) {
    return mockWait({ success: true });
  }

  const res = await request.post('/auth/logout');
  return res.data || res;
}

export default {
  login,
  register,
  getCurrentUser,
  logout
};
