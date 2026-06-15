<template>
  <el-card class="order-table-card" shadow="never">
    <template #header>
      <div class="table-header">
        <span class="card-title">入库单列表</span>
        <el-select
          v-model="statusFilter"
          placeholder="状态筛选"
          clearable
          style="width: 140px"
          @change="fetchList"
        >
          <el-option
            v-for="item in statusOptions"
            :key="item.value"
            :label="item.label"
            :value="item.value"
          />
        </el-select>
      </div>
    </template>

    <el-table :data="orders" v-loading="loading" stripe>
      <el-table-column prop="inboundId" label="入库单号" width="100" />
      <el-table-column prop="supplierId" label="供应商ID" width="100" />
      <el-table-column label="状态" width="100">
        <template #default="{ row }">
          <el-tag :type="statusTag(row.status)" size="small">
            {{ STATUS_LABEL[row.status] || row.status }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column prop="createdAt" label="创建时间" width="170" />
      <el-table-column label="明细/收货" min-width="200">
        <template #default="{ row }">
          <div
            v-for="line in row.lines"
            :key="line.lineId"
            class="line-summary"
          >
            商品 #{{ line.productId }}
            — 订货 {{ line.quantityOrdered }}
            / 已收
            <span
              :class="{ 'fully-received': line.quantityReceived >= line.quantityOrdered }"
            >
              {{ line.quantityReceived }}
            </span>
          </div>
        </template>
      </el-table-column>
      <el-table-column label="操作" width="280" fixed="right">
        <template #default="{ row }">
          <!-- DRAFT → 提交/取消 -->
          <template v-if="row.status === STATUS.DRAFT">
            <el-button type="primary" size="small" @click="handleSubmit(row)">
              提交
            </el-button>
            <el-button type="danger" size="small" @click="handleCancel(row)">
              取消
            </el-button>
          </template>

          <!-- SUBMITTED / PARTIAL → 收货 -->
          <template v-else-if="row.status === STATUS.SUBMITTED || row.status === STATUS.PARTIAL">
            <el-button
              v-for="line in row.lines"
              :key="'recv-' + line.lineId"
              type="success"
              size="small"
              :disabled="line.quantityReceived >= line.quantityOrdered"
              @click="handleReceive(line)"
            >
              收货 #{{ line.productId }}
            </el-button>
          </template>

          <!-- RECEIVED / CANCELLED → 只读 -->
          <template v-else>
            <el-tag type="info" size="small">已完成</el-tag>
          </template>
        </template>
      </el-table-column>
    </el-table>

    <!-- 分页 -->
    <div class="pagination-wrap">
      <el-pagination
        v-model:current-page="page"
        :page-size="pageSize"
        :total="total"
        layout="total, prev, pager, next"
        @current-change="fetchList"
      />
    </div>
  </el-card>
</template>

<script setup>
import { ref, onMounted } from 'vue';
import { ElMessage, ElMessageBox } from 'element-plus';

import {
  STATUS,
  STATUS_LABEL,
  listInboundOrders,
  submitInboundOrder,
  receiveInbound,
  cancelInboundOrder,
} from '../api/inbound';

// ---- 状态 ----
const statusFilter = ref('');
const orders = ref([]);
const loading = ref(false);
const page = ref(1);
const pageSize = ref(10);
const total = ref(0);

const statusOptions = [
  { label: '全部', value: '' },
  { label: '草稿', value: STATUS.DRAFT },
  { label: '已提交', value: STATUS.SUBMITTED },
  { label: '部分收货', value: STATUS.PARTIAL },
  { label: '已收货', value: STATUS.RECEIVED },
  { label: '已取消', value: STATUS.CANCELLED },
];

function statusTag(status) {
  const map = {
    [STATUS.DRAFT]: 'info',
    [STATUS.SUBMITTED]: 'warning',
    [STATUS.PARTIAL]: '',
    [STATUS.RECEIVED]: 'success',
    [STATUS.CANCELLED]: 'danger',
  };
  return map[status] || 'info';
}

// ---- 数据加载 ----
async function fetchList() {
  loading.value = true;
  try {
    const res = await listInboundOrders({
      status: statusFilter.value || undefined,
      limit: pageSize.value,
      offset: (page.value - 1) * pageSize.value,
    });
    orders.value = res.data.list;
    total.value = res.data.total;
  } catch (e) {
    ElMessage.error('加载列表失败');
  } finally {
    loading.value = false;
  }
}

// ---- 操作 ----
async function handleSubmit(row) {
  try {
    await ElMessageBox.confirm(
      `确认提交入库单 #${row.inboundId}？提交后将无法修改。`,
      '提交确认',
      { type: 'warning' },
    );
    await submitInboundOrder(row.inboundId);
    ElMessage.success('已提交');
    fetchList();
  } catch {
    // 取消
  }
}

async function handleCancel(row) {
  try {
    await ElMessageBox.confirm(
      `确认取消入库单 #${row.inboundId}？`,
      '取消确认',
      { type: 'warning' },
    );
    await cancelInboundOrder(row.inboundId);
    ElMessage.success('已取消');
    fetchList();
  } catch {
    // 取消
  }
}

async function handleReceive(line) {
  try {
    const qty = line.quantityOrdered - line.quantityReceived;
    const { value } = await ElMessageBox.prompt(
      `商品 #${line.productId} — 本次收货数量：`,
      '收货确认',
      {
        inputValue: String(qty),
        inputPattern: /^[1-9]\d*(\.\d+)?$/,
        inputErrorMessage: '请输入有效数量',
      },
    );
    await receiveInbound({
      lineId: line.lineId,
      receiveQuantity: Number(value),
    });
    ElMessage.success('收货成功');
    fetchList();
  } catch {
    // 取消
  }
}

// ---- 向外暴露刷新 ----
defineExpose({ refresh: fetchList });

onMounted(fetchList);
</script>

<style scoped>
.order-table-card {
  margin-bottom: 16px;
}

.table-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.card-title {
  font-size: 16px;
  font-weight: 600;
}

.line-summary {
  font-size: 13px;
  color: #606266;
  line-height: 1.6;
}

.fully-received {
  color: #67c23a;
  font-weight: 600;
}

.pagination-wrap {
  display: flex;
  justify-content: flex-end;
  margin-top: 16px;
}
</style>
