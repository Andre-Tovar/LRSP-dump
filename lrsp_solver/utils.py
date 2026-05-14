from __future__ import annotations

from math import hypot


def euclidean_distance(x1: float, y1: float, x2: float, y2: float) -> float:
    return hypot(x1 - x2, y1 - y2)
