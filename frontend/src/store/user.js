import { defineStore } from 'pinia';
import authApi from '../api/auth';

const readStoredUser = () => {
  try {
    return JSON.parse(window.localStorage.getItem('wms_user') || 'null');
  } catch {
    return null;
  }
};

export const useUserStore = defineStore('user', {
  state: () => ({
    token: window.localStorage.getItem('wms_token') || '',
    profile: readStoredUser(),
    loading: false,
    sessionValidated: false   // 只有 fetchProfile 成功后才为 true，防止用过期 token 自动登录
  }),
  getters: {
    isAuthenticated: (state) => Boolean(state.token) && state.sessionValidated,
    realName: (state) => state.profile?.realName || state.profile?.username || '未登录',
    roleName: (state) => state.profile?.roleName || '访客',
    role: (state) => state.profile?.role || 'guest'
  },
  actions: {
    persistSession(token, user) {
      this.token = token;
      this.profile = user;
      this.sessionValidated = true;
      window.localStorage.setItem('wms_token', token);
      window.localStorage.setItem('wms_user', JSON.stringify(user));
    },
    async login(payload) {
      this.loading = true;

      try {
        const result = await authApi.login(payload);
        this.persistSession(result.token, result.user);
        return result;
      } finally {
        this.loading = false;
      }
    },
    async register(payload) {
      this.loading = true;

      try {
        const result = await authApi.register(payload);
        this.persistSession(result.token, result.user);
        return result;
      } finally {
        this.loading = false;
      }
    },
    async fetchProfile() {
      if (!this.token) return null;

      try {
        const result = await authApi.getCurrentUser();
        this.profile = result.user;
        this.sessionValidated = true;
        window.localStorage.setItem('wms_user', JSON.stringify(result.user));
        return result.user;
      } catch {
        // token 已过期或无效，清除登录状态
        this.token = '';
        this.profile = null;
        this.sessionValidated = false;
        window.localStorage.removeItem('wms_token');
        window.localStorage.removeItem('wms_user');
        return null;
      }
    },
    async logout() {
      if (this.token) {
        await authApi.logout();
      }

      this.token = '';
      this.profile = null;
      this.sessionValidated = false;
      window.localStorage.removeItem('wms_token');
      window.localStorage.removeItem('wms_user');
    }
  }
});
