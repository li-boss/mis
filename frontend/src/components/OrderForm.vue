<template>
  <el-card class="order-form-card" shadow="never">
    <template #header>
      <span class="card-title">新建采购入库单</span>
    </template>

    <el-form
      ref="formRef"
      :model="form"
      :rules="rules"
      label-width="96px"
      @submit.prevent
    >
      <!-- 供应商 -->
      <el-form-item label="供应商" prop="supplierId">
        <el-select
          v-model="form.supplierId"
          placeholder="请选择供应商"
          filterable
          style="width: 100%"
          @change="onSupplierChange"
        >
          <el-option
            v-for="s in suppliers"
            :key="s.id"
            :label="s.name"
            :value="s.id"
          />
        </el-select>
      </el-form-item>

      <!-- 明细行 -->
      <el-form-item label="入库明细">
        <div class="lines-section">
          <div
            v-for="(line, idx) in form.lines"
            :key="idx"
            class="line-row"
          >
            <span class="line-index">{{ idx + 1 }}</span>
            <el-select
              v-model="line.productId"
              placeholder="选择商品"
              filterable
              class="line-input line-product"
            >
              <el-option
                v-for="p in products"
                :key="p.id"
                :label="`${p.skuCode} — ${p.name}`"
                :value="p.id"
              />
            </el-select>
            <el-input-number
              v-model="line.quantity"
              :min="1"
              :step="1"
              placeholder="数量"
              class="line-input line-qty"
            />
            <el-input-number
              v-model="line.unitPrice"
              :min="0"
              :precision="2"
              :step="0.01"
              placeholder="单价"
              class="line-input line-price"
            />
            <el-button
              v-if="form.lines.length > 1"
              type="danger"
              :icon="Delete"
              circle
              size="small"
              @click="removeLine(idx)"
            />
          </div>
          <el-button type="primary" link :icon="Plus" @click="addLine">
            添加明细行
          </el-button>
        </div>
      </el-form-item>

      <!-- 备注 -->
      <el-form-item label="备注">
        <el-input
          v-model="form.remark"
          type="textarea"
          :rows="2"
          placeholder="选填"
        />
      </el-form-item>

      <!-- 操作 -->
      <el-form-item>
        <el-button type="primary" :loading="submitting" @click="handleSubmit">
          创建入库单
        </el-button>
        <el-button @click="resetForm">重置</el-button>
      </el-form-item>
    </el-form>
  </el-card>
</template>

<script setup>
import { reactive, ref, onMounted } from 'vue';
import { ElMessage } from 'element-plus';
import { Plus, Delete } from '@element-plus/icons-vue';

import { createInboundOrder } from '../api/inbound';
import { getSkuList } from '../api/sku';
import { getSupplierList } from '../api/supplier';
import { useUserStore } from '../store/user';

const emit = defineEmits(['created']);

const formRef = ref(null);
const submitting = ref(false);
const userStore = useUserStore();

// ---- 供应商列表（从 API 获取） ----
const suppliers = ref([]);

async function loadSuppliers() {
  try {
    const res = await getSupplierList({ pageSize: 200 });
    suppliers.value = (res.list || []).map(s => ({ id: s.id, name: s.name }))
      .filter(s => s.id <= 4); // 暂时只显示种子供应商（ID 1-4）
  } catch {
    // 降级
  }
}

// ---- 商品/SKU 列表（从 API 获取） ----
const products = ref([]);

async function loadProducts() {
  try {
    const res = await getSkuList({ pageSize: 200 });
    products.value = (res.list || []).filter(p => p.status !== 'disabled');
  } catch {
    // 降级使用空列表
  }
}

onMounted(() => { loadSuppliers(); loadProducts(); });

const emptyLine = () => ({
  productId: '',
  quantity: 1,
  unitPrice: 0,
});

const form = reactive({
  supplierId: null,
  remark: '',
  lines: [emptyLine()],
});

const rules = {
  supplierId: [
    { required: true, message: '请选择供应商', trigger: 'change' },
  ],
};

// ---- 方法 ----
function addLine() {
  form.lines.push(emptyLine());
}

function removeLine(idx) {
  form.lines.splice(idx, 1);
}

function onSupplierChange() {
  // 预留：切换供应商后可加载该供应商的供货商品列表
}

function validateLines() {
  for (let i = 0; i < form.lines.length; i++) {
    const line = form.lines[i];
    if (!line.productId) {
      ElMessage.warning(`第 ${i + 1} 行商品ID不能为空`);
      return false;
    }
    if (!line.quantity || line.quantity <= 0) {
      ElMessage.warning(`第 ${i + 1} 行数量必须大于 0`);
      return false;
    }
  }
  return true;
}

async function handleSubmit() {
  const valid = await formRef.value.validate().catch(() => false);
  if (!valid) return;
  if (!validateLines()) return;

  submitting.value = true;
  try {
    const res = await createInboundOrder({
      supplierId: form.supplierId,
      createdBy: userStore.profile?.userId || 1,
      remark: form.remark,
      lines: form.lines.map((l) => ({
        productId: Number(l.productId),
        quantity: l.quantity,
        unitPrice: l.unitPrice,
      })),
    });
    ElMessage.success(`入库单 #${res.data.inboundId} 创建成功`);
    emit('created', res.data.inboundId);
    resetForm();
  } catch (e) {
    ElMessage.error(e?.message || '创建失败');
  } finally {
    submitting.value = false;
  }
}

function resetForm() {
  form.supplierId = null;
  form.remark = '';
  form.lines = [emptyLine()];
  formRef.value?.clearValidate();
}
</script>

<style scoped>
.order-form-card {
  margin-bottom: 16px;
}

.card-title {
  font-size: 16px;
  font-weight: 600;
}

.lines-section {
  width: 100%;
}

.line-row {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 8px;
}

.line-index {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: #409eff;
  color: #fff;
  font-size: 12px;
  flex-shrink: 0;
}

.line-input {
  flex: 0 0 auto;
}

.line-product {
  width: 140px;
}

.line-qty {
  width: 120px;
}

.line-price {
  width: 130px;
}
</style>
