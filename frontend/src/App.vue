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

onMounted(() => {
  window.addEventListener('wms:auth-expired', handleAuthExpired);
  if (userStore.token && !userStore.profile) {
    userStore.fetchProfile();
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
