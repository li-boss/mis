<script setup>
import { onMounted, reactive, ref } from 'vue';
import { PackagePlus } from 'lucide-vue-next';
import { getDashboardOverview, getStockTrend } from '../api/dashboard';
import { createInboundOrder } from '../api/inbound';

const loading = ref(false);
const notice = ref('');
const overview = reactive({
  totalStock: 0,
  inboundToday: 0,
  lowStockSku: 0,
  exceptionCount: 0,
  pendingInbound: 0
});
const trend = ref([]);
const inboundForm = reactive({
  warehouseCode: 'DEFAULT',
  skuCode: '',
  quantity: 1
});

const loadDashboard = async () => {
  loading.value = true;

  try {
    const [overviewData, trendData] = await Promise.all([
      getDashboardOverview({ warehouseCode: inboundForm.warehouseCode }),
      getStockTrend()
    ]);
    Object.assign(overview, overviewData);
    trend.value = trendData.list;
  } finally {
    loading.value = false;
  }
};

const submitInbound = async () => {
  notice.value = '';

  if (!inboundForm.skuCode.trim() || Number(inboundForm.quantity) <= 0) {
    notice.value = '请填写 SKU 和入库数量';
    return;
  }

  await createInboundOrder({ ...inboundForm, quantity: Number(inboundForm.quantity) });
  notice.value = '入库单已提交';
  inboundForm.skuCode = '';
  inboundForm.quantity = 1;
  await loadDashboard();
};

onMounted(loadDashboard);
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">库存看板</h1>
        <p class="page-subtitle">查看库存状态，选择需要处理的商品后完成入库登记。</p>
      </div>
      <div class="head-actions">
        <button class="btn btn-ghost" type="button">筛选仓库</button>
        <button class="btn btn-primary" type="button" :disabled="loading" @click="loadDashboard">
          刷新
        </button>
      </div>
    </div>

    <div class="guide-strip">
      <strong>下一步建议</strong>
      <span>1. 先确认库存趋势</span>
      <span>2. 选择仓库和 SKU</span>
      <span>3. 填写数量并提交</span>
      <span>4. 查看提交结果提示</span>
      <button class="btn btn-primary" type="button">
        <PackagePlus />
        开始入库
      </button>
    </div>

    <div class="stat-grid">
      <article class="stat-card">
        <div class="stat-label">总库存</div>
        <div class="stat-value">{{ overview.totalStock.toLocaleString() }}</div>
        <span class="tag tag-primary">较昨日 +3.2%</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">今日入库</div>
        <div class="stat-value">{{ overview.inboundToday }}</div>
        <span class="tag tag-success">待同步 {{ overview.pendingInbound }} 单</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">低库存 SKU</div>
        <div class="stat-value">{{ overview.lowStockSku }}</div>
        <span class="tag tag-warning">建议优先处理</span>
      </article>
      <article class="stat-card">
        <div class="stat-label">异常记录</div>
        <div class="stat-value">{{ overview.exceptionCount }}</div>
        <span class="tag tag-danger">需要复核</span>
      </article>
    </div>

    <div class="dashboard-grid">
      <section class="panel">
        <div class="panel-body">
          <h2 class="panel-title">近期出入库趋势</h2>
          <p class="panel-note">帮助判断是否需要补货或复核异常波动。</p>

          <div class="trend-chart" aria-label="近期出入库趋势图">
            <div class="chart-lines">
              <span></span>
              <span></span>
              <span></span>
              <span></span>
            </div>
            <svg viewBox="0 0 640 220" role="img" aria-hidden="true">
              <polyline
                points="24,178 116,146 208,164 300,96 392,112 484,54 616,22"
                fill="none"
                stroke="#2579ed"
                stroke-width="5"
                stroke-linecap="round"
                stroke-linejoin="round"
              />
              <g fill="#2579ed">
                <circle cx="24" cy="178" r="6" />
                <circle cx="116" cy="146" r="6" />
                <circle cx="208" cy="164" r="6" />
                <circle cx="300" cy="96" r="6" />
                <circle cx="392" cy="112" r="6" />
                <circle cx="484" cy="54" r="6" />
                <circle cx="616" cy="22" r="6" />
              </g>
            </svg>
            <div class="bar-row">
              <span
                v-for="item in trend"
                :key="item.label"
                class="trend-bar"
                :style="{ height: `${Math.max(44, item.inbound / 1.15)}px` }"
              ></span>
            </div>
            <div class="axis-labels">
              <span>周一</span>
              <span>周三</span>
              <span>周五</span>
              <span>周日</span>
            </div>
          </div>
        </div>
      </section>

      <section class="panel">
        <div class="panel-body">
          <h2 class="panel-title">入库登记</h2>
          <p class="panel-note">字段少、提示明确，减少录入犹豫。</p>

          <form class="quick-form" @submit.prevent="submitInbound">
            <label class="field">
              <span>仓库</span>
              <input v-model="inboundForm.warehouseCode" class="input" readonly />
            </label>
            <label class="field">
              <span>SKU 编号</span>
              <input v-model.trim="inboundForm.skuCode" class="input" placeholder="请输入或扫码 SKU" />
            </label>
            <label class="field">
              <span>数量</span>
              <input v-model.number="inboundForm.quantity" class="input" min="1" type="number" />
            </label>

            <p v-if="notice" class="message" :class="notice.includes('请') ? 'error' : 'success'">
              {{ notice }}
            </p>

            <button class="btn btn-primary submit-btn" type="submit">提交入库</button>
          </form>
        </div>
      </section>
    </div>

    <section class="panel requirement-panel">
      <div class="panel-body">
        <h2 class="panel-title">待处理事项</h2>
        <p class="panel-note">
          低库存 SKU 需要补货确认，异常记录需要仓管复核，入库单提交后等待后端返回处理结果。
        </p>
      </div>
    </section>
  </section>
</template>

<style scoped>
.guide-strip {
  min-height: 84px;
  display: flex;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
  border: 1px solid #cbe1f7;
  border-radius: 8px;
  padding: 17px 28px;
  background: var(--color-primary-soft);
  color: #3d4a5e;
}

.guide-strip strong {
  width: 100%;
  color: #105aa9;
  font-size: 18px;
}

.guide-strip .btn {
  margin-left: auto;
}

.dashboard-grid {
  display: grid;
  grid-template-columns: minmax(0, 1.6fr) minmax(330px, 0.95fr);
  gap: 30px;
}

.trend-chart {
  position: relative;
  height: 238px;
  margin-top: 28px;
  padding: 0 14px 30px;
}

.chart-lines {
  position: absolute;
  inset: 8px 0 44px;
  display: grid;
  grid-template-rows: repeat(4, 1fr);
}

.chart-lines span {
  border-top: 1px solid #e8edf4;
}

.trend-chart svg {
  position: absolute;
  inset: 0 0 30px;
  z-index: 2;
  width: 100%;
  height: calc(100% - 30px);
}

.bar-row {
  position: absolute;
  inset: 10px 22px 44px;
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  z-index: 1;
}

.trend-bar {
  width: 32px;
  border-radius: 7px;
  background: rgba(24, 164, 131, 0.48);
}

.axis-labels {
  position: absolute;
  left: 20px;
  right: 20px;
  bottom: 0;
  display: flex;
  justify-content: space-between;
  color: var(--color-muted);
  font-size: 13px;
}

.quick-form {
  display: grid;
  gap: 18px;
  margin-top: 28px;
}

.submit-btn {
  justify-self: end;
  min-width: 134px;
}

.requirement-panel {
  margin-top: 28px;
}

@media (max-width: 1040px) {
  .dashboard-grid {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 760px) {
  .guide-strip .btn {
    width: 100%;
    margin-left: 0;
  }
}
</style>
