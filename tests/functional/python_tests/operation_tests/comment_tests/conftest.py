import pytest

from ..conftest import CommentAccount


@pytest.fixture
def create_account_object():
    return CommentAccount
