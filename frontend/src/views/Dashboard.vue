<script setup>
import { onMounted, reactive, ref, computed } from 'vue';
import { useRouter } from 'vue-router';
import { ArrowRight } from 'lucide-vue-next';
import { getDashboardOverview, getStockTrend } from '../api/dashboard';

const router = useRouter();
const loading = ref(false);
const overview = reactive({
  totalStock: 0,
  inboundToday: 0,
  lowStockSku: 0,
  exceptionCount: 0,
  pendingInbound: 0
});
const trend = ref([]);

const maxInbound = computed(() => {
  if (!trend.value || trend.value.length === 0) return 1;
  const max = Math.max(...trend.value.map(item => item.inbound));
  return max > 0 ? max : 1;
});

const chartPoints = computed(() => {
  if (!trend.value || trend.value.length === 0) return '';
  const max = maxInbound.value;
  return trend.value.map((item, idx) => {
    const x = 24 + idx * 98.66;
    const y = 190 - (item.inbound / max) * 160;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  }).join(' ');
});

const getCircleX = (idx) => 24 + idx * 98.66;
const getCircleY = (item) => {
  const max = maxInbound.value;
  return 190 - (item.inbound / max) * 160;
};

const getBarHeight = (inbound) => {
  const max = maxInbound.value;
  const maxHeight = 160;
  if (max === 1 && inbound === 0) return '10px';
  const ratio = inbound / max;
  return `${Math.max(10, ratio * maxHeight)}px`;
};

const loadDashboard = async () => {
  loading.value = true;

  try {
    const [overviewData, trendData] = await Promise.all([
      getDashboardOverview({ warehouseCode: 'DEFAULT' }),
      getStockTrend()
    ]);
    Object.assign(overview, overviewData);
    trend.value = trendData.list;
  } catch {
    // 鉴权失效时 request 拦截器已处理，此处静默
  } finally {
    loading.value = false;
  }
};

onMounted(loadDashboard);
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">库存看板</h1>
        <p class="page-subtitle">查看库存状态与出入库趋势，关注低库存与异常提醒。</p>
      </div>
      <div class="head-actions">
        <button class="btn btn-ghost" type="button">筛选仓库</button>
        <button class="btn btn-primary" type="button" :disabled="loading" @click="loadDashboard">
          刷新
        </button>
      </div>
    </div>

    <div class="guide-strip">
      <strong>入库登记入口</strong>
      <span>请前往入库登记页面进行商品入库、采购订单创建与收货确认。</span>
      <button class="btn btn-primary" type="button" @click="router.push('/inbound')">
        <ArrowRight />
        前往入库登记
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

    <section class="panel trend-panel">
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
              v-if="chartPoints"
              :points="chartPoints"
              fill="none"
              stroke="#2579ed"
              stroke-width="5"
              stroke-linecap="round"
              stroke-linejoin="round"
            />
            <g fill="#2579ed">
              <circle
                v-for="(item, idx) in trend"
                :key="'dot-' + idx"
                :cx="getCircleX(idx)"
                :cy="getCircleY(item)"
                r="6"
              />
            </g>
          </svg>
          <div class="bar-row">
            <span
              v-for="item in trend"
              :key="item.label"
              class="trend-bar"
              :style="{ height: getBarHeight(item.inbound) }"
            ></span>
          </div>
          <div class="axis-labels">
            <span v-for="item in trend" :key="'lbl-' + item.label">{{ item.label }}</span>
          </div>
        </div>
      </div>
    </section>

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

.trend-panel {
  margin-top: 28px;
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

.requirement-panel {
  margin-top: 28px;
}

@media (max-width: 760px) {
  .guide-strip .btn {
    width: 100%;
    margin-left: 0;
  }
}
</style>
