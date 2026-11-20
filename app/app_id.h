/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Kevin Sint */
/* What this firmware is called: on the screen, on USB, and to the installer. */
#pragma once
#define APP_NAME "SINT_FM1_DR1"
#define APP_VERSION "V0.27"
/* the installer compares this with the package's (scripts/build.sh gives fm1pkg.py the same);
 * fm1-base's examples and felucca-bsp's hwtest use FM-1_11.. to FM-1_19.., Felucca the 900s */
#define APP_PACKAGE_ID "FM-1_2000"
