<script setup>
import { computed, ref, onMounted, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { LogOut, RefreshCw, Warehouse } from 'lucide-vue-next';
import { useUserStore } from '../store/user';
import { getWarehouseList } from '../api/warehouse';

const route = useRoute();
const router = useRouter();
const userStore = useUserStore();
const routeTitle = computed(() => route.meta.title || '工作台');

// ---- 仓库选择 ----
const warehouses = ref([]);
const currentWh = ref(null);

const savedWh = localStorage.getItem('wms_warehouse');
if (savedWh) {
  try { currentWh.value = JSON.parse(savedWh); } catch { /* ignore */ }
}

async function loadWarehouses() {
  try {
    const res = await getWarehouseList();
    warehouses.value = res.list || [];
    if (!currentWh.value && warehouses.value.length) {
      currentWh.value = warehouses.value[0];
    }
  } catch { /* ignore */ }
}

watch(currentWh, (val) => {
  if (val) localStorage.setItem('wms_warehouse', JSON.stringify(val));
}, { deep: true });

function switchWarehouse(wh) {
  currentWh.value = wh;
  window.location.reload(); // 切换仓库后刷新页面数据
}

onMounted(loadWarehouses);

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
      <div class="wh-selector">
        <Warehouse class="wh-icon" />
        <select
          class="wh-dropdown"
          :value="currentWh?.code || ''"
          @change="switchWarehouse(warehouses.find(w => w.code === ($event.target).value))"
        >
          <option v-for="w in warehouses" :key="w.id" :value="w.code">
            {{ w.name }} ({{ w.code }})
          </option>
        </select>
      </div>
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

.wh-selector {
  display: flex;
  align-items: center;
  gap: 6px;
  background: var(--color-surface);
  border: 1px solid var(--color-line);
  border-radius: 8px;
  padding: 0 10px;
  min-height: 42px;
}

.wh-icon {
  width: 16px;
  height: 16px;
  color: var(--color-muted);
  flex-shrink: 0;
}

.wh-dropdown {
  border: none;
  background: transparent;
  font-size: 13px;
  font-weight: 600;
  color: #334155;
  outline: none;
  cursor: pointer;
  padding: 4px 0;
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
