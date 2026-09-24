from collections.abc import Mapping

from fontTools.ttLib import TTFont

def instantiateVariableFont(varfont: TTFont, axisLimits: Mapping[str, float]) -> TTFont: ...
