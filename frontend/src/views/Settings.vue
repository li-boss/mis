<script setup>
import { onMounted, ref } from 'vue';
import { ElMessage, ElMessageBox } from 'element-plus';
import { listUsers, updateUserRole } from '../api/user';
import { useUserStore } from '../store/user';

const userStore = useUserStore();
const users = ref([]);
const loading = ref(false);

const roleOptions = [
  { value: 'admin', label: '系统管理员' },
  { value: 'keeper', label: '库管员' },
  { value: 'purchaser', label: '采购员' }
];

const roleLabel = (role) => {
  const opt = roleOptions.find((r) => r.value === role);
  return opt ? opt.label : role;
};

const isSelf = (userId) => userId === userStore.profile?.userId;

const loadUsers = async () => {
  loading.value = true;
  try {
    const result = await listUsers();
    users.value = result.list || [];
  } catch {
    // 鉴权失效时拦截器已处理
  } finally {
    loading.value = false;
  }
};

const handleRoleChange = async (user) => {
  if (isSelf(user.userId) && user.role !== 'admin') {
    ElMessage.warning('不可降级自己的管理员角色');
    return;
  }

  try {
    await ElMessageBox.confirm(
      `确认将「${user.realName || user.username}」的角色改为「${roleLabel(user.role)}」？`,
      '修改角色',
      { confirmButtonText: '确认', cancelButtonText: '取消', type: 'warning' }
    );

    await updateUserRole(user.userId, user.role);
    ElMessage.success('角色已更新');
  } catch {
    // 取消操作
  }
};

onMounted(loadUsers);
</script>

<template>
  <section class="page">
    <div class="page-head">
      <div>
        <h1 class="page-title">系统设置</h1>
        <p class="page-subtitle">用户管理与角色分配。</p>
      </div>
    </div>

    <el-card shadow="never">
      <template #header>
        <span class="card-title">用户列表</span>
      </template>

      <el-table :data="users" v-loading="loading" stripe style="width: 100%">
        <el-table-column prop="userId" label="ID" width="70" />
        <el-table-column prop="username" label="用户名" min-width="120" />
        <el-table-column prop="realName" label="姓名" min-width="100" />
        <el-table-column label="角色" min-width="220">
          <template #default="{ row }">
            <el-select
              :model-value="row.role"
              @change="(val) => { row.role = val; handleRoleChange(row); }"
              :disabled="isSelf(row.userId)"
              style="width: 180px"
            >
              <el-option
                v-for="opt in roleOptions"
                :key="opt.value"
                :label="opt.label"
                :value="opt.value"
              />
            </el-select>
          </template>
        </el-table-column>
        <el-table-column label="备注" min-width="140">
          <template #default="{ row }">
            <el-tag v-if="isSelf(row.userId)" type="info" size="small">当前用户</el-tag>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </section>
</template>

<style scoped>
.card-title {
  font-size: 16px;
  font-weight: 600;
}
</style>
