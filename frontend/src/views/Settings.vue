<script setup>
import { isMockEnabled } from '../api/request';

const apiBaseUrl = import.meta.env.VITE_API_BASE_URL || '/api';
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">系统设置</h1>
        <p class="page-subtitle">查看前端运行配置与接口联调状态。</p>
      </div>
    </div>

    <div class="settings-grid">
      <section class="panel">
        <div class="panel-body">
          <h2 class="panel-title">接口配置</h2>
          <dl class="config-list">
            <div>
              <dt>Base URL</dt>
              <dd>{{ apiBaseUrl }}</dd>
            </div>
            <div>
              <dt>Mock</dt>
              <dd>{{ isMockEnabled ? '启用' : '关闭' }}</dd>
            </div>
          </dl>
        </div>
      </section>

      <section class="panel">
        <div class="panel-body">
          <h2 class="panel-title">模块负责人</h2>
          <dl class="config-list">
            <div>
              <dt>姓名</dt>
              <dd>白沁禾</dd>
            </div>
            <div>
              <dt>范围</dt>
              <dd>核心配置、用户、SKU、供应商、接口文档</dd>
            </div>
          </dl>
        </div>
      </section>
    </div>
  </section>
</template>

<style scoped>
.settings-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 24px;
}

.config-list {
  display: grid;
  gap: 16px;
  margin: 20px 0 0;
}

.config-list div {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 18px;
  border-bottom: 1px solid var(--color-line);
  padding-bottom: 14px;
}

.config-list dt {
  color: var(--color-muted);
  font-weight: 800;
}

.config-list dd {
  margin: 0;
  color: var(--color-text);
  font-weight: 800;
  overflow-wrap: anywhere;
  text-align: right;
}

@media (max-width: 900px) {
  .settings-grid {
    grid-template-columns: 1fr;
  }
}
</style>
