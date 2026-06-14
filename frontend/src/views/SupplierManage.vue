<script setup>
import { computed, onMounted, reactive, ref } from 'vue';
import { Pencil, Plus, Search, Trash2, X } from 'lucide-vue-next';
import SupplierForm from '../components/SupplierForm.vue';
import {
  createSupplier,
  deleteSupplier,
  getSupplierList,
  updateSupplier,
  updateSupplierStatus
} from '../api/supplier';

const filters = reactive({
  keyword: '',
  rating: '',
  status: ''
});
const suppliers = ref([]);
const total = ref(0);
const loading = ref(false);
const saving = ref(false);
const showModal = ref(false);
const editingSupplier = ref(null);
const toast = ref('');

const statusText = {
  active: '合作中',
  paused: '暂停'
};

const stats = computed(() => {
  const active = suppliers.value.filter((item) => item.status === 'active').length;
  const levelA = suppliers.value.filter((item) => item.rating === 'A').length;

  return {
    total: total.value,
    active,
    levelA,
    paused: suppliers.value.length - active
  };
});

const loadSuppliers = async () => {
  loading.value = true;

  try {
    const result = await getSupplierList({ ...filters, page: 1, pageSize: 20 });
    suppliers.value = result.list;
    total.value = result.total;
  } finally {
    loading.value = false;
  }
};

const openCreate = () => {
  editingSupplier.value = null;
  showModal.value = true;
};

const openEdit = (supplier) => {
  editingSupplier.value = supplier;
  showModal.value = true;
};

const closeModal = () => {
  showModal.value = false;
  editingSupplier.value = null;
};

const saveSupplier = async (payload) => {
  saving.value = true;
  toast.value = '';

  try {
    if (editingSupplier.value) {
      await updateSupplier(editingSupplier.value.id, payload);
      toast.value = '供应商已更新';
    } else {
      await createSupplier(payload);
      toast.value = '供应商已新增';
    }

    closeModal();
    await loadSuppliers();
  } finally {
    saving.value = false;
  }
};

const removeSupplier = async (supplier) => {
  const confirmed = window.confirm(`确认删除 ${supplier.name} 吗？`);
  if (!confirmed) return;

  await deleteSupplier(supplier.id);
  toast.value = '供应商已删除';
  await loadSuppliers();
};

const toggleSupplier = async (supplier) => {
  const nextStatus = supplier.status === 'active' ? 'paused' : 'active';
  await updateSupplierStatus(supplier.id, nextStatus);
  toast.value = nextStatus === 'active' ? '供应商已恢复合作' : '供应商已暂停合作';
  await loadSuppliers();
};

onMounted(loadSuppliers);
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">供应商管理</h1>
        <p class="page-subtitle">维护供应商资料、联系人和合作状态。</p>
      </div>
      <button class="btn btn-primary" type="button" @click="openCreate">
        <Plus />
        新增供应商
      </button>
    </div>

    <div class="stat-grid supplier-stats">
      <article class="stat-card">
        <div class="stat-label">供应商总数</div>
        <div class="stat-value">{{ stats.total }}</div>
        <span class="tag tag-primary">当前筛选</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">合作中</div>
        <div class="stat-value">{{ stats.active }}</div>
        <span class="tag tag-success">可采购</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">A级供应商</div>
        <div class="stat-value">{{ stats.levelA }}</div>
        <span class="tag tag-warning">优先合作</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">暂停</div>
        <div class="stat-value">{{ stats.paused }}</div>
        <span class="tag tag-danger">需复核</span>
      </article>
    </div>

    <section class="panel">
      <div class="panel-body">
        <div class="list-head">
          <div>
            <h2 class="panel-title">供应商列表</h2>
            <p class="panel-note">按编号、名称或联系人检索。</p>
          </div>

          <form class="toolbar" @submit.prevent="loadSuppliers">
            <label class="search-box">
              <Search />
              <input v-model.trim="filters.keyword" placeholder="搜索供应商/联系人" />
            </label>
            <select v-model="filters.rating" class="select filter-select">
              <option value="">全部评级</option>
              <option value="A">A级</option>
              <option value="B">B级</option>
              <option value="C">C级</option>
            </select>
            <select v-model="filters.status" class="select filter-select">
              <option value="">全部状态</option>
              <option value="active">合作中</option>
              <option value="paused">暂停</option>
            </select>
            <button class="btn btn-primary" type="submit">查询</button>
          </form>
        </div>

        <p v-if="toast" class="message success">{{ toast }}</p>

        <div class="data-table-wrap supplier-table">
          <table class="data-table">
            <thead>
              <tr>
                <th>供应商编号</th>
                <th>名称</th>
                <th>联系人</th>
                <th>电话</th>
                <th>评级</th>
                <th>状态</th>
                <th>操作</th>
              </tr>
            </thead>
            <tbody>
              <tr v-if="loading">
                <td class="table-empty" colspan="7">数据加载中</td>
              </tr>
              <tr v-else-if="suppliers.length === 0">
                <td class="table-empty" colspan="7">暂无供应商数据</td>
              </tr>
              <template v-else>
                <tr v-for="supplier in suppliers" :key="supplier.id">
                  <td>
                    <strong>{{ supplier.supplierCode }}</strong>
                  </td>
                  <td>{{ supplier.name }}</td>
                  <td>{{ supplier.contactName }}</td>
                  <td>{{ supplier.phone }}</td>
                  <td>
                    <span class="tag tag-warning">{{ supplier.rating }}</span>
                  </td>
                  <td>
                    <button
                      class="status-button"
                      type="button"
                      :class="supplier.status === 'active' ? 'is-active' : 'is-paused'"
                      @click="toggleSupplier(supplier)"
                    >
                      {{ statusText[supplier.status] || supplier.status }}
                    </button>
                  </td>
                  <td>
                    <div class="inline-actions">
                      <button class="icon-button" type="button" title="编辑" @click="openEdit(supplier)">
                        <Pencil />
                      </button>
                      <button class="icon-button" type="button" title="删除" @click="removeSupplier(supplier)">
                        <Trash2 />
                      </button>
                    </div>
                  </td>
                </tr>
              </template>
            </tbody>
          </table>
        </div>
      </div>
    </section>

    <div v-if="showModal" class="modal-mask" role="dialog" aria-modal="true">
      <section class="modal">
        <header class="modal-head">
          <h2>{{ editingSupplier ? '编辑供应商' : '新增供应商' }}</h2>
          <button class="icon-button" type="button" title="关闭" @click="closeModal">
            <X />
          </button>
        </header>
        <div class="modal-body">
          <SupplierForm
            :supplier="editingSupplier"
            :loading="saving"
            @submit="saveSupplier"
            @cancel="closeModal"
          />
        </div>
      </section>
    </div>
  </section>
</template>

<style scoped>
.supplier-stats {
  margin-top: 0;
}

.list-head {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 18px;
  margin-bottom: 18px;
}

.search-box {
  width: 260px;
  min-height: 42px;
  display: flex;
  align-items: center;
  gap: 9px;
  border: 1px solid var(--color-line);
  border-radius: 8px;
  padding: 0 12px;
  background: #fbfdff;
}

.search-box svg {
  width: 17px;
  height: 17px;
  color: #8794a7;
}

.search-box input {
  width: 100%;
  min-width: 0;
  border: 0;
  outline: 0;
  background: transparent;
}

.filter-select {
  width: 128px;
}

.supplier-table {
  margin-top: 18px;
}

.status-button {
  min-height: 28px;
  border-radius: 999px;
  padding: 0 12px;
  font-size: 12px;
  font-weight: 800;
}

.status-button.is-active {
  color: #08775e;
  background: var(--color-teal-soft);
}

.status-button.is-paused {
  color: #b63232;
  background: #ffe8e8;
}

@media (max-width: 980px) {
  .list-head {
    display: grid;
  }

  .search-box {
    width: 100%;
  }
}
</style>
