import axios from 'axios';
import { ElMessage } from 'element-plus';

import { useUserStore } from '../store/user';

const request = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL || 'http://localhost:8080',
  timeout: 10000,
});

request.interceptors.request.use((config) => {
  const userStore = useUserStore();
  if (userStore.token) {
    config.headers.Authorization = `Bearer ${userStore.token}`;
  }
  return config;
});

request.interceptors.response.use(
  (response) => response.data,
  (error) => {
    const status = error.response?.status;
    if (status === 401) {
      useUserStore().logout();
      ElMessage.error('登录已过期，请重新登录');
    } else if (status >= 500) {
      ElMessage.error('服务器异常，请稍后再试');
    } else {
      ElMessage.error(error.response?.data?.message || '请求失败');
    }

    return Promise.reject(error);
  },
);

export default request;
