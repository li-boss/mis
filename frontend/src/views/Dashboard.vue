<script setup>
import { onMounted, reactive, ref, computed } from 'vue';
import { useRouter } from 'vue-router';
import { ArrowRight } from 'lucide-vue-next';
import { getDashboardOverview } from '../api/dashboard';

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

function getCurrentWarehouseCode() {
  try {
    const wh = JSON.parse(localStorage.getItem('wms_warehouse'));
    return wh?.code || 'DEFAULT';
  } catch { return 'DEFAULT'; }
}

const loadDashboard = async () => {
  loading.value = true;
  try {
    const whCode = getCurrentWarehouseCode();
    const data = await getDashboardOverview({ warehouseCode: whCode });
    Object.assign(overview, {
      totalStock: data.totalStock,
      inboundToday: data.inboundToday,
      lowStockSku: data.lowStockSku,
      exceptionCount: data.exceptionCount,
      pendingInbound: data.pendingInbound,
    });
    if (data.trend) trend.value = data.trend;
  } catch {
    // Silent on error
  } finally {
    loading.value = false;
  }
};

// ---- Line Chart Calculations ----
const chartMax = computed(() => {
  if (!trend.value || !trend.value.length) return 1;
  return Math.max(...trend.value.map(t => t.quantity || t.inbound || 0), 1);
});

const trendPoints = computed(() => {
  if (!trend.value || !trend.value.length) return [];
  const w = 640, h = 180;
  const padX = 40;
  const padTop = 25;
  const padBottom = 35;
  const step = (w - padX * 2) / (trend.value.length - 1 || 1);
  return trend.value.map((t, i) => {
    const x = padX + i * step;
    const val = t.quantity || t.inbound || 0;
    const y = h - padBottom - (val / chartMax.value) * (h - padBottom - padTop);
    return {
      x: Math.round(x),
      y: Math.round(y),
      date: t.date || t.label || '',
      value: val
    };
  });
});

const linePoints = computed(() => {
  return trendPoints.value.map(p => `${p.x},${p.y}`).join(' ');
});

const areaPath = computed(() => {
  const pts = trendPoints.value;
  if (!pts.length) return '';
  const first = pts[0];
  const last = pts[pts.length - 1];
  const linePart = pts.map(p => `L ${p.x} ${p.y}`).join(' ');
  return `M ${first.x} 145 ${linePart} L ${last.x} 145 Z`;
});

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
          
          <svg v-if="trend.length" viewBox="0 0 640 180" class="svg-chart" role="img" aria-hidden="true">
            <!-- 定义渐变 -->
            <defs>
              <linearGradient id="chartGrad" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="#2579ed" stop-opacity="0.25" />
                <stop offset="100%" stop-color="#2579ed" stop-opacity="0.00" />
              </linearGradient>
            </defs>

            <!-- 网格背景线 -->
            <g stroke="#e2e8f0" stroke-width="1" stroke-dasharray="4,4">
              <line x1="40" y1="25" x2="600" y2="25" />
              <line x1="40" y1="65" x2="600" y2="65" />
              <line x1="40" y1="105" x2="600" y2="105" />
              <line x1="40" y1="145" x2="600" y2="145" />
            </g>

            <!-- 渐变面积图 -->
            <path :d="areaPath" fill="url(#chartGrad)" />

            <!-- 折线 -->
            <polyline
              :points="linePoints"
              fill="none"
              stroke="#2579ed"
              stroke-width="3.5"
              stroke-linecap="round"
              stroke-linejoin="round"
            />

            <!-- 数据点和标签 -->
            <g v-for="(p, i) in trendPoints" :key="i">
              <!-- 数值 -->
              <text :x="p.x" :y="p.y - 12" text-anchor="middle" class="chart-val-text">
                {{ p.value }}
              </text>
              <!-- 日期 -->
              <text :x="p.x" y="166" text-anchor="middle" class="chart-axis-text">
                {{ p.date }}
              </text>
              <!-- 交互点 -->
              <circle :cx="p.x" :cy="p.y" r="5.5" class="chart-dot" />
              <circle :cx="p.x" :cy="p.y" r="12" class="chart-dot-hover" />
            </g>
          </svg>
          <div v-else class="chart-empty">暂无趋势数据</div>
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
  pointer-events: none;
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

.chart-dot {
  fill: #ffffff;
  stroke: #2579ed;
  stroke-width: 3;
  transition: all 0.3s ease;
}

.chart-dot-hover {
  fill: #2579ed;
  opacity: 0;
  transition: all 0.3s ease;
  cursor: pointer;
}

.chart-dot-hover:hover {
  opacity: 0.18;
  r: 15;
}

.chart-val-text {
  font-size: 12px;
  font-weight: 700;
  fill: #1e293b;
  opacity: 0.9;
  transition: opacity 0.3s ease;
}

.chart-axis-text {
  font-size: 12px;
  fill: #64748b;
  font-weight: 500;
}

.chart-empty {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--color-muted);
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
