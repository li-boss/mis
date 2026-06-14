<script setup>
import { computed } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { LogOut, RefreshCw, Warehouse } from 'lucide-vue-next';
import { useUserStore } from '../store/user';

const route = useRoute();
const router = useRouter();
const userStore = useUserStore();
const routeTitle = computed(() => route.meta.title || '工作台');

const refreshPage = () => {
  window.location.reload();
};

const logout = async () => {
  await userStore.logout();
  router.push('/login');
};
</script>

<template>
  <header class="topbar">
    <div class="topbar-title">
      <span>{{ routeTitle }}</span>
      <small>WMS Inventory</small>
    </div>

    <div class="topbar-actions">
      <button class="btn btn-ghost" type="button">
        <Warehouse />
        默认仓库 DEFAULT
      </button>
      <button class="btn btn-primary" type="button" @click="refreshPage">
        <RefreshCw />
        刷新
      </button>
      <div class="user-chip">
        <span>{{ userStore.realName }}</span>
        <small>{{ userStore.roleName }}</small>
      </div>
      <button class="icon-button" type="button" title="退出登录" @click="logout">
        <LogOut />
      </button>
    </div>
  </header>
</template>

<style scoped>
.topbar {
  max-width: 1248px;
  min-height: 46px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  margin: 0 auto 22px;
}

.topbar-title {
  display: grid;
  gap: 2px;
  color: var(--color-muted);
  font-weight: 800;
}

.topbar-title span {
  color: #334155;
  font-size: 15px;
}

.topbar-title small {
  font-size: 12px;
  font-weight: 700;
}

.topbar-actions {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 10px;
  flex-wrap: wrap;
}

.user-chip {
  min-height: 42px;
  display: grid;
  align-content: center;
  gap: 2px;
  border: 1px solid var(--color-line);
  border-radius: 8px;
  padding: 0 14px;
  background: var(--color-surface);
}

.user-chip span {
  font-size: 14px;
  font-weight: 800;
}

.user-chip small {
  color: var(--color-muted);
  font-size: 12px;
}

@media (max-width: 760px) {
  .topbar {
    display: grid;
  }

  .topbar-actions {
    justify-content: flex-start;
  }
}
</style>
