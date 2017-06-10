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
 * @file   gmusicprc.c
 * @author Juan A. Rubio <juan.rubio@aratelia.com>
 *
 * @brief  Google Music streaming client - processor class
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <OMX_TizoniaExt.h>

#include <tizplatform.h>

#include <tizkernel.h>
#include <tizscheduler.h>

#include "httpsrc.h"
#include "gmusicprc.h"
#include "gmusicprc_decls.h"

#ifdef TIZ_LOG_CATEGORY_NAME
#undef TIZ_LOG_CATEGORY_NAME
#define TIZ_LOG_CATEGORY_NAME "tiz.http_source.prc.gmusic"
#endif

/* forward declarations */
static OMX_ERRORTYPE
gmusic_prc_deallocate_resources (void *);
static OMX_ERRORTYPE
release_buffer (gmusic_prc_t *);
static OMX_ERRORTYPE
prepare_for_port_auto_detection (gmusic_prc_t * ap_prc);
static OMX_ERRORTYPE
gmusic_prc_prepare_to_transfer (void * ap_prc, OMX_U32 a_pid);
static OMX_ERRORTYPE
gmusic_prc_transfer_and_process (void * ap_prc, OMX_U32 a_pid);

#define on_gmusic_error_ret_omx_oom(expr)                                    \
  do                                                                         \
    {                                                                        \
      int gmusic_error = 0;                                                  \
      if (0 != (gmusic_error = (expr)))                                      \
        {                                                                    \
          TIZ_ERROR (handleOf (p_prc),                                       \
                     "[OMX_ErrorInsufficientResources] : error while using " \
                     "libtizgmusic");                                        \
          return OMX_ErrorInsufficientResources;                             \
        }                                                                    \
    }                                                                        \
  while (0)

static inline bool
is_valid_character (const char c)
{
  return (unsigned char) c > 0x20;
}

static void
obtain_coding_type (gmusic_prc_t * ap_prc, char * ap_info)
{
  assert (ap_prc);
  assert (ap_info);

  if (memcmp (ap_info, "audio/mpeg", 10) == 0
      || memcmp (ap_info, "audio/mpg", 9) == 0
      || memcmp (ap_info, "audio/mp3", 9) == 0)
    {
      ap_prc->audio_coding_type_ = OMX_AUDIO_CodingMP3;
    }
  else
    {
      ap_prc->audio_coding_type_ = OMX_AUDIO_CodingUnused;
    }
}

static int
convert_str_to_int (gmusic_prc_t * ap_prc, const char * ap_start,
                    char ** ap_end)
{
  long val = -1;
  assert (ap_prc);
  assert (ap_start);
  assert (ap_end);

  errno = 0;
  val = strtol (ap_start, ap_end, 0);

  if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
      || (errno != 0 && val == 0))
    {
      TIZ_ERROR (handleOf (ap_prc),
                 "Error retrieving the number of channels : [%s]",
                 strerror (errno));
    }
  else if (*ap_end == ap_start)
    {
      TIZ_ERROR (handleOf (ap_prc),
                 "Error retrieving the number of channels : "
                 "[No digits were found]");
    }
  return val;
}

static void
obtain_content_length (gmusic_prc_t * ap_prc, char * ap_info)
{
  char * p_end = NULL;

  assert (ap_prc);
  assert (ap_info);
  ap_prc->content_length_bytes_ = convert_str_to_int (ap_prc, ap_info, &p_end);
  ap_prc->bytes_before_eos_ = ap_prc->content_length_bytes_;
}

static OMX_ERRORTYPE
set_audio_coding_on_port (gmusic_prc_t * ap_prc)
{
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  assert (ap_prc);

  TIZ_INIT_OMX_PORT_STRUCT (port_def, ARATELIA_HTTP_SOURCE_PORT_INDEX);
  tiz_check_omx (
    tiz_api_GetParameter (tiz_get_krn (handleOf (ap_prc)), handleOf (ap_prc),
                          OMX_IndexParamPortDefinition, &port_def));

  /* Set the new value */
  port_def.format.audio.eEncoding = ap_prc->audio_coding_type_;

  tiz_check_omx (tiz_krn_SetParameter_internal (
    tiz_get_krn (handleOf (ap_prc)), handleOf (ap_prc),
    OMX_IndexParamPortDefinition, &port_def));
  return OMX_ErrorNone;
}

static void
update_cache_size (gmusic_prc_t * ap_prc)
{
  assert (ap_prc);
  assert (ap_prc->bitrate_ > 0);
  ap_prc->cache_bytes_ = ((ap_prc->bitrate_ * 1000) / 8)
                         * ARATELIA_HTTP_SOURCE_DEFAULT_CACHE_SECONDS;
  if (ap_prc->p_trans_)
    {
      tiz_urltrans_set_internal_buffer_size (ap_prc->p_trans_,
                                             ap_prc->cache_bytes_);
    }
}

static OMX_ERRORTYPE
store_metadata (gmusic_prc_t * ap_prc, const char * ap_header_name,
                const char * ap_header_info)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  OMX_CONFIG_METADATAITEMTYPE * p_meta = NULL;
  size_t metadata_len = 0;
  size_t info_len = 0;

  assert (ap_prc);
  if (ap_header_name && ap_header_info)
    {
      info_len = strnlen (ap_header_info, OMX_MAX_STRINGNAME_SIZE - 1) + 1;
      metadata_len = sizeof (OMX_CONFIG_METADATAITEMTYPE) + info_len;

      if (NULL == (p_meta = (OMX_CONFIG_METADATAITEMTYPE *) tiz_mem_calloc (
                     1, metadata_len)))
        {
          rc = OMX_ErrorInsufficientResources;
        }
      else
        {
          const size_t name_len
            = strnlen (ap_header_name, OMX_MAX_STRINGNAME_SIZE - 1) + 1;
          strncpy ((char *) p_meta->nKey, ap_header_name, name_len - 1);
          p_meta->nKey[name_len - 1] = '\0';
          p_meta->nKeySizeUsed = name_len;

          strncpy ((char *) p_meta->nValue, ap_header_info, info_len - 1);
          p_meta->nValue[info_len - 1] = '\0';
          p_meta->nValueMaxSize = info_len;
          p_meta->nValueSizeUsed = info_len;

          p_meta->nSize = metadata_len;
          p_meta->nVersion.nVersion = OMX_VERSION;
          p_meta->eScopeMode = OMX_MetadataScopeAllLevels;
          p_meta->nScopeSpecifier = 0;
          p_meta->nMetadataItemIndex = 0;
          p_meta->eSearchMode = OMX_MetadataSearchValueSizeByIndex;
          p_meta->eKeyCharset = OMX_MetadataCharsetASCII;
          p_meta->eValueCharset = OMX_MetadataCharsetASCII;

          rc = tiz_krn_store_metadata (tiz_get_krn (handleOf (ap_prc)), p_meta);
        }
    }
  return rc;
}

static void
obtain_audio_encoding_from_headers (gmusic_prc_t * ap_prc,
                                    const char * ap_header, const size_t a_size)
{
  assert (ap_prc);
  assert (ap_header);
  {
    const char * p_end = ap_header + a_size;
    const char * p_value = (const char *) memchr (ap_header, ':', a_size);
    char name[64];

    if (p_value && (size_t) (p_value - ap_header) < sizeof (name))
      {
        memcpy (name, ap_header, p_value - ap_header);
        name[p_value - ap_header] = 0;

        /* skip the colon */
        ++p_value;

        /* strip the value */
        while (p_value < p_end && !is_valid_character (*p_value))
          {
            ++p_value;
          }

        while (p_end > p_value && !is_valid_character (p_end[-1]))
          {
            --p_end;
          }

        {
          char * p_info = tiz_mem_calloc (1, (p_end - p_value) + 1);
          memcpy (p_info, p_value, p_end - p_value);
          p_info[(p_end - p_value)] = '\000';

          if (memcmp (name, "Content-Type", 12) == 0
              || memcmp (name, "content-type", 12) == 0)
            {
              obtain_coding_type (ap_prc, p_info);
              /* Now set the new coding type value on the output port */
              (void) set_audio_coding_on_port (ap_prc);
            }
          else if (memcmp (name, "Content-Length", 14) == 0)
            {
              obtain_content_length (ap_prc, p_info);
            }
          tiz_mem_free (p_info);
        }
      }
  }
}

static void
send_port_auto_detect_events (gmusic_prc_t * ap_prc)
{
  assert (ap_prc);
  if (ap_prc->audio_coding_type_ != OMX_AUDIO_CodingUnused
      || ap_prc->audio_coding_type_ != OMX_AUDIO_CodingAutoDetect)
    {
      tiz_srv_issue_event ((OMX_PTR) ap_prc, OMX_EventPortFormatDetected, 0, 0,
                           NULL);
      tiz_srv_issue_event ((OMX_PTR) ap_prc, OMX_EventPortSettingsChanged,
                           ARATELIA_HTTP_SOURCE_PORT_INDEX, /* port 0 */
                           OMX_IndexParamPortDefinition,    /* the index of the
                                                         struct that has
                                                         been modififed */
                           NULL);
    }
  else
    {
      /* Oops... could not detect the stream format */
      tiz_srv_issue_err_event ((OMX_PTR) ap_prc, OMX_ErrorFormatNotDetected);
    }
}

static inline void
delete_uri (gmusic_prc_t * ap_prc)
{
  assert (ap_prc);
  tiz_mem_free (ap_prc->p_uri_param_);
  ap_prc->p_uri_param_ = NULL;
}

static OMX_ERRORTYPE
update_metadata (gmusic_prc_t * ap_prc)
{
  assert (ap_prc);

  /* Clear previous metatada items */
  tiz_krn_clear_metadata (tiz_get_krn (handleOf (ap_prc)));

  /* Artist and song title */
  tiz_check_omx (store_metadata (
    ap_prc, tiz_gmusic_get_current_song_artist (ap_prc->p_gmusic_),
    tiz_gmusic_get_current_song_title (ap_prc->p_gmusic_)));

  /* Album */
  tiz_check_omx (store_metadata (
    ap_prc, "Album", tiz_gmusic_get_current_song_album (ap_prc->p_gmusic_)));

  /* Store the year if not 0 */
  {
    const char * p_year = tiz_gmusic_get_current_song_year (ap_prc->p_gmusic_);
    if (p_year && strncmp (p_year, "0", 4) != 0)
      {
        tiz_check_omx (store_metadata (ap_prc, "Year", p_year));
      }
  }

  /* Song duration */
  tiz_check_omx (
    store_metadata (ap_prc, "Duration",
                    tiz_gmusic_get_current_song_duration (ap_prc->p_gmusic_)));

  /* Track number */
  tiz_check_omx (store_metadata (
    ap_prc, "Track",
    tiz_gmusic_get_current_song_track_number (ap_prc->p_gmusic_)));

  /* Store total tracks if not 0 */
  {
    const char * p_total_tracks
      = tiz_gmusic_get_current_song_tracks_in_album (ap_prc->p_gmusic_);
    if (p_total_tracks && strncmp (p_total_tracks, "0", 2) != 0)
      {
        tiz_check_omx (
          store_metadata (ap_prc, "Total tracks", p_total_tracks));
      }
  }

  /* Signal that a new set of metatadata items is available */
  (void) tiz_srv_issue_event ((OMX_PTR) ap_prc, OMX_EventIndexSettingChanged,
                              OMX_ALL, /* no particular port associated */
                              OMX_IndexConfigMetadataItem, /* index of the
                                                             struct that has
                                                             been modififed */
                              NULL);
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
obtain_next_url (gmusic_prc_t * ap_prc, int a_skip_value)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  const long pathname_max = PATH_MAX + NAME_MAX;

  assert (ap_prc);
  assert (ap_prc->p_gmusic_);

  if (!ap_prc->p_uri_param_)
    {
      ap_prc->p_uri_param_ = tiz_mem_calloc (
        1, sizeof (OMX_PARAM_CONTENTURITYPE) + pathname_max + 1);
    }

  tiz_check_null_ret_oom (ap_prc->p_uri_param_ != NULL);

  ap_prc->p_uri_param_->nSize
    = sizeof (OMX_PARAM_CONTENTURITYPE) + pathname_max + 1;
  ap_prc->p_uri_param_->nVersion.nVersion = OMX_VERSION;

  {
    const char * p_next_url = a_skip_value > 0
                                ? tiz_gmusic_get_next_url (ap_prc->p_gmusic_)
                                : tiz_gmusic_get_prev_url (ap_prc->p_gmusic_);
    tiz_check_null_ret_oom (p_next_url != NULL);

    {
      const OMX_U32 url_len = strnlen (p_next_url, pathname_max);
      TIZ_TRACE (handleOf (ap_prc), "URL [%s]", p_next_url);

      /* Verify we are getting an http scheme */
      if (!p_next_url || !url_len
          || (memcmp (p_next_url, "http://", 7) != 0
              && memcmp (p_next_url, "https://", 8) != 0))
        {
          rc = OMX_ErrorContentURIError;
        }
      else
        {
          strncpy ((char *) ap_prc->p_uri_param_->contentURI, p_next_url,
                   url_len);
          ap_prc->p_uri_param_->contentURI[url_len] = '\000';

          /* Song metadata is now available, update the IL client */
          rc = update_metadata (ap_prc);
        }
    }
  }

  return rc;
}

static OMX_ERRORTYPE
release_buffer (gmusic_prc_t * ap_prc)
{
  assert (ap_prc);

  if (ap_prc->p_outhdr_)
    {
      if (ap_prc->bytes_before_eos_ > ap_prc->p_outhdr_->nFilledLen)
        {
          ap_prc->bytes_before_eos_ -= ap_prc->p_outhdr_->nFilledLen;
        }
      else
        {
          ap_prc->bytes_before_eos_ = 0;
          ap_prc->eos_ = true;
        }

      if (ap_prc->eos_)
        {
          ap_prc->eos_ = false;
          ap_prc->p_outhdr_->nFlags |= OMX_BUFFERFLAG_EOS;
        }
      tiz_check_omx (tiz_krn_release_buffer (
        tiz_get_krn (handleOf (ap_prc)), ARATELIA_HTTP_SOURCE_PORT_INDEX,
        ap_prc->p_outhdr_));
      ap_prc->p_outhdr_ = NULL;
    }
  return OMX_ErrorNone;
}

static void
buffer_filled (OMX_BUFFERHEADERTYPE * ap_hdr, void * ap_arg)
{
  gmusic_prc_t * p_prc = ap_arg;
  assert (p_prc);
  assert (ap_hdr);
  assert (p_prc->p_outhdr_ == ap_hdr);
  ap_hdr->nOffset = 0;
  (void) release_buffer (p_prc);
}

static OMX_BUFFERHEADERTYPE *
buffer_emptied (OMX_PTR ap_arg)
{
  gmusic_prc_t * p_prc = ap_arg;
  OMX_BUFFERHEADERTYPE * p_hdr = NULL;
  assert (p_prc);

  if (!p_prc->port_disabled_)
    {
      if (p_prc->p_outhdr_)
        {
          p_hdr = p_prc->p_outhdr_;
        }
      else
        {
          if (OMX_ErrorNone
              == (tiz_krn_claim_buffer (tiz_get_krn (handleOf (p_prc)),
                                        ARATELIA_HTTP_SOURCE_PORT_INDEX, 0,
                                        &p_prc->p_outhdr_)))
            {
              if (p_prc->p_outhdr_)
                {
                  TIZ_TRACE (handleOf (p_prc),
                             "Claimed HEADER [%p]...nFilledLen [%d]",
                             p_prc->p_outhdr_, p_prc->p_outhdr_->nFilledLen);
                  p_hdr = p_prc->p_outhdr_;
                }
            }
        }
    }
  return p_hdr;
}

static void
header_available (OMX_PTR ap_arg, const void * ap_ptr, const size_t a_nbytes)
{
  gmusic_prc_t * p_prc = ap_arg;
  assert (p_prc);
  assert (ap_ptr);
  obtain_audio_encoding_from_headers (p_prc, ap_ptr, a_nbytes);
}

static bool
data_available (OMX_PTR ap_arg, const void * ap_ptr, const size_t a_nbytes)
{
  gmusic_prc_t * p_prc = ap_arg;
  bool pause_needed = false;
  assert (p_prc);
  assert (ap_ptr);

  if (p_prc->auto_detect_on_ && a_nbytes > 0)
    {
      p_prc->auto_detect_on_ = false;

      /* This will pause the http transfer */
      pause_needed = true;

      /* And now trigger the OMX_EventPortFormatDetected and
         OMX_EventPortSettingsChanged events or a
         OMX_ErrorFormatNotDetected event */
      send_port_auto_detect_events (p_prc);
    }
  return pause_needed;
}

static bool
connection_lost (OMX_PTR ap_arg)
{
  gmusic_prc_t * p_prc = ap_arg;
  assert (p_prc);
  TIZ_PRINTF_DBG_RED ("connection_lost - bytes_before_eos_ [%d]\n",
                      p_prc->bytes_before_eos_);
  /* Return false to indicate that there is no need to start the automatic
     reconnection procedure */
  return false;
}

static OMX_ERRORTYPE
prepare_for_port_auto_detection (gmusic_prc_t * ap_prc)
{
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  assert (ap_prc);

  TIZ_INIT_OMX_PORT_STRUCT (port_def, ARATELIA_HTTP_SOURCE_PORT_INDEX);
  tiz_check_omx (
    tiz_api_GetParameter (tiz_get_krn (handleOf (ap_prc)), handleOf (ap_prc),
                          OMX_IndexParamPortDefinition, &port_def));
  ap_prc->audio_coding_type_ = port_def.format.audio.eEncoding;
  ap_prc->auto_detect_on_
    = (OMX_AUDIO_CodingAutoDetect == ap_prc->audio_coding_type_) ? true : false;
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
retrieve_session_configuration (gmusic_prc_t * ap_prc)
{
  return tiz_api_GetParameter (
    tiz_get_krn (handleOf (ap_prc)), handleOf (ap_prc),
    OMX_TizoniaIndexParamAudioGmusicSession, &(ap_prc->session_));
}

static OMX_ERRORTYPE
retrieve_playlist (gmusic_prc_t * ap_prc)
{
  return tiz_api_GetParameter (
    tiz_get_krn (handleOf (ap_prc)), handleOf (ap_prc),
    OMX_TizoniaIndexParamAudioGmusicPlaylist, &(ap_prc->playlist_));
}

static OMX_ERRORTYPE
enqueue_playlist_items (gmusic_prc_t * ap_prc)
{
  int rc = 1;

  assert (ap_prc);
  assert (ap_prc->p_gmusic_);

  {
    const char * p_playlist = (const char *) ap_prc->playlist_.cPlaylistName;
    const OMX_BOOL is_unlimited_search = ap_prc->playlist_.bUnlimitedSearch;
    const OMX_BOOL shuffle = ap_prc->playlist_.bShuffle;

    tiz_gmusic_set_playback_mode (
      ap_prc->p_gmusic_, (shuffle == OMX_TRUE ? ETIZGmusicPlaybackModeShuffle
                                              : ETIZGmusicPlaybackModeNormal));

    switch (ap_prc->playlist_.ePlaylistType)
      {
        case OMX_AUDIO_GmusicPlaylistTypeUnknown:
          {
            /* TODO */
            assert (0);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypeUser:
          {
            rc = tiz_gmusic_play_playlist (ap_prc->p_gmusic_, p_playlist,
                                           is_unlimited_search);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypeArtist:
          {
            rc = tiz_gmusic_play_artist (ap_prc->p_gmusic_, p_playlist,
                                         is_unlimited_search);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypeAlbum:
          {
            rc = tiz_gmusic_play_album (ap_prc->p_gmusic_, p_playlist,
                                        is_unlimited_search);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypeStation:
          {
            rc = tiz_gmusic_play_station (ap_prc->p_gmusic_, p_playlist);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypeGenre:
          {
            rc = tiz_gmusic_play_genre (ap_prc->p_gmusic_, p_playlist);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypeSituation:
          {
            rc = tiz_gmusic_play_situation (ap_prc->p_gmusic_, p_playlist);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypePromotedTracks:
          {
            rc = tiz_gmusic_play_promoted_tracks (ap_prc->p_gmusic_);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypeTracks:
          {
            rc = tiz_gmusic_play_tracks (ap_prc->p_gmusic_, p_playlist,
                                         is_unlimited_search);
          }
          break;
        case OMX_AUDIO_GmusicPlaylistTypePodcast:
          {
            rc = tiz_gmusic_play_podcast (ap_prc->p_gmusic_, p_playlist);
          }
          break;
        default:
          {
            assert (0);
          }
          break;
      };
  }
  return (rc == 0 ? OMX_ErrorNone : OMX_ErrorInsufficientResources);
}

/*
 * gmusicprc
 */

static void *
gmusic_prc_ctor (void * ap_obj, va_list * app)
{
  gmusic_prc_t * p_prc = super_ctor (typeOf (ap_obj, "gmusicprc"), ap_obj, app);
  p_prc->p_outhdr_ = NULL;
  p_prc->p_uri_param_ = NULL;
  p_prc->eos_ = false;
  p_prc->port_disabled_ = false;
  p_prc->uri_changed_ = false;
  p_prc->audio_coding_type_ = OMX_AUDIO_CodingUnused;
  p_prc->num_channels_ = 2;
  p_prc->samplerate_ = 44100;
  p_prc->content_length_bytes_ = 0;
  p_prc->bytes_before_eos_ = 0;
  p_prc->auto_detect_on_ = false;
  p_prc->bitrate_ = ARATELIA_HTTP_SOURCE_DEFAULT_BIT_RATE_KBITS;
  update_cache_size (p_prc);
  return p_prc;
}

static void *
gmusic_prc_dtor (void * ap_obj)
{
  (void) gmusic_prc_deallocate_resources (ap_obj);
  return super_dtor (typeOf (ap_obj, "gmusicprc"), ap_obj);
}

/*
 * from tizsrv class
 */

static OMX_ERRORTYPE
gmusic_prc_allocate_resources (void * ap_obj, OMX_U32 a_pid)
{
  gmusic_prc_t * p_prc = ap_obj;
  OMX_ERRORTYPE rc = OMX_ErrorInsufficientResources;
  assert (p_prc);
  tiz_check_omx (retrieve_session_configuration (p_prc));
  tiz_check_omx (retrieve_playlist (p_prc));

  TIZ_TRACE (handleOf (p_prc), "cUserName  : [%s]", p_prc->session_.cUserName);
  TIZ_TRACE (handleOf (p_prc), "cUserPassword  : [%s]",
             p_prc->session_.cUserPassword);
  TIZ_TRACE (handleOf (p_prc), "cDeviceId  : [%s]", p_prc->session_.cDeviceId);

  on_gmusic_error_ret_omx_oom (tiz_gmusic_init (
    &(p_prc->p_gmusic_), (const char *) p_prc->session_.cUserName,
    (const char *) p_prc->session_.cUserPassword,
    (const char *) p_prc->session_.cDeviceId));

  tiz_check_omx (enqueue_playlist_items (p_prc));
  tiz_check_omx (obtain_next_url (p_prc, 1));

  {
    const tiz_urltrans_buffer_cbacks_t buffer_cbacks
      = {buffer_filled, buffer_emptied};
    const tiz_urltrans_info_cbacks_t info_cbacks
      = {header_available, data_available, connection_lost};
    const tiz_urltrans_event_io_cbacks_t io_cbacks
      = {tiz_srv_io_watcher_init, tiz_srv_io_watcher_destroy,
         tiz_srv_io_watcher_start, tiz_srv_io_watcher_stop};
    const tiz_urltrans_event_timer_cbacks_t timer_cbacks
      = {tiz_srv_timer_watcher_init, tiz_srv_timer_watcher_destroy,
         tiz_srv_timer_watcher_start, tiz_srv_timer_watcher_stop,
         tiz_srv_timer_watcher_restart};
    rc
      = tiz_urltrans_init (&(p_prc->p_trans_), p_prc, p_prc->p_uri_param_,
                           ARATELIA_HTTP_SOURCE_COMPONENT_NAME,
                           ARATELIA_HTTP_SOURCE_PORT_MIN_BUF_SIZE,
                           ARATELIA_HTTP_SOURCE_DEFAULT_RECONNECT_TIMEOUT,
                           buffer_cbacks, info_cbacks, io_cbacks, timer_cbacks);
  }
  return rc;
}

static OMX_ERRORTYPE
gmusic_prc_deallocate_resources (void * ap_prc)
{
  gmusic_prc_t * p_prc = ap_prc;
  assert (p_prc);
  tiz_urltrans_destroy (p_prc->p_trans_);
  p_prc->p_trans_ = NULL;
  delete_uri (p_prc);
  tiz_gmusic_destroy (p_prc->p_gmusic_);
  p_prc->p_gmusic_ = NULL;
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
gmusic_prc_prepare_to_transfer (void * ap_prc, OMX_U32 a_pid)
{
  gmusic_prc_t * p_prc = ap_prc;
  assert (ap_prc);
  p_prc->eos_ = false;
  tiz_urltrans_cancel (p_prc->p_trans_);
  tiz_urltrans_set_internal_buffer_size (p_prc->p_trans_, p_prc->cache_bytes_);
  return prepare_for_port_auto_detection (p_prc);
}

static OMX_ERRORTYPE
gmusic_prc_transfer_and_process (void * ap_prc, OMX_U32 a_pid)
{
  gmusic_prc_t * p_prc = ap_prc;
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  assert (p_prc);
  if (p_prc->auto_detect_on_)
    {
      rc = tiz_urltrans_start (p_prc->p_trans_);
    }
  return rc;
}

static OMX_ERRORTYPE
gmusic_prc_stop_and_return (void * ap_prc)
{
  gmusic_prc_t * p_prc = ap_prc;
  assert (p_prc);
  if (p_prc->p_trans_)
    {
      tiz_urltrans_pause (p_prc->p_trans_);
      tiz_urltrans_flush_buffer (p_prc->p_trans_);
    }
  return release_buffer (p_prc);
}

/*
 * from tizprc class
 */

static OMX_ERRORTYPE
gmusic_prc_buffers_ready (const void * ap_prc)
{
  gmusic_prc_t * p_prc = (gmusic_prc_t *) ap_prc;
  assert (p_prc);
  return tiz_urltrans_on_buffers_ready (p_prc->p_trans_);
}

static OMX_ERRORTYPE
gmusic_prc_io_ready (void * ap_prc, tiz_event_io_t * ap_ev_io, int a_fd,
                     int a_events)
{
  gmusic_prc_t * p_prc = ap_prc;
  assert (p_prc);
  return tiz_urltrans_on_io_ready (p_prc->p_trans_, ap_ev_io, a_fd, a_events);
}

static OMX_ERRORTYPE
gmusic_prc_timer_ready (void * ap_prc, tiz_event_timer_t * ap_ev_timer,
                        void * ap_arg, const uint32_t a_id)
{
  gmusic_prc_t * p_prc = ap_prc;
  assert (p_prc);
  return tiz_urltrans_on_timer_ready (p_prc->p_trans_, ap_ev_timer);
}

static OMX_ERRORTYPE
gmusic_prc_pause (const void * ap_obj)
{
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
gmusic_prc_resume (const void * ap_obj)
{
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
gmusic_prc_port_flush (const void * ap_obj, OMX_U32 TIZ_UNUSED (a_pid))
{
  gmusic_prc_t * p_prc = (gmusic_prc_t *) ap_obj;
  if (p_prc->p_trans_)
    {
      tiz_urltrans_flush_buffer (p_prc->p_trans_);
    }
  return release_buffer (p_prc);
}

static OMX_ERRORTYPE
gmusic_prc_port_disable (const void * ap_obj, OMX_U32 TIZ_UNUSED (a_pid))
{
  gmusic_prc_t * p_prc = (gmusic_prc_t *) ap_obj;
  assert (p_prc);
  TIZ_PRINTF_DBG_RED ("Disabling port was disabled? [%s]\n",
                      p_prc->port_disabled_ ? "YES" : "NO");
  p_prc->port_disabled_ = true;
  if (p_prc->p_trans_)
    {
      tiz_urltrans_pause (p_prc->p_trans_);
      tiz_urltrans_flush_buffer (p_prc->p_trans_);
    }
  /* Release any buffers held  */
  return release_buffer ((gmusic_prc_t *) ap_obj);
}

static OMX_ERRORTYPE
gmusic_prc_port_enable (const void * ap_prc, OMX_U32 a_pid)
{
  gmusic_prc_t * p_prc = (gmusic_prc_t *) ap_prc;
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  assert (p_prc);
  TIZ_PRINTF_DBG_RED ("Enabling port was disabled? [%s]\n",
                      p_prc->port_disabled_ ? "YES" : "NO");
  if (p_prc->port_disabled_)
    {
      p_prc->port_disabled_ = false;
      if (!p_prc->uri_changed_)
        {
          rc = tiz_urltrans_unpause (p_prc->p_trans_);
        }
      else
        {
          p_prc->uri_changed_ = false;
          rc = tiz_urltrans_start (p_prc->p_trans_);
        }
    }
  return rc;
}

static OMX_ERRORTYPE
gmusic_prc_config_change (void * ap_prc, OMX_U32 TIZ_UNUSED (a_pid),
                          OMX_INDEXTYPE a_config_idx)
{
  gmusic_prc_t * p_prc = ap_prc;
  OMX_ERRORTYPE rc = OMX_ErrorNone;

  assert (p_prc);

  if (OMX_TizoniaIndexConfigPlaylistSkip == a_config_idx && p_prc->p_trans_)
    {
      TIZ_INIT_OMX_STRUCT (p_prc->playlist_skip_);
      tiz_check_omx (tiz_api_GetConfig (
        tiz_get_krn (handleOf (p_prc)), handleOf (p_prc),
        OMX_TizoniaIndexConfigPlaylistSkip, &p_prc->playlist_skip_));
      p_prc->playlist_skip_.nValue > 0 ? obtain_next_url (p_prc, 1)
                                       : obtain_next_url (p_prc, -1);
      /* Changing the URL has the side effect of halting the current
         download */
      tiz_urltrans_set_uri (p_prc->p_trans_, p_prc->p_uri_param_);
      if (p_prc->port_disabled_)
        {
          /* Record that the URI has changed, so that when the port is
             re-enabled, we restart the transfer */
          p_prc->uri_changed_ = true;
        }
      else
        {
          /* re-start the transfer */
          tiz_urltrans_start (p_prc->p_trans_);
        }
    }
  return rc;
}

/*
 * gmusic_prc_class
 */

static void *
gmusic_prc_class_ctor (void * ap_obj, va_list * app)
{
  /* NOTE: Class methods might be added in the future. None for now. */
  return super_ctor (typeOf (ap_obj, "gmusicprc_class"), ap_obj, app);
}

/*
 * initialization
 */

void *
gmusic_prc_class_init (void * ap_tos, void * ap_hdl)
{
  void * tizprc = tiz_get_type (ap_hdl, "tizprc");
  void * gmusicprc_class = factory_new
    /* TIZ_CLASS_COMMENT: class type, class name, parent, size */
    (classOf (tizprc), "gmusicprc_class", classOf (tizprc),
     sizeof (gmusic_prc_class_t),
     /* TIZ_CLASS_COMMENT: */
     ap_tos, ap_hdl,
     /* TIZ_CLASS_COMMENT: class constructor */
     ctor, gmusic_prc_class_ctor,
     /* TIZ_CLASS_COMMENT: stop value*/
     0);
  return gmusicprc_class;
}

void *
gmusic_prc_init (void * ap_tos, void * ap_hdl)
{
  void * tizprc = tiz_get_type (ap_hdl, "tizprc");
  void * gmusicprc_class = tiz_get_type (ap_hdl, "gmusicprc_class");
  TIZ_LOG_CLASS (gmusicprc_class);
  void * gmusicprc = factory_new
    /* TIZ_CLASS_COMMENT: class type, class name, parent, size */
    (gmusicprc_class, "gmusicprc", tizprc, sizeof (gmusic_prc_t),
     /* TIZ_CLASS_COMMENT: */
     ap_tos, ap_hdl,
     /* TIZ_CLASS_COMMENT: class constructor */
     ctor, gmusic_prc_ctor,
     /* TIZ_CLASS_COMMENT: class destructor */
     dtor, gmusic_prc_dtor,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_allocate_resources, gmusic_prc_allocate_resources,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_deallocate_resources, gmusic_prc_deallocate_resources,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_prepare_to_transfer, gmusic_prc_prepare_to_transfer,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_transfer_and_process, gmusic_prc_transfer_and_process,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_stop_and_return, gmusic_prc_stop_and_return,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_io_ready, gmusic_prc_io_ready,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_timer_ready, gmusic_prc_timer_ready,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_buffers_ready, gmusic_prc_buffers_ready,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_pause, gmusic_prc_pause,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_resume, gmusic_prc_resume,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_port_flush, gmusic_prc_port_flush,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_port_disable, gmusic_prc_port_disable,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_port_enable, gmusic_prc_port_enable,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_config_change, gmusic_prc_config_change,
     /* TIZ_CLASS_COMMENT: stop value */
     0);

  return gmusicprc;
}
