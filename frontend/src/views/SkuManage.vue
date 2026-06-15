<script setup>
import { computed, onMounted, reactive, ref } from 'vue';
import { Plus, Search, X } from 'lucide-vue-next';
import SkuTable from '../components/SkuTable.vue';
import {
  createSku,
  deleteSku,
  getSkuList,
  updateSku,
  updateSkuStatus
} from '../api/sku';

const emptySku = () => ({
  skuCode: '',
  name: '',
  category: '耗材',
  unit: '件',
  supplierName: '',
  currentStock: 0,
  safetyStock: 0,
  status: 'active'
});

const filters = reactive({
  keyword: '',
  category: '',
  status: '',
  lowStock: false
});
const skuForm = reactive(emptySku());
const skuItems = ref([]);
const total = ref(0);
const loading = ref(false);
const saving = ref(false);
const showModal = ref(false);
const editingSku = ref(null);
const formError = ref('');
const toast = ref('');

const stats = computed(() => {
  const active = skuItems.value.filter((item) => item.status === 'active').length;
  const lowStock = skuItems.value.filter(
    (item) => Number(item.currentStock) < Number(item.safetyStock)
  ).length;

  return {
    total: total.value,
    active,
    lowStock,
    disabled: skuItems.value.length - active
  };
});

const loadSkus = async () => {
  loading.value = true;

  try {
    const params = { ...filters, page: 1, pageSize: 20 };
    if (filters.lowStock) params.lowStock = '1';
    const result = await getSkuList(params);
    skuItems.value = result.list;
    total.value = result.total;
  } finally {
    loading.value = false;
  }
};

const resetForm = () => {
  Object.assign(skuForm, emptySku());
  formError.value = '';
};

const openCreate = () => {
  editingSku.value = null;
  resetForm();
  showModal.value = true;
};

const openEdit = (item) => {
  editingSku.value = item;
  Object.assign(skuForm, emptySku(), item);
  formError.value = '';
  showModal.value = true;
};

const closeModal = () => {
  showModal.value = false;
  resetForm();
};

const saveSku = async () => {
  formError.value = '';
  toast.value = '';

  if (!skuForm.skuCode.trim() || !skuForm.name.trim()) {
    formError.value = '请填写 SKU 编号和商品名称';
    return;
  }

  saving.value = true;

  try {
    const payload = {
      ...skuForm,
      currentStock: Number(skuForm.currentStock || 0),
      safetyStock: Number(skuForm.safetyStock || 0)
    };

    if (editingSku.value) {
      await updateSku(editingSku.value.id, payload);
      toast.value = 'SKU 已更新';
    } else {
      await createSku(payload);
      toast.value = 'SKU 已新增';
    }

    showModal.value = false;
    await loadSkus();
  } finally {
    saving.value = false;
  }
};

const removeSku = async (item) => {
  const confirmed = window.confirm(`确认删除 ${item.skuCode} 吗？`);
  if (!confirmed) return;

  await deleteSku(item.id);
  toast.value = 'SKU 已删除';
  await loadSkus();
};

const toggleStatus = async (item) => {
  const status = item.status === 'active' ? 'disabled' : 'active';
  await updateSkuStatus(item.id, status);
  toast.value = status === 'active' ? 'SKU 已启用' : 'SKU 已停用';
  await loadSkus();
};

onMounted(loadSkus);
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">SKU/商品管理</h1>
        <p class="page-subtitle">维护商品主数据、库存阈值和供应商绑定。</p>
      </div>
      <button class="btn btn-primary" type="button" @click="openCreate">
        <Plus />
        新增 SKU
      </button>
    </div>

    <div class="stat-grid sku-stats">
      <article class="stat-card">
        <div class="stat-label">SKU 总数</div>
        <div class="stat-value">{{ stats.total }}</div>
        <span class="tag tag-primary">当前筛选</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">启用中</div>
        <div class="stat-value">{{ stats.active }}</div>
        <span class="tag tag-success">可参与出入库</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">低库存</div>
        <div class="stat-value">{{ stats.lowStock }}</div>
        <span class="tag tag-warning">需要补货</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">停用</div>
        <div class="stat-value">{{ stats.disabled }}</div>
        <span class="tag tag-danger">不可交易</span>
      </article>
    </div>

    <section class="panel">
      <div class="panel-body">
        <div class="list-head">
          <div>
            <h2 class="panel-title">商品列表</h2>
            <p class="panel-note">按编号、名称或供应商检索。</p>
          </div>

          <form class="toolbar" @submit.prevent="loadSkus">
            <label class="search-box">
              <Search />
              <input v-model.trim="filters.keyword" placeholder="搜索 SKU/商品/供应商" />
            </label>
            <select v-model="filters.category" class="select filter-select">
              <option value="">全部分类</option>
              <option value="耗材">耗材</option>
              <option value="设备">设备</option>
              <option value="仓储">仓储</option>
            </select>
            <select v-model="filters.status" class="select filter-select">
              <option value="">全部状态</option>
              <option value="active">启用</option>
              <option value="disabled">停用</option>
            </select>
            <button
              type="button"
              class="btn"
              :class="filters.lowStock ? 'btn-warning' : 'btn-ghost'"
              @click="filters.lowStock = !filters.lowStock; loadSkus()"
            >
              ⚠ 低库存
            </button>
            <button class="btn btn-primary" type="submit">查询</button>
          </form>
        </div>

        <p v-if="toast" class="message success">{{ toast }}</p>
        <SkuTable
          class="sku-table"
          :items="skuItems"
          :loading="loading"
          @edit="openEdit"
          @remove="removeSku"
          @toggle-status="toggleStatus"
        />
      </div>
    </section>

    <div v-if="showModal" class="modal-mask" role="dialog" aria-modal="true">
      <section class="modal">
        <header class="modal-head">
          <h2>{{ editingSku ? '编辑 SKU' : '新增 SKU' }}</h2>
          <button class="icon-button" type="button" title="关闭" @click="closeModal">
            <X />
          </button>
        </header>

        <div class="modal-body">
          <form class="sku-form" @submit.prevent="saveSku">
            <div class="grid-2">
              <label class="field">
                <span>SKU 编号</span>
                <input v-model.trim="skuForm.skuCode" class="input" placeholder="例如 SKU-PK-018" />
              </label>

              <label class="field">
                <span>商品名称</span>
                <input v-model.trim="skuForm.name" class="input" placeholder="请输入商品名称" />
              </label>

              <label class="field">
                <span>分类</span>
                <select v-model="skuForm.category" class="select">
                  <option value="耗材">耗材</option>
                  <option value="设备">设备</option>
                  <option value="仓储">仓储</option>
                </select>
              </label>

              <label class="field">
                <span>单位</span>
                <input v-model.trim="skuForm.unit" class="input" placeholder="件/台/卷" />
              </label>

              <label class="field">
                <span>当前库存</span>
                <input v-model.number="skuForm.currentStock" class="input" min="0" type="number" />
              </label>

              <label class="field">
                <span>安全库存</span>
                <input v-model.number="skuForm.safetyStock" class="input" min="0" type="number" />
              </label>

              <label class="field">
                <span>供应商</span>
                <input v-model.trim="skuForm.supplierName" class="input" placeholder="请输入供应商名称" />
              </label>

              <label class="field">
                <span>状态</span>
                <select v-model="skuForm.status" class="select">
                  <option value="active">启用</option>
                  <option value="disabled">停用</option>
                </select>
              </label>
            </div>

            <p v-if="formError" class="message error">{{ formError }}</p>

            <div class="form-actions">
              <button class="btn btn-ghost" type="button" @click="closeModal">取消</button>
              <button class="btn btn-primary" type="submit" :disabled="saving">
                {{ saving ? '保存中' : '保存 SKU' }}
              </button>
            </div>
          </form>
        </div>
      </section>
    </div>
  </section>
</template>

<style scoped>
.sku-stats {
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
  width: 270px;
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

.sku-table {
  margin-top: 18px;
}

.sku-form {
  display: grid;
  gap: 16px;
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
