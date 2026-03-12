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
    schema = mergeAllOf(schema, {
      resolvers: {
        // When allOf contains incompatible types (e.g. extensions object vs array),
        // use the last value as an override (condenser_api extensions_legacy pattern).
        type: function (values) {
          return values[values.length - 1];
        },
      },
    });
  }

  // Handle anyOf: for nullable patterns (anyOf: [null, X]), keep the anyOf intact
  // so datamodel-code-generator can produce proper Optional types.
  // For other anyOf patterns, flatten to the first non-null option.
  if (schema.anyOf) {
    const nullIdx = schema.anyOf.findIndex(
      (s) => s.type === "null" || (Array.isArray(s.type) && s.type.includes("null"))
    );
    const nonNullOptions = schema.anyOf.filter((_, i) => i !== nullIdx);
    if (nullIdx !== -1 && nonNullOptions.length === 1) {
      // Nullable pattern: anyOf [null, X] — keep as anyOf so generator handles it
      schema.anyOf = [{ type: "null" }, flattenSchema(nonNullOptions[0])];
    } else {
      // Non-nullable anyOf: flatten to first option (original behavior)
      schema = { ...schema, ...schema.anyOf[0] };
      delete schema.anyOf;
    }
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