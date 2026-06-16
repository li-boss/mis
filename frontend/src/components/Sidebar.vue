<script setup>
import {
  Boxes,
  LayoutDashboard,
  PackagePlus,
  Settings,
  Truck
} from 'lucide-vue-next';
import { computed } from 'vue';
import { useUserStore } from '../store/user';

const userStore = useUserStore();

const navItems = [
  { title: '库存看板', path: '/dashboard', icon: LayoutDashboard, roles: ['keeper', 'admin'] },
  { title: '入库登记', path: '/inbound', icon: PackagePlus, roles: ['keeper', 'purchaser', 'admin'] },
  { title: 'SKU 管理', path: '/sku', icon: Boxes, roles: ['data_manager', 'admin'] },
  { title: '供应商管理', path: '/supplier', icon: Truck, roles: ['purchaser', 'admin'] },
  { title: '系统设置', path: '/settings', icon: Settings, roles: ['admin'] }
];

const visibleItems = computed(() =>
  navItems.filter((item) => item.roles.includes(userStore.role))
);
</script>

<template>
  <aside class="sidebar">
    <div>
      <div class="brand">
        <h1>库存管理系统</h1>
        <p>MIS Inventory</p>
      </div>

      <nav class="nav-list" aria-label="主导航">
        <RouterLink
          v-for="item in visibleItems"
          :key="item.path"
          :to="item.path"
          class="nav-item"
          active-class="is-active"
        >
          <span class="nav-icon">
            <component :is="item.icon" />
          </span>
          <span>{{ item.title }}</span>
        </RouterLink>
      </nav>
    </div>

    <div class="sidebar-profile">
      <span>当前用户</span>
      <strong>{{ userStore.realName || '未登录' }}</strong>
      <small>{{ userStore.roleName || '访客' }}</small>
    </div>
  </aside>
</template>

<style scoped>
.sidebar {
  width: 256px;
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  flex-shrink: 0;
  padding: 42px 24px 24px;
  color: #ffffff;
  background: linear-gradient(180deg, #1e293b 0%, #0f172a 100%);
  border-right: 1px solid rgba(255, 255, 255, 0.04);
}

.brand h1 {
  margin: 0;
  font-size: 24px;
  line-height: 1.2;
  font-weight: 800;
  letter-spacing: -0.5px;
}

.brand p {
  margin: 4px 0 0;
  color: rgba(255, 255, 255, 0.55);
  font-size: 13px;
  font-weight: 600;
  letter-spacing: 0.5px;
  text-transform: uppercase;
}

.nav-list {
  display: grid;
  gap: 8px;
  margin-top: 36px;
}

.nav-item {
  min-height: 46px;
  display: flex;
  align-items: center;
  gap: 12px;
  border-radius: 8px;
  padding: 0 12px;
  color: rgba(255, 255, 255, 0.7);
  font-size: 15px;
  font-weight: 600;
  transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
}

.nav-item:hover {
  color: #ffffff;
  background: rgba(255, 255, 255, 0.05);
  transform: translateX(2px);
}

.nav-item.is-active {
  color: #ffffff;
  background: var(--color-primary);
  box-shadow: 0 4px 14px rgba(37, 121, 237, 0.28);
}

.nav-icon {
  width: 24px;
  height: 24px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 6px;
  background: rgba(255, 255, 255, 0.08);
  transition: background-color 0.2s ease;
}

.nav-item:hover .nav-icon {
  background: rgba(255, 255, 255, 0.15);
}

.nav-item.is-active .nav-icon {
  background: rgba(255, 255, 255, 0.2);
}

.nav-icon svg {
  width: 16px;
  height: 16px;
}

.sidebar-profile {
  display: grid;
  gap: 4px;
  padding: 16px;
  border-radius: 10px;
  background: rgba(255, 255, 255, 0.03);
  border: 1px solid rgba(255, 255, 255, 0.06);
  color: rgba(255, 255, 255, 0.6);
  font-size: 13px;
}

.sidebar-profile strong {
  color: #ffffff;
  font-size: 16px;
  font-weight: 700;
}

.sidebar-profile small {
  line-height: 1.4;
  color: rgba(255, 255, 255, 0.45);
}

@media (max-width: 760px) {
  .sidebar {
    width: 100%;
    min-height: auto;
    padding: 20px 16px;
    gap: 18px;
  }

  .brand h1 {
    font-size: 24px;
  }

  .nav-list {
    grid-template-columns: repeat(2, minmax(0, 1fr));
    margin-top: 20px;
  }

  .nav-item {
    min-height: 46px;
    font-size: 15px;
  }
}
</style>
