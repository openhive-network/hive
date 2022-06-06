from typing import Dict

import pytest


class Mismatched:
    """
    Marks asset to be in different format, then handled by wallet.
    E.g. if wallet uses modern format, then asset will be in legacy format.
    """
    def __init__(self, asset):
        self.asset = asset


def __serialize_asset(asset, *, serialization: str):
    def __serialize_as_legacy(asset_) -> str:
        return str(asset_)

    def __serialize_as_modern(asset_) -> Dict:
        return asset_.as_nai()

    if serialization == 'modern':
        return __serialize_as_modern(asset) if not isinstance(asset, Mismatched) else __serialize_as_legacy(asset.asset)
    elif serialization == 'legacy':
        return __serialize_as_legacy(asset) if not isinstance(asset, Mismatched) else __serialize_as_modern(asset.asset)
    else:
        raise RuntimeError(f'Unsupported argument value "{serialization}", should be "modern" or "legacy"')


def test_asset_serialization(**assets):
    """
    Run decorated test two times:
    - with wallet using legacy serialization and assets in legacy format,
    - with wallet using modern serialization and assets in modern (nai) format.

    If you need mismatched assets, e.g. legacy asset for modern wallet, use `Mismatched` wrapper.
    """
    return pytest.mark.parametrize(
        f'prepared_wallet{"".join([f", {key}" for key in assets.keys()])}',
        [
            ('legacy', *[__serialize_asset(asset, serialization='legacy') for asset in assets.values()]),
            ('modern', *[__serialize_asset(asset, serialization='modern') for asset in assets.values()]),
        ],
        indirect=['prepared_wallet'],
    )
