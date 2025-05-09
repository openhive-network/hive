import createBeekeeper from "@hiveio/beekeeper";

const bkAsync = createBeekeeper({ inMemory: true, enableLogs: false });

export default defineEventHandler(async() => ({
  version: (await bkAsync).getVersion()
}));