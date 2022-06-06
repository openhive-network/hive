import functools
import inspect
from typing import Callable, Dict, Iterable

import pytest

import test_tools as tt


def __serialize_legacy(assets: Iterable) -> Iterable[str]:
    return (str(asset) for asset in assets)


def __serialize_modern(assets: Iterable) -> Iterable[Dict]:
    return (asset.as_nai() for asset in assets)


def run_for_all_cases(**assets):
    """
    Runs decorated test four times:
    - asserts that wallet with matching assets doesn't raise any exception:
        1. with wallet using legacy serialization and assets in legacy format,
        2. with wallet using modern serialization and assets in modern (nai) format,
    - asserts that wallet with mismatched assets raises exception:
        3. with wallet using legacy serialization and assets in modern (nai) format,
        4. with wallet using modern serialization and assets in legacy format.
    """
    def __decorator(test: Callable):
        old_test_signature = inspect.signature(test)
        test.__signature__ = old_test_signature.replace(
            parameters=[
                inspect.Parameter('description', inspect.Parameter.POSITIONAL_OR_KEYWORD, annotation=str),
                inspect.Parameter('formats_matches', inspect.Parameter.POSITIONAL_OR_KEYWORD, annotation=bool),
                *old_test_signature.parameters.values(),
            ],
        )

        @pytest.mark.parametrize(
            f'description, formats_matches, prepared_wallet{"".join([f", {key}" for key in assets.keys()])}',
            [
                ('legacy wallet and legacy assets (matched)', True, 'legacy', *__serialize_legacy(assets.values())),
                ('modern wallet and modern assets (matched)', True, 'modern', *__serialize_modern(assets.values())),
                ('legacy wallet and modern assets (mismatched)', False, 'legacy', *__serialize_modern(assets.values())),
                ('modern wallet and legacy assets (mismatched)', False, 'modern', *__serialize_legacy(assets.values())),
            ],
            indirect=['prepared_wallet'],
        )
        @functools.wraps(test)
        def __decorated_test(description, formats_matches, **kwargs):
            tt.logger.info(f'Running {test.__name__} -- {description}')
            if formats_matches:
                return test(**kwargs)

            with pytest.raises(tt.exceptions.CommunicationError):
                test(**kwargs)

        return __decorated_test

    return __decorator
