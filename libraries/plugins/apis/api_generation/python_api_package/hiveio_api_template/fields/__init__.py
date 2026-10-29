"""
Field types of Hive data - in three variants of one set of names, each in its own package:

- `validation` - forms checked by msgspec itself (validation models generated in `hiveio_api`),
- `user_friendly` - types checked only when created (`AccountName(".")` raises), used by hand-written code,
- `public` - plain builtins without any checks (public models generated in `hiveio_api`), `Asset`, information about
  kinds of assets.

Types with their own JSON form decoded by the hooks of hand-written models (`HiveDateTime`, `JsonString`) are shared by
the variants. The package stays import-light (no msgspec) - import the variants from their packages.
"""
