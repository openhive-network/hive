<script setup lang="ts">
const { data } = useFetch('/api/get-version');

const version = ref("loading...");

if (process.client) {
  import("@hiveio/beekeeper/vite").then(async ({ createBeekeeper }) => {
    const bk = await createBeekeeper({ inMemory: true, enableLogs: false });
    version.value = bk.getVersion();
  });
}
</script>

<template>
  <div>
    <h3>SSR Version:</h3>
    {{ data.version }}
    <h3>Client version:</h3>
    {{ version }}
  </div>
</template>
