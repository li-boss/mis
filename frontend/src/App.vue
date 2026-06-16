<script setup>
import { computed, onMounted, onUnmounted } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import NavBar from './components/NavBar.vue';
import Sidebar from './components/Sidebar.vue';
import { useUserStore } from './store/user';

const route = useRoute();
const router = useRouter();
const userStore = useUserStore();
const isPublicPage = computed(() => Boolean(route.meta.public));

const handleAuthExpired = () => {
  if (!route.meta.public) {
    router.push('/login');
  }
};

onMounted(async () => {
  window.addEventListener('wms:auth-expired', handleAuthExpired);
  // 始终验证 token 有效性，防止使用过期 token 自动登录
  if (userStore.token) {
    await userStore.fetchProfile();
    // 验证通过且当前在公开页 → 跳转到看板
    if (userStore.isAuthenticated && route.meta.public && route.name !== 'Register') {
      router.push('/dashboard');
    }
  }
});

onUnmounted(() => {
  window.removeEventListener('wms:auth-expired', handleAuthExpired);
});
</script>

<template>
  <div class="app-shell" :class="{ 'auth-shell': isPublicPage }">
    <RouterView v-if="isPublicPage" />
    <template v-else>
      <Sidebar />
      <main class="workspace">
        <NavBar />
        <RouterView />
      </main>
    </template>
  </div>
</template>
