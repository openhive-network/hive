def is_in_dev_mode() -> bool:
    from clive_config import settings

    return settings.get("dev", False)  # type: ignore[no-any-return]