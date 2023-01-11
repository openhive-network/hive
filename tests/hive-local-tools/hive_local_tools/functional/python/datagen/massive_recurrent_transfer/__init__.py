import math
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path
from typing import Callable, Iterable, Optional, Protocol, Sequence

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


def execute_function_in_threads(
    function: Callable,
    *,
    amount: int,
    args: Optional[Iterable] = None,
    args_sequences: Optional[Iterable[Sequence]] = None,
    chunk_size: Optional[int] = None,
    max_workers: Optional[int] = None,
) -> None:
    """
    Execute the given function in threads.

    Example:
        letters = ['a', 'b', 'c', 'd']

        def function(x: int, y: float, z: List[str]) -> None:
            print(x, y, *z)

        execute_function_in_threads(
            function, amount=len(letters), args=(1, 2.0), args_sequences=(letters,), chunk_size=2
        )

        Output:
            1 2.0 a b
            1 2.0 c d

    :param function: Function to be called.
    :param args: Arguments for the function. If given, the function will be called with the same arguments for
        each thread.
    :param args_sequences: Sequenced arguments for the function (like tuple or list). If given, the function will be
        called with sequenced arguments, automatically dividing their length by chunk size, evenly distributing the work
        among workers. See the example above.
    :param amount: Amount of times the function will be called or length of the sequences given in the args_sequential
        parameter (so there will be `n = amount/chunk_size` calls), but only when chunk_size was specified.
    :param chunk_size: Size of the chunks. Should be specified if the args_sequential parameter is given.
    :param max_workers: Maximum amount of workers used in ThreadPoolExecutor. Defaults ThreadPoolExecutor's default.
    """
    args = args or []
    args_sequences = args_sequences or []
    chunk_size = chunk_size or 1
    max_workers = math.inf if chunk_size and not max_workers else max_workers

    with ThreadPoolExecutor(max_workers=min(math.ceil(amount / chunk_size), max_workers)) as executor:
        futures = []
        for lower in range(0, amount, chunk_size):
            upper = min(amount + 1, lower + chunk_size)

            sequences = [sequence[lower:upper] for sequence in args_sequences]
            futures.append(executor.submit(function, *args, *sequences))

            detail = f" with elements of index: <{lower}:{upper - 1}>" if chunk_size > 1 else f": {lower + 1}/{amount}"
            tt.logger.info(f"Pack submitted{detail}")

        for index, future in enumerate(as_completed(futures)):
            tt.logger.info(f"Pack finished: {index + 1}/{len(futures)}")
            if exception := future.exception():
                for f in futures:
                    f.cancel()
                raise exception
