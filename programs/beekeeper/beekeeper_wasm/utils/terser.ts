import type { MinifyOptions } from "terser";
import { minify } from "terser";
import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { resolve, join, dirname } from "node:path";
import { fileURLToPath } from 'node:url';
import { existsSync } from "node:fs";

const __dirname = dirname(fileURLToPath(import.meta.url));

const packageJsonLocation = join(process.cwd(), "package.json");
const { files } = JSON.parse(readFileSync(packageJsonLocation, { encoding: "utf-8" }));

// =============== #1 ANALYZE "files" in package.json and collect all .js sources ===============
const jsSources: string[] = [];
const analyzePath = (path: string) => {
  if (!existsSync(path)) {
    console.warn(`The path ${path} does not exist`);

    return;
  }

  if (statSync(path).isFile()) {
    if (path.endsWith(".js"))
      jsSources.push(resolve(__dirname, path));

    return;
  }

  for(const file of readdirSync(path, { withFileTypes: true }))
    analyzePath(join(file.parentPath, file.name));
}

for(const file of files)
  analyzePath(resolve(file));

// =============== #2 Define terser minification configuration ===============

const minifyOptions: MinifyOptions = {
  compress: true,
  mangle: false,
  ecma: 2020,
  ie8: false,
  keep_classnames: true,
  keep_fnames: true,
  sourceMap: false,
  format: {
    inline_script: false,
    comments: false,
    max_line_len: 100
  }
};

// =============== #3 Minify the files ===============

for(const file of jsSources) {
  const inputCode = readFileSync(file, { encoding: "utf-8" });

  void minify(inputCode, minifyOptions).then(({ code }) => {
    if (!code)
      throw new Error(`Failed to minify the file ${file}`);

    writeFileSync(file, code, { encoding: "utf-8" });

    console.log(`Minified ${file}`);
  });
}
