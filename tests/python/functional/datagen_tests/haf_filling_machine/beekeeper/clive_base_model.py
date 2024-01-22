from __future__ import annotations

import json
from datetime import datetime
from typing import Final, Any

from pydantic import BaseModel


class CustomJSONEncoder(json.JSONEncoder):
    TIME_FORMAT_WITH_MILLIS: Final[str] = "%Y-%m-%dT%H:%M:%S.%f"

    def default(self, obj: Any) -> Any:
        if isinstance(obj, datetime):
            return obj.strftime(self.TIME_FORMAT_WITH_MILLIS)

        return super().default(obj)


class CliveBaseModel(BaseModel):
    class Config:
        allow_population_by_field_name = True
        json_encoders = {datetime: lambda d: d.strftime(CustomJSONEncoder.TIME_FORMAT_WITH_MILLIS)}

