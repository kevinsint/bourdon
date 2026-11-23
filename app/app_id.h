/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kevin Sint */
/* What this firmware is called: on the screen, on USB, and to the installer. */
#pragma once
#define APP_NAME "SINT_BOURDON"
#define APP_VERSION "V0.34"
/* the installer compares this with the package's (scripts/build.sh gives fm1pkg.py the same);
 * fm1-base's examples and felucca-bsp's hwtest use FM-1_11.. to FM-1_19.., Felucca the 900s.
 * BOURDON is 309 (free on thefirmwarehub.com, 2026-11-23), then the version: V0.34 -> 0034. M-UPGRADE sees only
 * FM-1_309 (bsp/chip/ota.c). Until V0.33 the firmware was DR1, FM-1_2000 */
#define APP_PACKAGE_ID "FM-1_3090034"
