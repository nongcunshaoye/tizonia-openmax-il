/**
 * Copyright (C) 2011-2017 Aratelia Limited - Juan A. Rubio
 *
 * This file is part of Tizonia
 *
 * Tizonia is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Tizonia is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Tizonia.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   tizdeezer_c.h
 * @author Juan A. Rubio <juan.rubio@aratelia.com>
 *
 * @brief  Tizonia - Simple Deezer client library (c wrapper)
 *
 *
 */

#ifndef TIZDEEZER_C_H
#define TIZDEEZER_C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
* @defgroup libtizdeezer 'libtizdeezer' : Tizonia's Deezer client
* library
*
* A C library to access the Deezer streaming service.
*
* @ingroup Tizonia
*/

/**
 * The deezer opaque structure
 * @ingroup libtizdeezer
 */
typedef struct tiz_deezer tiz_deezer_t;
typedef /*@null@ */ tiz_deezer_t *tiz_deezer_ptr_t;

/**
 * Various playback modes that control the playback queue.
 * @ingroup libtizdeezer
 */
typedef enum tiz_deezer_playback_mode
{
  ETIZDeezerPlaybackModeNormal,
  ETIZDeezerPlaybackModeShuffle,
  ETIZDeezerPlaybackModeMax
} tiz_deezer_playback_mode_t;

/**
 * Initialize the deezer handle.
 *
 * @ingroup libtizdeezer
 *
 * @param app_deezer A pointer to the deezer handle which will be
 * initialised.
 * @param ap_oauth_token A Deezer email account.
 *
 * @return 0 on success.
 */
int tiz_deezer_init (/*@null@ */ tiz_deezer_ptr_t *app_deezer,
                     const char *ap_oauth_token);

/**
 * Clear the playback queue.
 *
 * @ingroup libtizdeezer
 *
 * @param ap_deezer The deezer handle.
 */
void tiz_deezer_set_playback_mode (tiz_deezer_t *ap_deezer,
                                   const tiz_deezer_playback_mode_t mode);

/**
 * Destroy the deezer handle.
 *
 * @ingroup libtizdeezer
 *
 * @param ap_deezer The deezer handle.
 */
void tiz_deezer_destroy (tiz_deezer_t *ap_deezer);

#ifdef __cplusplus
}
#endif

#endif  // TIZDEEZER_C_H
