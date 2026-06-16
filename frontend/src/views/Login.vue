<script setup>
import { ref } from 'vue';
import { RouterLink, useRoute, useRouter } from 'vue-router';
import AuthForm from '../components/AuthForm.vue';
import { useUserStore } from '../store/user';

const route = useRoute();
const router = useRouter();
const userStore = useUserStore();
const error = ref('');

const submit = async (payload) => {
  error.value = '';

  try {
    await userStore.login(payload);
    router.push(route.query.redirect || '/dashboard');
  } catch (err) {
    error.value = err.message || '登录失败';
  }
};
</script>

<template>
  <main class="auth-page">
    <section class="auth-card">
      <div class="auth-brand">
        <span class="brand-mark"></span>
        <div>
          <h1>库存管理系统</h1>
          <p>MIS Inventory</p>
        </div>
      </div>

      <div class="auth-copy">
        <h2>登录</h2>
        <p>进入用户、SKU 与供应商管理工作台。</p>
      </div>

      <p v-if="error" class="message error">{{ error }}</p>
      <AuthForm mode="login" :loading="userStore.loading" @submit="submit" />

      <p class="auth-switch">
        没有账号
        <RouterLink to="/register">去注册</RouterLink>
      </p>
    </section>
  </main>
</template>

<style scoped>
.auth-page {
  min-height: 100vh;
  display: grid;
  place-items: center;
  padding: 24px;
  background: linear-gradient(135deg, #0f172a 0%, #1e293b 100%);
}

.auth-card {
  width: min(460px, 100%);
  padding: 34px;
  border: 1px solid var(--color-line);
  border-radius: 10px;
  background: var(--color-surface);
  box-shadow: var(--shadow-soft);
}

.auth-brand {
  display: flex;
  align-items: center;
  gap: 14px;
}

.brand-mark {
  width: 44px;
  height: 44px;
  display: inline-block;
  border-radius: 8px;
  background: var(--color-primary);
  box-shadow: inset 0 0 0 10px rgba(255, 255, 255, 0.18);
}

.auth-brand h1,
.auth-copy h2,
.auth-brand p,
.auth-copy p {
  margin: 0;
}

.auth-brand h1 {
  font-size: 24px;
}

.auth-brand p,
.auth-copy p {
  color: var(--color-muted);
}

.auth-copy {
  margin: 34px 0 22px;
}

.auth-copy h2 {
  font-size: 30px;
}

.auth-copy p {
  margin-top: 8px;
}

.auth-switch {
  margin: 18px 0 0;
  color: var(--color-muted);
  text-align: center;
}

.auth-switch a {
  color: var(--color-primary);
  font-weight: 800;
}

@media (max-width: 760px) {
  .auth-page {
    background: var(--color-bg);
  }

  .auth-card {
    padding: 26px;
  }
}
</style>
