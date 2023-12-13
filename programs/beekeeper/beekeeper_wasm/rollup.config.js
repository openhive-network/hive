import typescript from 'rollup-plugin-typescript2';
import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';

export default {
  input: 'dist/index.js',
  output: {
    dir: 'dist/bundle',
    format: 'es',
    name: 'beekeeper'
  },
  plugins: [
    nodeResolve({ preferBuiltins: false, browser: true }),
    typescript({
      rollupCommonJSResolveHack: false,
      clean: true
    }),
    commonjs()
  ]
};
