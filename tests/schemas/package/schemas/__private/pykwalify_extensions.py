from __future__ import annotations

import logging
from typing import List, TYPE_CHECKING

from pykwalify.core import Core

if TYPE_CHECKING:
    from pykwalify.rule import Rule


log = logging.getLogger(__name__)


def sequence_strictly_matches_rule(sequence: List, rule: Rule, path: str) -> bool:
    log.debug("sequence: %s", sequence)
    log.debug("rule: %s", rule)
    log.debug("path: %s", path)

    if len(sequence) != len(rule.sequence):
        raise AssertionError(
            f'Incorrect number of elements in sequence.\n'
            f'Expected: {[item.type for item in rule.sequence]}\n'
            f'Actual: {sequence}'
        )

    for item_index, item_schema_pair in enumerate(zip(sequence, rule.sequence)):
        item, schema = item_schema_pair

        core = Core(source_data=item, schema_data=schema.schema_str)
        core.validate(raise_exception=False)  # TODO: Try to use core._validate method, maybe it doesn't spam logs

        if core.errors:
            errors_description = ""
            for error in core.errors:
                error.path = fr'{path}\{item_index}'
                errors_description += f'\n{error}'

            raise AssertionError(f'Validation failed with following errors: {errors_description}')

    return True
