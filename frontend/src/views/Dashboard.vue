<script setup>
import { onMounted, reactive, ref, computed } from 'vue';
import { useRouter } from 'vue-router';
import { ArrowRight } from 'lucide-vue-next';
import { getDashboardOverview } from '../api/dashboard';

const router = useRouter();
const loading = ref(false);
const overview = reactive({
  totalStock: 0, inboundToday: 0, lowStockSku: 0, exceptionCount: 0, pendingInbound: 0
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
    // trend 在同一个响应里
    if (data.trend) trend.value = data.trend;
  } catch { /* ignore */ }
  finally { loading.value = false; }
};

// ---- 图表计算 ----
const chartMax = computed(() => {
  if (!trend.value.length) return 1;
  return Math.max(...trend.value.map(t => t.quantity || 0), 1);
});

const parsedPoints = computed(() => {
  if (!trend.value.length) return [];
  const w = 640, h = 180, pad = 28;
  const step = (w - pad * 2) / (trend.value.length - 1 || 1);
  return trend.value.map((t, i) => {
    const x = pad + i * step;
    // Keep y within 30 to 145 to leave space for labels and avoid clipping
    const y = 145 - (t.quantity || 0) / chartMax.value * 110;
    return {
      x: Math.round(x),
      y: Math.round(y),
      value: t.quantity || 0,
      date: t.date || t.label || ''
    };
  });
});

const chartPoints = computed(() => {
  return parsedPoints.value.map(p => `${p.x},${p.y}`).join(' ');
});

const chartFillPoints = computed(() => {
  if (!parsedPoints.value.length) return '';
  const first = parsedPoints.value[0];
  const last = parsedPoints.value[parsedPoints.value.length - 1];
  return `${first.x},180 ` + parsedPoints.value.map(p => `${p.x},${p.y}`).join(' ') + ` ${last.x},180`;
});

const barItems = computed(() => {
  if (!trend.value.length) return [];
  return trend.value.map(t => ({
    date: t.date || t.label || '',
    value: t.quantity || 0,
  }));
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
            <span></span><span></span><span></span><span></span>
          </div>
          <!-- 折线 + 渐变填充 + 数据点 -->
          <svg v-if="trend.length" viewBox="0 0 640 180" role="img" aria-hidden="true" class="line-chart-svg">
            <defs>
              <linearGradient id="chart-grad" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stop-color="var(--color-primary)" stop-opacity="0.25" />
                <stop offset="100%" stop-color="var(--color-primary)" stop-opacity="0.00" />
              </linearGradient>
            </defs>
            <!-- 渐变填充区域 -->
            <polygon
              :points="chartFillPoints"
              fill="url(#chart-grad)"
            />
            <!-- 趋势折线 -->
            <polyline
              :points="chartPoints"
              fill="none" stroke="var(--color-primary)" stroke-width="3.5"
              stroke-linecap="round" stroke-linejoin="round"
            />
            <!-- 数据点圆圈 -->
            <circle
              v-for="(pt, i) in parsedPoints"
              :key="'circle-' + i"
              :cx="pt.x"
              :cy="pt.y"
              r="5.5"
              fill="#ffffff"
              stroke="var(--color-primary)"
              stroke-width="3"
              class="chart-dot"
            />
            <!-- 数据值标注 -->
            <text
              v-for="(pt, i) in parsedPoints"
              :key="'text-' + i"
              :x="pt.x"
              :y="pt.y - 12"
              text-anchor="middle"
              font-size="11"
              font-weight="700"
              fill="var(--color-text)"
              class="chart-val"
            >
              {{ pt.value }}
            </text>
          </svg>
          <!-- 底部的日期轴 -->
          <div class="date-row" v-if="trend.length">
            <div
              v-for="(b, i) in barItems"
              :key="i"
              class="date-col"
            >
              <span class="bar-date">{{ b.date }}</span>
            </div>
          </div>
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
}

.chart-lines span {
  border-top: 1px solid #e8edf4;
}

.line-chart-svg {
  position: absolute;
  inset: 0 0 30px;
  z-index: 2;
  width: 100%;
  height: calc(100% - 30px);
}

.chart-dot {
  transition: r 0.2s ease, stroke-width 0.2s ease;
  cursor: pointer;
}

.chart-dot:hover {
  r: 7.5;
  stroke-width: 4;
}

.chart-val {
  font-family: inherit;
  pointer-events: none;
}

.date-row {
  position: absolute;
  bottom: 12px;
  left: 4.375%;
  right: 4.375%;
  display: flex;
  justify-content: space-between;
  z-index: 1;
}

.date-col {
  display: flex;
  flex-direction: column;
  align-items: center;
  width: 0;
  overflow: visible;
}

.bar-date {
  font-size: 11px;
  color: var(--color-muted);
  white-space: nowrap;
  transform: translateX(-50%);
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
