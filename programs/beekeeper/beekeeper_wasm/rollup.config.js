import dts from 'rollup-plugin-dts';
import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import replace from '@rollup/plugin-replace';
import terser from '@rollup/plugin-terser';
import copy from 'rollup-plugin-copy';

export default [
  {
    input: 'build/beekeeper_wasm.web.js',
    output: {
      format: 'es',
      file: 'dist/bundle/build/beekeeper_wasm.web.js'
    },
    plugins: [
      replace({
        values: {
          'beekeeper_wasm.web.wasm': 'beekeeper.common.wasm'
        },
        preventAssignment: true
      }),
      terser({
        format: {
          inline_script: false,
          comments: false,
          max_line_len: 100
        }
      })
    ]
  },
  {
    input: 'build/beekeeper_wasm.node.js',
    output: {
      format: 'es',
      file: 'dist/bundle/build/beekeeper_wasm.node.js'
    },
    plugins: [
      copy({
        targets: [{ src: ['build/beekeeper_wasm.node.wasm'], dest: 'dist/bundle/build', rename: 'beekeeper.common.wasm' }]
      }),
      replace({
        values: {
          'beekeeper_wasm.node.wasm': 'beekeeper.common.wasm'
        },
        preventAssignment: true
      }),
      terser({
        format: {
          inline_script: false,
          comments: false,
          max_line_len: 100
        }
      })
    ]
  },
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
        values: {
          './beekeeper_module.js': './build/beekeeper_wasm.web.js',
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
      replace({
        values: {
          './beekeeper_module.js': './build/beekeeper_wasm.node.js',
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
        values: {
          './beekeeper_module.js': './build/beekeeper_wasm.node.js',
          'beekeeper.common.wasm?url': './beekeeper.common.wasm?url'
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
