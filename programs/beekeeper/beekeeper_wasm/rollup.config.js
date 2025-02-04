import dts from 'rollup-plugin-dts';
import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import replace from '@rollup/plugin-replace';
import terser from '@rollup/plugin-terser';
import copy from 'rollup-plugin-copy';

export default [
  {
    input: `dist/detailed/index.js`,
    output: {
      format: 'es',
      file: `dist/bundle/detailed/index.js`
    },
    plugins: [
      replace({
        values: {
          'process.env.npm_package_version': `"${process.env.npm_package_version}"`
        },
        preventAssignment: true
      }),
      nodeResolve({ preferBuiltins: false, browser: false }),
      commonjs()
    ]
  },
  {
    input: 'dist/index.js',
    output: {
      format: 'es',
      file: 'dist/bundle/web.js'
    },
    external: [
      './build/beekeeper_wasm.web.js',
      './detailed/index.js'
    ],
    plugins: [
      replace({
        delimiters: ['[\'"]','[\'"]'],
        values: {
          './beekeeper_module.js': '"./build/beekeeper_wasm.web.js"'
        },
        preventAssignment: true
      }),
      replace({
        values: {
          'process.env.DEFAULT_STORAGE_ROOT': `"/storage_root"`,
          'process.env.ROLLUP_TARGET_ENV': `"web"`
        },
        preventAssignment: true
      })

    ]
  },
  {
    input: 'dist/index.js',
    output: {
      format: 'es',
      file: 'dist/bundle/node.js'
    },
    external: [
      './build/beekeeper_wasm.node.js',
      './detailed/index.js'
    ],
    plugins: [
      copy({
        targets: [{ src: ['build/beekeeper.common.wasm', 'build/beekeeper_wasm.*.js'], dest: 'dist/bundle/build' }]
      }),

      replace({
        delimiters: ['[\'"]', '[\'"]'],
        values: {
          './beekeeper_module.js': '"./build/beekeeper_wasm.node.js"'
        },
        preventAssignment: true
      }),
      replace({
        values: {
          'process.env.DEFAULT_STORAGE_ROOT': `"./storage_root-node"`,
          'process.env.ROLLUP_TARGET_ENV': `"node"`
        },
        preventAssignment: true
      })

    ]
  },
  {
    input: 'dist/vite.js',
    output: {
      format: 'es',
      file: 'dist/bundle/vite.js'
    },
    external: [
      './build/beekeeper_wasm.node.js',
      './detailed/index.js',
      './beekeeper.common.wasm?url'
    ],
    plugins: [
      replace({
        delimiters: ['[\'"]', '[\'"]'],
        values: {
          './beekeeper_module.js': '"./build/beekeeper_wasm.node.js"',
          'beekeeper.common.wasm?url': '"./beekeeper.common.wasm?url"'
        },
        preventAssignment: true
      })
    ]
  },
  {
    input: `dist/index.d.ts`,
    output: [
      { file: `dist/bundle/index.d.ts`, format: "es" }
    ],
    plugins: [
      dts()
    ]
  }
];
