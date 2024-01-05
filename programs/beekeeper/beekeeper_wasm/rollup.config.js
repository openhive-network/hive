import typescript from 'rollup-plugin-typescript2';
import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import replace from '@rollup/plugin-replace';
import alias from '@rollup/plugin-alias';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const commonConfiguration = (env, merge = {}) => ({
  input: `dist/${env}.js`,
  output: {
    format: 'es',
    name: 'beekeeper',
    ...(merge.output || {})
  },
  plugins: [
    alias({
      entries: [
        { find: 'beekeeper_wasm/beekeeper_wasm.web.js', replacement: path.resolve(__dirname, `./build/beekeeper_wasm.${env}.js`) }
      ]
    }),
    replace({
      'process.env.ROLLUP_TARGET_ENV': `"${env}"`,
      preventAssignment: true
    }),
    nodeResolve({ preferBuiltins: env !== "web", browser: env === "web" }),
    commonjs(),
    ...(merge.plugins || [])
  ]
});

export default [
  commonConfiguration('node', { output: { file: 'dist/bundle/node.js' } }),
  commonConfiguration('web', { output: { dir: 'dist/bundle' }, plugins: [
    typescript({
      rollupCommonJSResolveHack: false,
      clean: true
    }) // We only need one typescript documentation, as it is the same as for node
    ]
  })
];
