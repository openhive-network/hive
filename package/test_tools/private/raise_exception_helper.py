import os
import signal
import threading
from typing import ClassVar, Optional


class RaiseExceptionHelper:
    __is_initialized: ClassVar[bool] = False
    __last_exception: ClassVar[Optional[Exception]] = None
    __lock: ClassVar[threading.Lock] = threading.Lock()

    @classmethod
    def __external_error_handler(cls, signal_number, current_stack_frame) -> None:
        with cls.__lock:
            if cls.__last_exception is None:
                # Default SIGINT handler raises KeyboardInterrupt, so below code is not executed
                signal.default_int_handler(signal_number, current_stack_frame)

            exception_to_raise = cls.__last_exception
            cls.__last_exception = None

            # pylint: disable=raising-bad-type
            # False positive; reported bad type [NoneType] will never be raised
            raise exception_to_raise

    @classmethod
    def initialize(cls) -> None:
        with cls.__lock:
            signal.signal(signal.SIGINT, cls.__external_error_handler)
            cls.__is_initialized = True

    @classmethod
    def raise_exception_in_main_thread(cls, exception: Exception) -> None:
        with cls.__lock:
            assert threading.current_thread() is not threading.main_thread()
            assert cls.__is_initialized, 'initialize() should be called before this function'

            cls.__last_exception = exception
            os.kill(os.getpid(), signal.SIGINT)
