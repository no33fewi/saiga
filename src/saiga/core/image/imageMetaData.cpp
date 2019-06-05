/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#include "imageMetaData.h"

#include "saiga/core/math/math.h"

#include "internal/noGraphicsAPI.h"

namespace Saiga
{
std::ostream& operator<<(std::ostream& os, const ImageMetadata& d)
{
    os << "> ImageMetadata" << endl;
    os << "Size: " << d.width << "x" << d.height << endl;
    os << "DateTime: " << d.DateTime << endl;
    os << "Make: " << d.Make << endl;
    os << "Model: " << d.Model << endl;

    os << "FocalLengthMM: " << d.FocalLengthMM << endl;
    os << "FocalLengthMM35: " << d.FocalLengthMM35 << endl;

    std::string resStr;
    switch (d.FocalPlaneResolutionUnit)
    {
        case ImageMetadata::NoUnit:
            resStr = "NoUnit";
            break;
        case ImageMetadata::Inch:
            resStr = "Inch";
            break;
        case ImageMetadata::Centimeter:
            resStr = "Centimeter";
            break;
    }

    os << "FocalPlaneResolutionUnit: " << resStr << endl;
    os << "FocalPlaneXResolution: " << d.FocalPlaneXResolution << endl;
    os << "FocalPlaneYResolution: " << d.FocalPlaneYResolution;
    return os;
}

}  // namespace Saiga
