﻿/**
 * Copyright (c) 2017 Darius Rückert
 * Licensed under the MIT License.
 * See LICENSE file for more information.
 */


#pragma once

#include "Buffer.h"
#include "vulkanBase.h"

namespace Saiga {
namespace Vulkan {


class SAIGA_GLOBAL IndexBuffer : public Buffer
{
public:


    void init(VulkanBase& base);
};

}
}
