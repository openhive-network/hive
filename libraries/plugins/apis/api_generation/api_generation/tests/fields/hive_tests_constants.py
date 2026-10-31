"""File with constants to test hive operations"""

from __future__ import annotations

from typing import Any, Final

OWNER: Final[dict[str, Any]] = {
    "weight_threshold": 1,
    "account_auths": [],
    "key_auths": [["STM65PUAPA4yC4RgPtGgsPupxT6yJtMhmT5JHFdsT3uoCbR8WJ25s", 1]],
}
ACTIVE: Final[dict[str, Any]] = {
    "weight_threshold": 1,
    "account_auths": [],
    "key_auths": [["STM69zfrFGnZtU3gWFWpQJ6GhND1nz7TJsKBTjcWfebS1JzBEweQy", 1]],
}
POSTING: Final[dict[str, Any]] = {
    "weight_threshold": 1,
    "account_auths": [["threespeak", 1], ["vimm.app", 1]],
    "key_auths": [["STM6vJmrwaX5TjgTS9dPH8KsArso5m91fVodJvv91j7G765wqcNM9", 1]],
}
HIVE_EXAMPLE_DATE: Final[str] = "1970-01-01T00:00:00"
MEMO_EXAMPLE_KEY: Final[str] = "STM7wrsg1BZogeK7X3eG4ivxmLaH69FomR8rLkBbepb3z3hm5SbXu"
