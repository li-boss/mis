<script setup>
import { reactive, ref } from 'vue';
import OrderForm from '../components/OrderForm.vue';
import OrderTable from '../components/OrderTable.vue';
import { receiveBySku } from '../api/inbound';

const tableRef = ref(null);
const activeTab = ref('quick');

const onOrderCreated = (inboundId) => {
  tableRef.value?.refresh();
};

// ---- SKU 收货（原快速入库） ----
const quickForm = reactive({
  skuCode: '',
  quantity: 1
});
const quickSubmitting = ref(false);
const quickNotice = ref('');
const quickNoticeType = ref('info');

const submitQuickInbound = async () => {
  quickNotice.value = '';

  if (!quickForm.skuCode.trim() || Number(quickForm.quantity) <= 0) {
    quickNotice.value = '请填写 SKU 和收货数量';
    quickNoticeType.value = 'error';
    return;
  }

  quickSubmitting.value = true;
  try {
    const res = await receiveBySku({
      skuCode: quickForm.skuCode.trim(),
      quantity: Number(quickForm.quantity)
    });
    quickNotice.value = res.message;
    quickNoticeType.value = res.data?.success ? 'success' : 'warning';
    if (res.data?.success) {
      quickForm.skuCode = '';
      quickForm.quantity = 1;
    }
    tableRef.value?.refresh();
  } catch (e) {
    quickNotice.value = e?.message || '收货失败';
    quickNoticeType.value = 'error';
  } finally {
    quickSubmitting.value = false;
  }
};
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">入库管理</h1>
        <p class="page-subtitle">采购入库单管理：创建→提交→SKU扫码收货，完整入库流程。</p>
      </div>
    </div>

    <el-tabs v-model="activeTab" class="inbound-tabs">
      <!-- SKU 收货 -->
      <el-tab-pane label="SKU收货" name="quick">
        <el-card shadow="never" class="quick-card">
          <template #header>
            <span class="card-title">SKU 扫码收货</span>
          </template>

          <el-alert
            type="info"
            :closable="false"
            show-icon
            style="margin-bottom: 16px"
            title="请先在「采购入库单」中创建并提交入库单，再在此处扫码收货。"
          />

          <el-form
            :model="quickForm"
            label-width="96px"
            @submit.prevent="submitQuickInbound"
          >
            <el-form-item label="SKU 编号">
              <el-input
                v-model.trim="quickForm.skuCode"
                placeholder="请输入或扫码 SKU（如 101）"
              />
            </el-form-item>

            <el-form-item label="收货数量">
              <el-input-number
                v-model="quickForm.quantity"
                :min="1"
                :step="1"
              />
            </el-form-item>

            <el-form-item v-if="quickNotice">
              <el-alert
                :title="quickNotice"
                :type="quickNoticeType"
                :closable="false"
                show-icon
              />
            </el-form-item>

            <el-form-item>
              <el-button
                type="primary"
                :loading="quickSubmitting"
                @click="submitQuickInbound"
              >
                确认收货
              </el-button>
              <el-button @click="quickForm.skuCode = ''; quickForm.quantity = 1; quickNotice = ''">
                重置
              </el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-tab-pane>

      <!-- 采购入库单 -->
      <el-tab-pane label="采购入库单" name="purchase">
        <OrderForm @created="onOrderCreated" />
        <OrderTable ref="tableRef" />
      </el-tab-pane>
    </el-tabs>
  </section>
</template>

<style scoped>
.inbound-tabs {
  margin-top: 8px;
}

.quick-card {
  max-width: 560px;
}

.card-title {
  font-size: 16px;
  font-weight: 600;
}
</style>
