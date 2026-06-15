<script setup>
import { reactive, ref, watch } from 'vue';

const props = defineProps({
  supplier: {
    type: Object,
    default: null
  },
  loading: {
    type: Boolean,
    default: false
  }
});

const emit = defineEmits(['submit', 'cancel']);

const emptyForm = {
  supplierCode: '',
  name: '',
  contactName: '',
  phone: '',
  rating: 'B',
  status: 'active',
  address: '',
  remark: ''
};

const form = reactive({ ...emptyForm });
const error = ref('');

watch(
  () => props.supplier,
  (supplier) => {
    Object.assign(form, emptyForm, supplier || {});
    error.value = '';
  },
  { immediate: true }
);

const submit = () => {
  error.value = '';

  if (!form.supplierCode.trim() || !form.name.trim()) {
    error.value = '请填写供应商编号和名称';
    return;
  }

  emit('submit', { ...form });
};
</script>

<template>
  <form class="supplier-form" @submit.prevent="submit">
    <div class="grid-2">
      <label class="field">
        <span>供应商编号</span>
        <input v-model.trim="form.supplierCode" class="input" placeholder="例如 SUP-HD-001" />
      </label>

      <label class="field">
        <span>供应商名称</span>
        <input v-model.trim="form.name" class="input" placeholder="请输入供应商名称" />
      </label>

      <label class="field">
        <span>联系人</span>
        <input v-model.trim="form.contactName" class="input" placeholder="请输入联系人" />
      </label>

      <label class="field">
        <span>联系电话</span>
        <input v-model.trim="form.phone" class="input" placeholder="请输入联系电话" />
      </label>

      <label class="field">
        <span>评级</span>
        <select v-model="form.rating" class="select">
          <option value="A">A</option>
          <option value="B">B</option>
          <option value="C">C</option>
        </select>
      </label>

      <label class="field">
        <span>状态</span>
        <select v-model="form.status" class="select">
          <option value="active">合作中</option>
          <option value="paused">暂停</option>
        </select>
      </label>
    </div>

    <label class="field">
      <span>地址</span>
      <input v-model.trim="form.address" class="input" placeholder="请输入地址" />
    </label>

    <label class="field">
      <span>备注</span>
      <textarea v-model.trim="form.remark" class="textarea" placeholder="请输入备注"></textarea>
    </label>

    <p v-if="error" class="message error">{{ error }}</p>

    <div class="form-actions">
      <button class="btn btn-ghost" type="button" @click="$emit('cancel')">取消</button>
      <button class="btn btn-primary" type="submit" :disabled="loading">
        {{ loading ? '保存中' : '保存供应商' }}
      </button>
    </div>
  </form>
</template>

<style scoped>
.supplier-form {
  display: grid;
  gap: 16px;
}
</style>
