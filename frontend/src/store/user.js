import { defineStore } from 'pinia';

export const useUserStore = defineStore('user', {
  state: () => ({
    token: localStorage.getItem('MIS_TOKEN') || '',
    role: localStorage.getItem('MIS_ROLE') || '',
    profile: JSON.parse(localStorage.getItem('MIS_PROFILE') || 'null'),
  }),
  getters: {
    isAdmin: (state) => state.role === 'ADMIN',
    isLoggedIn: (state) => Boolean(state.token),
  },
  actions: {
    setSession({ token, role, profile }) {
      this.token = token;
      this.role = role;
      this.profile = profile;

      localStorage.setItem('MIS_TOKEN', token);
      localStorage.setItem('MIS_ROLE', role);
      localStorage.setItem('MIS_PROFILE', JSON.stringify(profile));
    },
    logout() {
      this.token = '';
      this.role = '';
      this.profile = null;

      localStorage.removeItem('MIS_TOKEN');
      localStorage.removeItem('MIS_ROLE');
      localStorage.removeItem('MIS_PROFILE');
    },
  },
});
