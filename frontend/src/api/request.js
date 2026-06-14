import axios from 'axios';

export const isMockEnabled = import.meta.env.VITE_USE_MOCK !== 'false';

export const mockWait = (payload, timeout = 220) =>
  new Promise((resolve) => {
    window.setTimeout(() => resolve(payload), timeout);
  });

const request = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL || '/api',
  timeout: 12000,
  headers: {
    'Content-Type': 'application/json'
  }
});

request.interceptors.request.use((config) => {
  const token = window.localStorage.getItem('wms_token');

  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }

  return config;
});

request.interceptors.response.use(
  (response) => response.data,
  (error) => {
    const status = error.response?.status;
    const payload = error.response?.data;

    if (status === 401) {
      window.localStorage.removeItem('wms_token');
      window.localStorage.removeItem('wms_user');
      window.dispatchEvent(new CustomEvent('wms:auth-expired'));
    }

    return Promise.reject({
      status,
      message: payload?.message || error.message || '请求失败',
      details: payload?.details || null
    });
  }
);

export default request;
