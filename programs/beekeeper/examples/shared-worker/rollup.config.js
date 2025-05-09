// Used to resolve node_modules imports
import resolve from "@rollup/plugin-node-resolve";

// Required for replacing `import.meta.url` to `self.location.href` in
// `new URL('...', import.meta.url)` syntax - @hiveio/beeekeeper bundled inside the shared worker uses it
import replace from "@rollup/plugin-replace";

// Required for `new URL('...', import.meta.url)` syntax - @hiveio/beeekeeper uses it
import { importMetaAssets } from '@web/rollup-plugin-import-meta-assets';

export default [
  {
    input: ["dist/worker.js"],
    output: {
      dir: 'dist/bundle',
      format: "es"
    },
    plugins: [
      importMetaAssets(),
      resolve({
        moduleDirectories: ["node_modules"]
      }),
      replace({
        delimiters: ["", ""],
        values: {
          'import.meta.url': 'self.location.href'
        },
        preventAssignment: true
      })
    ]
  }
];
