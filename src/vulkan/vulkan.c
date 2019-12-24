/*
 * Copyright 2019 by Erik Hofman.
 * Copyright 2019 by Adalin B.V.
 *
 * This file is part of AeonWave
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
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
   static int rv = AAX_FALSE;

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
            rv = AAX_TRUE;
         } else {
            rv = AAX_FALSE;
         }
      } else {
         rv = AAX_FALSE;
      }
   }

   return rv;
}

