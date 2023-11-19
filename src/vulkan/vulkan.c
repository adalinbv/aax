/*
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Erik Hofman.
 * SPDX-FileCopyrightText: Copyright © 2019-2023 by Adalin B.V.
 *
 * Package Name: AeonWave Audio eXtentions library.
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 */

#include <vulkan/vulkan.h>

typedef struct
{
   VkInstance instance;

   uint32_t num = 0;
   VkPhysicalDevice* const devices;

} _driver_t;

static int _aaxDetectPhysicalDevice(VkInstance*);

int
_aaxDetectVulkan()
{
   static void *renderer = NULL;
   static int rv = false;

   char *error = 0;

   _AAX_LOG(LOG_DEBUG, __func__);

   if (TEST_FOR_FALSE(rv) && !renderer) {
      renderer = _aaxIsLibraryPresent("vulkan", "1");
      _aaxGetSymError(0);
   }

   if (renderer) 
   {
      const VkApplicationInfo appinfo = {
         VK_STRUCTURE_TYPE_APPLICATION_INFO, 0, "VkRingBuffer", 0, "", 0,
         VK_MAKE_VERSION(1, 0, 9)
      };
      const VkInstanceCreateInfo info = {
         VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, 0, 0, &appinfo, 0, 0, 0, 0
      };
      VkInstance instance;

      if (vkCreateInstance(&info, 0, &instance) == VK_SUCCESS)
      {
         if (vkEnumeratePhysicalDevices(id, &num, 0) == VK_SUCCESS) {
            rv = true;
         } else {
            rv = false;
         }
      } else {
         rv = false;
      }
   }

   return rv;
}

