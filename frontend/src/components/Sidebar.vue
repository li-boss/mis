<script setup>
import {
  Boxes,
  LayoutDashboard,
  PackagePlus,
  Settings,
  Truck
} from 'lucide-vue-next';

const navItems = [
  { title: '库存看板', path: '/dashboard', icon: LayoutDashboard },
  { title: '入库登记', path: '/inbound', icon: PackagePlus },
  { title: 'SKU 管理', path: '/sku', icon: Boxes },
  { title: '供应商管理', path: '/supplier', icon: Truck },
  { title: '系统设置', path: '/settings', icon: Settings }
];
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
          v-for="item in navItems"
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
      <span>负责人</span>
      <strong>白沁禾</strong>
      <small>前端核心 · 用户/SKU/供应商</small>
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
  padding: 42px 30px 24px;
  color: #ffffff;
  background: var(--color-sidebar);
}

.brand h1 {
  margin: 0;
  font-size: 28px;
  line-height: 1.1;
  letter-spacing: 0;
}

.brand p {
  margin: 7px 0 0;
  color: rgba(255, 255, 255, 0.68);
  font-size: 15px;
}

.nav-list {
  display: grid;
  gap: 14px;
  margin-top: 32px;
}

.nav-item {
  min-height: 54px;
  display: flex;
  align-items: center;
  gap: 14px;
  border-radius: 8px;
  padding: 0 14px;
  color: rgba(255, 255, 255, 0.75);
  font-size: 18px;
  font-weight: 700;
}

.nav-item:hover,
.nav-item.is-active {
  color: #ffffff;
  background: var(--color-primary);
}

.nav-icon {
  width: 24px;
  height: 24px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  border-radius: 6px;
  background: rgba(255, 255, 255, 0.16);
}

.nav-icon svg {
  width: 17px;
  height: 17px;
}

.sidebar-profile {
  display: grid;
  gap: 5px;
  color: rgba(255, 255, 255, 0.68);
  font-size: 14px;
}

.sidebar-profile strong {
  color: #ffffff;
  font-size: 18px;
}

.sidebar-profile small {
  line-height: 1.5;
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
