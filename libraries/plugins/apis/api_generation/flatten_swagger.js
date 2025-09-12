#!/usr/bin/env node
const fs = require("fs");
const path = require("path");
const $RefParser = require("@apidevtools/json-schema-ref-parser");
const mergeAllOf = require("json-schema-merge-allof");

/**
 * Recursively flatten allOf/oneOf/anyOf in a schema
 */
function flattenSchema(schema) {
  if (!schema || typeof schema !== "object") return schema;

  // Handle allOf
  if (schema.allOf) {
    schema = mergeAllOf(schema, { resolvers: {} });
  }

  // Handle anyOf (keep the first option for now)
  if (schema.anyOf) {
    schema = { ...schema, ...schema.anyOf[0] };
    delete schema.anyOf;
  }

  // Recurse into properties/items/etc
  for (const key of Object.keys(schema)) {
    if (typeof schema[key] === "object") {
      schema[key] = flattenSchema(schema[key]);
    }
  }

  return schema;
}

async function main() {
  const filePath = process.argv[2];
  if (!filePath) {
    console.error("Usage: node flatten-schema.js <schema.json>");
    process.exit(1);
  }

  const absPath = path.resolve(filePath);

  try {
    let schema = await $RefParser.dereference(absPath);
    schema = flattenSchema(schema);
    console.log(JSON.stringify(schema, null, 2));
  } catch (err) {
    console.error("Error:", err);
    process.exit(1);
  }
}

main();