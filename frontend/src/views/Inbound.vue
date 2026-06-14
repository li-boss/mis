<script setup>
import { ref } from 'vue';
import OrderForm from '../components/OrderForm.vue';
import OrderTable from '../components/OrderTable.vue';

const tableRef = ref(null);

const onOrderCreated = (inboundId) => {
  tableRef.value?.refresh();
};
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">入库管理</h1>
        <p class="page-subtitle">采购订单创建、提交、收货确认与状态流转。</p>
      </div>
    </div>

    <!-- 新建入库单 -->
    <OrderForm @created="onOrderCreated" />

    <!-- 入库单列表 & 操作 -->
    <OrderTable ref="tableRef" />
  </section>
</template>
