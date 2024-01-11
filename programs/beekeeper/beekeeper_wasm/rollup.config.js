import dts from 'rollup-plugin-dts';
import { nodeResolve } from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import replace from '@rollup/plugin-replace';
import alias from '@rollup/plugin-alias';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const commonConfiguration = env => ([
  {
    input: `dist/${env}.js`,
    output: {
      format: 'es',
      name: 'beekeeper',
      file: `dist/bundle/${env}.js`
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
      commonjs()
    ]
  }, {
    input: `dist/${env}.d.ts`,
    output: [
      { file: `dist/bundle/${env}.d.ts`, format: "es" }
    ],
    plugins: [
      dts()
    ]
  }
]);

export default [
  ...commonConfiguration('node'),
  ...commonConfiguration('web')
];
