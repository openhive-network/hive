from datetime import datetime
from pathlib import Path
from typing import Protocol, Optional

import test_tools as tt


class ReplayedNodeMaker(Protocol):
    def __call__(
        self,
        block_log_directory: Path,
        *,
        absolute_start_time: Optional[datetime] = None,
        time_multiplier: Optional[float] = None,
        timeout: float = tt.InitNode.DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    ) -> tt.InitNode:
        pass
