from os import PathLike
from typing import BinaryIO, Literal, overload

class _Axis:
    axisTag: str
    minValue: float
    defaultValue: float
    maxValue: float

class _VariationTable:
    axes: list[_Axis]

class _MetricsTable:
    metrics: dict[str, tuple[int, int]]

class TTFont:
    def __init__(
        self, file: str | PathLike[str] | BinaryIO, *, recalcTimestamp: bool = True
    ) -> None: ...
    def __contains__(self, tag: str) -> bool: ...
    @overload
    def __getitem__(self, tag: Literal["fvar"]) -> _VariationTable: ...
    @overload
    def __getitem__(self, tag: Literal["hmtx"]) -> _MetricsTable: ...
    def getBestCmap(self) -> dict[int, str] | None: ...
    def save(self, file: str | PathLike[str] | BinaryIO) -> None: ...
    def close(self) -> None: ...
