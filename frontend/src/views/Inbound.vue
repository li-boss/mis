<script setup>
import { reactive, ref } from 'vue';
import OrderForm from '../components/OrderForm.vue';
import OrderTable from '../components/OrderTable.vue';
import { createInboundOrder } from '../api/inbound';

const tableRef = ref(null);
const activeTab = ref('quick');

const onOrderCreated = (inboundId) => {
  tableRef.value?.refresh();
};

// ---- 快速入库 ----
const quickForm = reactive({
  warehouseCode: 'DEFAULT',
  skuCode: '',
  quantity: 1
});
const quickSubmitting = ref(false);
const quickNotice = ref('');

const submitQuickInbound = async () => {
  quickNotice.value = '';

  if (!quickForm.skuCode.trim() || Number(quickForm.quantity) <= 0) {
    quickNotice.value = '请填写 SKU 和入库数量';
    return;
  }

  quickSubmitting.value = true;
  try {
    await createInboundOrder({
      warehouseCode: quickForm.warehouseCode,
      skuCode: quickForm.skuCode.trim(),
      quantity: Number(quickForm.quantity)
    });
    quickNotice.value = '入库单已提交';
    quickForm.skuCode = '';
    quickForm.quantity = 1;
    tableRef.value?.refresh();
  } catch (e) {
    quickNotice.value = e?.message || '提交失败';
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
        <p class="page-subtitle">快速扫码入库或创建采购订单，提交、收货确认与状态流转。</p>
      </div>
    </div>

    <el-tabs v-model="activeTab" class="inbound-tabs">
      <!-- 快速入库 -->
      <el-tab-pane label="快速入库" name="quick">
        <el-card shadow="never" class="quick-card">
          <template #header>
            <span class="card-title">快速入库登记</span>
          </template>

          <el-form
            :model="quickForm"
            label-width="96px"
            @submit.prevent="submitQuickInbound"
          >
            <el-form-item label="仓库">
              <el-input v-model="quickForm.warehouseCode" readonly />
            </el-form-item>

            <el-form-item label="SKU 编号">
              <el-input
                v-model.trim="quickForm.skuCode"
                placeholder="请输入或扫码 SKU"
              />
            </el-form-item>

            <el-form-item label="数量">
              <el-input-number
                v-model="quickForm.quantity"
                :min="1"
                :step="1"
              />
            </el-form-item>

            <el-form-item v-if="quickNotice">
              <el-alert
                :title="quickNotice"
                :type="quickNotice.includes('请') || quickNotice.includes('失败') ? 'error' : 'success'"
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
                提交入库
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
