<script setup>
import { computed, reactive, ref } from 'vue';
import { LockKeyhole, UserRound } from 'lucide-vue-next';

const props = defineProps({
  mode: {
    type: String,
    default: 'login',
    validator: (value) => ['login', 'register'].includes(value)
  },
  loading: {
    type: Boolean,
    default: false
  }
});

const emit = defineEmits(['submit']);

const form = reactive({
  username: '',
  realName: '',
  password: '',
  confirmPassword: ''
});

const error = ref('');
const isRegister = computed(() => props.mode === 'register');
const submitText = computed(() => (isRegister.value ? '创建账号' : '登录系统'));

const submit = () => {
  error.value = '';

  if (!form.username.trim() || !form.password) {
    error.value = '请输入账号和密码';
    return;
  }

  if (isRegister.value && form.password !== form.confirmPassword) {
    error.value = '两次输入的密码不一致';
    return;
  }

  emit('submit', { ...form });
};
</script>

<template>
  <form class="auth-form" @submit.prevent="submit">
    <label class="auth-field">
      <span>账号</span>
      <div class="auth-input">
        <UserRound />
        <input v-model.trim="form.username" type="text" autocomplete="username" placeholder="请输入账号" />
      </div>
    </label>

    <label v-if="isRegister" class="auth-field">
      <span>姓名</span>
      <div class="auth-input">
        <UserRound />
        <input v-model.trim="form.realName" type="text" autocomplete="name" placeholder="请输入姓名" />
      </div>
    </label>

    <label class="auth-field">
      <span>密码</span>
      <div class="auth-input">
        <LockKeyhole />
        <input v-model="form.password" type="password" autocomplete="current-password" placeholder="请输入密码" />
      </div>
    </label>

    <label v-if="isRegister" class="auth-field">
      <span>确认密码</span>
      <div class="auth-input">
        <LockKeyhole />
        <input v-model="form.confirmPassword" type="password" autocomplete="new-password" placeholder="请再次输入密码" />
      </div>
    </label>

    <p v-if="error" class="message error">{{ error }}</p>

    <button class="btn btn-primary auth-submit" type="submit" :disabled="loading">
      {{ loading ? '处理中' : submitText }}
    </button>
  </form>
</template>

<style scoped>
.auth-form {
  display: grid;
  gap: 18px;
}

.auth-field {
  display: grid;
  gap: 8px;
}

.auth-field span {
  color: #3d4a5e;
  font-size: 14px;
  font-weight: 800;
}

.auth-input {
  min-height: 48px;
  display: flex;
  align-items: center;
  gap: 10px;
  border: 1px solid var(--color-line);
  border-radius: 8px;
  padding: 0 13px;
  background: #fbfdff;
}

.auth-input:focus-within {
  border-color: #73b4ff;
  box-shadow: 0 0 0 3px rgba(37, 121, 237, 0.12);
}

.auth-input svg {
  width: 18px;
  height: 18px;
  color: #8794a7;
  flex-shrink: 0;
}

.auth-input input {
  width: 100%;
  min-width: 0;
  border: 0;
  outline: 0;
  background: transparent;
  color: var(--color-text);
}

.auth-submit {
  width: 100%;
  min-height: 48px;
  margin-top: 2px;
}
</style>
