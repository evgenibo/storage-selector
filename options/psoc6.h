/* mbed Microcontroller Library
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _PSOC6_H_
#define _PSOC6_H_

#ifdef TARGET_MCU_PSoC6_M4

#include "Psoc6BlockDevice.h"

Psoc6BlockDevice* _storage_selector_PSOC6();

#endif //TARGET_MCU_PSoC6_M4

#endif //_PSOC6_H_
