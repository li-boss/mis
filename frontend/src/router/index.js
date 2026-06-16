import { createRouter, createWebHistory } from 'vue-router';

const routes = [
  {
    path: '/',
    redirect: '/login'
  },
  {
    path: '/login',
    name: 'Login',
    component: () => import('../views/Login.vue'),
    meta: { public: true, title: '登录' }
  },
  {
    path: '/register',
    name: 'Register',
    component: () => import('../views/Register.vue'),
    meta: { public: true, title: '注册' }
  },
  {
    path: '/dashboard',
    name: 'Dashboard',
    component: () => import('../views/Dashboard.vue'),
    meta: { title: '库存看板', requiresAuth: true, roles: ['keeper', 'admin'] }
  },
  {
    path: '/inbound',
    name: 'Inbound',
    component: () => import('../views/Inbound.vue'),
    meta: { title: '入库登记', requiresAuth: true, roles: ['keeper', 'purchaser', 'admin'] }
  },
  {
    path: '/sku',
    name: 'SkuManage',
    component: () => import('../views/SkuManage.vue'),
    meta: {
      title: 'SKU/商品管理',
      requiresAuth: true,
      roles: ['data_manager', 'admin']
    }
  },
  {
    path: '/supplier',
    name: 'SupplierManage',
    component: () => import('../views/SupplierManage.vue'),
    meta: {
      title: '供应商管理',
      requiresAuth: true,
      roles: ['purchaser', 'admin']
    }
  },
  {
    path: '/settings',
    name: 'Settings',
    component: () => import('../views/Settings.vue'),
    meta: { title: '系统设置', requiresAuth: true, roles: ['admin'] }
  }
];

const router = createRouter({
  history: createWebHistory(),
  routes,
  scrollBehavior: () => ({ top: 0 })
});

router.beforeEach((to) => {
  document.title = `${to.meta.title || 'WMS'} - WMS Inventory`;

  const token = window.localStorage.getItem('wms_token');
  const rawUser = window.localStorage.getItem('wms_user');
  const user = rawUser ? JSON.parse(rawUser) : null;

  if (to.meta.public && token && to.name !== 'Register') {
    return { path: '/dashboard' };
  }

  if (to.meta.requiresAuth && !token) {
    return { path: '/login', query: { redirect: to.fullPath } };
  }

  if (to.meta.roles?.length && user?.role && !to.meta.roles.includes(user.role)) {
    return { path: '/dashboard' };
  }

  return true;
});

export default router;
