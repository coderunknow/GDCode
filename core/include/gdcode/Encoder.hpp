#pragma once

#include "Ir.hpp"

#include <string>

namespace gdcode {

/// Format a number the way GD's level-string parser expects:
/// integral values without a decimal point, others with up to 6 decimals.
std::string formatGdNumber(double value);

/// Encode a LevelIR into the *raw* (uncompressed) GD level string:
///
///   kA13,0,kA15,0,...,kS38,<color|color|...>,kS39,0;<object>;<object>;...
///
/// where each object is `key,value,key,value,...` (keys per the 2.2 level
/// format: 1=id, 2=x, 3=y, 4=flipX, 5=flipY, 6=rotation, 20=editor layer,
/// 21=main color channel, 24=z layer, 25=z order, 32=scale, 33=group id).
///
/// The Geode backend then wraps this with ZipUtils::compressString() to get
/// the exact byte format GD stores in GJGameLevel::m_levelString.
std::string encodeLevelString(LevelIR const& ir);

} // namespace gdcode
