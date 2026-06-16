import { createRouter, createWebHistory } from 'vue-router';
import { useUserStore } from '../store/user';

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

  // 使用 store 而非直接读 localStorage，确保 token 已通过 fetchProfile 验证
  const userStore = useUserStore();

  // 已登录用户访问公开页面（登录/注册）→ 跳转到看板
  if (to.meta.public && userStore.isAuthenticated && to.name !== 'Register') {
    return { path: '/dashboard' };
  }

  // 需鉴权页面但未登录 → 跳转到登录页
  if (to.meta.requiresAuth && !userStore.isAuthenticated) {
    return { path: '/login', query: { redirect: to.fullPath } };
  }

  // 角色权限检查
  if (to.meta.roles?.length && userStore.profile?.role && !to.meta.roles.includes(userStore.profile.role)) {
    return { path: '/dashboard' };
  }

  return true;
});

export default router;
