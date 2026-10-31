from __future__ import annotations

from hiveio_api.fields.validation import HardforkVersion, NodeType, Revision
from hiveio_api.validation.model import PreconfiguredBaseModel

__all__ = [
    "HiveVersionFromExecutable",
]


class HiveVersionFromExecutable(PreconfiguredBaseModel):
    """Version of hived given by its executable (`hived --version`) - strings checked by the forms of validation."""

    blockchain_version: HardforkVersion
    hive_revision: Revision
    fc_revision: Revision
    node_type: NodeType
    haf_revision: Revision | None = None
