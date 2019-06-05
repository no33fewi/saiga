/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */

#pragma once
#include "saiga/core/util/ObjectCache.h"
#include "saiga/core/util/singleton.h"
#include "saiga/opengl/texture/texture.h"

#include <memory>

namespace Saiga
{
struct SAIGA_OPENGL_API TextureParameters
{
    bool srgb = true;
};

SAIGA_OPENGL_API bool operator==(const TextureParameters& lhs, const TextureParameters& rhs);


class SAIGA_OPENGL_API TextureLoader : public Singleton<TextureLoader>
{
    friend class Singleton<TextureLoader>;

    ObjectCache<std::string, std::shared_ptr<Texture>, TextureParameters> cache;

   public:
    std::shared_ptr<Texture> load(const std::string& name, const TextureParameters& params = TextureParameters());
};

}  // namespace Saiga
