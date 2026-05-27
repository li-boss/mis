<template>
  <main class="inbound-page">
    <section class="inbound-panel">
      <header class="page-header">
        <h1>入库登记</h1>
      </header>

      <el-form ref="formRef" :model="form" :rules="rules" label-width="96px">
        <el-form-item label="仓库" prop="warehouseId">
          <el-input v-model="form.warehouseId" placeholder="请输入仓库编号" />
        </el-form-item>
        <el-form-item label="SKU" prop="skuId">
          <el-input v-model="form.skuId" placeholder="请输入 SKU 编号" />
        </el-form-item>
        <el-form-item label="数量" prop="quantity">
          <el-input-number v-model="form.quantity" :min="1" :step="1" />
        </el-form-item>
        <el-form-item>
          <el-button type="primary" :loading="submitting" @click="handleSubmit">提交入库</el-button>
          <el-button @click="resetForm">重置</el-button>
        </el-form-item>
      </el-form>
    </section>
  </main>
</template>

<script setup>
import { reactive, ref } from 'vue';
import { ElMessage } from 'element-plus';

import { submitInbound } from '../api/inventory';
import { useUserStore } from '../store/user';

const formRef = ref();
const submitting = ref(false);
const userStore = useUserStore();

const form = reactive({
  warehouseId: 'DEFAULT',
  skuId: '',
  quantity: 1,
});

const rules = {
  warehouseId: [{ required: true, message: '请输入仓库编号', trigger: 'blur' }],
  skuId: [{ required: true, message: '请输入 SKU 编号', trigger: 'blur' }],
  quantity: [{ required: true, type: 'number', min: 1, message: '数量必须大于 0', trigger: 'change' }],
};

const handleSubmit = async () => {
  await formRef.value.validate();
  submitting.value = true;

  try {
    await submitInbound({
      ...form,
      operatorId: userStore.profile?.userId,
    });
    ElMessage.success('入库成功');
    resetForm();
  } finally {
    submitting.value = false;
  }
};

const resetForm = () => {
  form.warehouseId = 'DEFAULT';
  form.skuId = '';
  form.quantity = 1;
  formRef.value?.clearValidate();
};
</script>

<style scoped>
.inbound-page {
  min-height: 100vh;
  padding: 32px;
  background: #f5f7fa;
}

.inbound-panel {
  max-width: 720px;
  padding: 24px;
  background: #ffffff;
  border: 1px solid #e4e7ed;
  border-radius: 8px;
}

.page-header {
  margin-bottom: 24px;
}

.page-header h1 {
  margin: 0;
  font-size: 24px;
  font-weight: 600;
  color: #1f2d3d;
}
</style>
