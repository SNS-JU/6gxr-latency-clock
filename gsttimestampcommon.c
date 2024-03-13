/* GStreamer
 * Copyright (C) 2024 Felician Nemeth <nemethf@tmit.bme.hu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <liquid.h>

#include "gsttimestampcommon.h"

GType
gst_fec_scheme_get_type (void)
{
  static GType fec_scheme_type = 0;

  if (!fec_scheme_type) {
    static GEnumValue fec_scheme_types[] = {
      { LIQUID_FEC_UNKNOWN,   "",                         "unknown" },
      { LIQUID_FEC_NONE,      "no error-correction",      "none" },
      { LIQUID_FEC_REP3,      "simple repeat code, r1/3", "rep3" },
      { LIQUID_FEC_REP5,      "simple repeat code, r1/5", "rep5" },
      { LIQUID_FEC_HAMMING74, "Hamming (7,4) block code", "hamming74" },
      { LIQUID_FEC_HAMMING84, "Hamming (7,4) with extra parity bit, r1/2", "hamming84" },
      { LIQUID_FEC_HAMMING128,"Hamming (12,8) block code, r2/3", "hamming128" },
      { LIQUID_FEC_GOLAY2412, "Golay (24,12) block code, r1/2", "golay2421" },
      { LIQUID_FEC_SECDED2216, "SEC-DED (22,16) block code, r8/11", "secded2216" },
      { LIQUID_FEC_SECDED3932, "SEC-DED (39,32) block code", "secded3932" },
      { LIQUID_FEC_SECDED7264, "SEC-DED (72,64) block code, r8/9", "secdec7264" },
      { LIQUID_FEC_CONV_V27,  "r1/2, K=7, dfree=10",      "conv_v27" },
      { LIQUID_FEC_CONV_V29,  "r1/2, K=9, dfree=12",      "conv_v29" },
      { LIQUID_FEC_CONV_V39,  "r1/3, K=9, dfree=18",      "conv_v39" },
      { LIQUID_FEC_CONV_V615, "r1/6, K=15, dfree<=57",    "conv_v615" },
      { LIQUID_FEC_CONV_V27P23, "r2/3, K=7, dfree=6", "conv_v27p23" },
      { LIQUID_FEC_RS_M8,     "Reed-Solomon, m=8, n=255, k=223", "rs_m8" },
      { 0, NULL, NULL },
    };

    fec_scheme_type = g_enum_register_static ("fec_scheme", fec_scheme_types);
  }

  return fec_scheme_type;
}
