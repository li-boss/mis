<template>
  <main class="dashboard-page">
    <header class="page-header">
      <h1>库存看板</h1>
      <el-button :loading="loading" @click="loadDashboard">刷新</el-button>
    </header>

    <section class="chart-grid">
      <div ref="rankingChartRef" class="chart"></div>
      <div ref="trendChartRef" class="chart"></div>
    </section>
  </main>
</template>

<script setup>
import { nextTick, onMounted, ref } from 'vue';
import * as echarts from 'echarts';
import { ElMessage } from 'element-plus';

import { fetchInventoryDashboard } from '../api/inventory';

const loading = ref(false);
const rankingChartRef = ref();
const trendChartRef = ref();

let rankingChart;
let trendChart;

const renderCharts = (data) => {
  const topItems = data?.topItems || [];
  const trend = data?.trend || [];

  rankingChart.setOption({
    title: { text: '库存量排名', left: 16, top: 12 },
    tooltip: {},
    grid: { left: 48, right: 24, bottom: 40, top: 64 },
    xAxis: { type: 'category', data: topItems.map((item) => item.skuId) },
    yAxis: { type: 'value' },
    series: [{ type: 'bar', data: topItems.map((item) => item.quantity), itemStyle: { color: '#409eff' } }],
  });

  trendChart.setOption({
    title: { text: '近期出入库趋势', left: 16, top: 12 },
    tooltip: { trigger: 'axis' },
    grid: { left: 48, right: 24, bottom: 40, top: 64 },
    xAxis: { type: 'category', data: trend.map((item) => item.date) },
    yAxis: { type: 'value' },
    series: [{ type: 'line', smooth: true, data: trend.map((item) => item.quantity), itemStyle: { color: '#67c23a' } }],
  });
};

const loadDashboard = async () => {
  loading.value = true;

  try {
    const response = await fetchInventoryDashboard();
    renderCharts(response.data);
  } catch {
    ElMessage.error('看板数据加载失败');
  } finally {
    loading.value = false;
  }
};

onMounted(async () => {
  await nextTick();
  rankingChart = echarts.init(rankingChartRef.value);
  trendChart = echarts.init(trendChartRef.value);
  await loadDashboard();
});
</script>

<style scoped>
.dashboard-page {
  min-height: 100vh;
  padding: 32px;
  background: #f5f7fa;
}

.page-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 24px;
}

.page-header h1 {
  margin: 0;
  font-size: 24px;
  font-weight: 600;
  color: #1f2d3d;
}

.chart-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 16px;
}

.chart {
  height: 360px;
  background: #ffffff;
  border: 1px solid #e4e7ed;
  border-radius: 8px;
}

@media (max-width: 900px) {
  .chart-grid {
    grid-template-columns: 1fr;
  }
}
</style>
