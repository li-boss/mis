<script setup>
import { Pencil, Power, Trash2 } from 'lucide-vue-next';

defineProps({
  items: {
    type: Array,
    default: () => []
  },
  loading: {
    type: Boolean,
    default: false
  }
});

defineEmits(['edit', 'remove', 'toggle-status']);

const statusText = {
  active: '启用',
  disabled: '停用'
};

const stockClass = (item) => {
  if (Number(item.currentStock) === 0) return 'tag-danger';
  if (Number(item.currentStock) < Number(item.safetyStock)) return 'tag-warning';
  return 'tag-success';
};
</script>

<template>
  <div class="data-table-wrap">
    <table class="data-table">
      <thead>
        <tr>
          <th>SKU 编号</th>
          <th>商品名称</th>
          <th>分类</th>
          <th>库存</th>
          <th>安全库存</th>
          <th>供应商</th>
          <th>状态</th>
          <th>操作</th>
        </tr>
      </thead>
      <tbody>
        <tr v-if="loading">
          <td class="table-empty" colspan="8">数据加载中</td>
        </tr>
        <tr v-else-if="items.length === 0">
          <td class="table-empty" colspan="8">暂无 SKU 数据</td>
        </tr>
        <template v-else>
          <tr v-for="item in items" :key="item.id">
            <td>
              <strong>{{ item.skuCode }}</strong>
            </td>
            <td>{{ item.name }}</td>
            <td>{{ item.category }}</td>
            <td>
              <span class="tag" :class="stockClass(item)">
                {{ item.currentStock }} {{ item.unit }}
              </span>
            </td>
            <td>{{ item.safetyStock }} {{ item.unit }}</td>
            <td>{{ item.supplierName }}</td>
            <td>
              <span class="tag" :class="item.status === 'active' ? 'tag-success' : 'tag-danger'">
                {{ statusText[item.status] || item.status }}
              </span>
            </td>
            <td>
              <div class="inline-actions">
                <button class="icon-button" type="button" title="编辑" @click="$emit('edit', item)">
                  <Pencil />
                </button>
                <button class="icon-button" type="button" title="切换状态" @click="$emit('toggle-status', item)">
                  <Power />
                </button>
                <button class="icon-button" type="button" title="删除" @click="$emit('remove', item)">
                  <Trash2 />
                </button>
              </div>
            </td>
          </tr>
        </template>
      </tbody>
    </table>
  </div>
</template>
