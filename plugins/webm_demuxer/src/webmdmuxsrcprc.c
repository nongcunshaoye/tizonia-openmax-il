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
 * @file   webmdmuxsrcprc.c
 * @author Juan A. Rubio <juan.rubio@aratelia.com>
 *
 * @brief  Tizonia - WebM Demuxer source processor
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <string.h>

#include <OMX_TizoniaExt.h>

#include <tizplatform.h>

#include <tizkernel.h>
#include <tizscheduler.h>

#include "webmdmux.h"
#include "webmdmuxsrcprc.h"
#include "webmdmuxsrcprc_decls.h"

#ifdef TIZ_LOG_CATEGORY_NAME
#undef TIZ_LOG_CATEGORY_NAME
#define TIZ_LOG_CATEGORY_NAME "tiz.webm_demuxer.source.prc"
#endif

/* Forward declarations */
static OMX_ERRORTYPE
webmdmuxsrc_prc_deallocate_resources (void *);
static OMX_ERRORTYPE
release_buffer (webmdmuxsrc_prc_t *);
static OMX_ERRORTYPE
prepare_for_port_auto_detection (webmdmuxsrc_prc_t * ap_prc);

#define on_nestegg_error_ret_omx_oom(expr)                                   \
  do                                                                         \
    {                                                                        \
      int nestegg_error = 0;                                                 \
      if (0 != (nestegg_error = (expr)))                                     \
        {                                                                    \
          TIZ_ERROR (handleOf (p_prc),                                       \
                     "[OMX_ErrorInsufficientResources] : error while using " \
                     "libnestegg");                                          \
          return OMX_ErrorInsufficientResources;                             \
        }                                                                    \
    }                                                                        \
  while (0)

static int
ne_io_read (void * a_buffer, size_t a_length, void * a_userdata)
{
  return 0;
}

static int
ne_io_seek (int64_t offset, int whence, void * userdata)
{
  return 0;
}

static int64_t
ne_io_tell (void * userdata)
{
  return 0;
}

/* static inline OMX_BUFFERHEADERTYPE * */
/* get_aud_hdr (webmdmuxsrc_prc_t * ap_prc) */
/* { */
/*   assert (ap_prc); */
/*   return tiz_filter_prc_get_header (ap_prc, */
/*                                     ARATELIA_WEBM_DEMUXER_FILTER_PORT_1_INDEX); */
/* } */

/* static inline OMX_BUFFERHEADERTYPE * */
/* get_vid_hdr (webmdmuxsrc_prc_t * ap_prc) */
/* { */
/*   assert (ap_prc); */
/*   return tiz_filter_prc_get_header (ap_prc, */
/*                                     ARATELIA_WEBM_DEMUXER_FILTER_PORT_2_INDEX); */
/* } */

/* static OMX_ERRORTYPE init_demuxer (webmdmuxsrc_prc_t *ap_prc) */
/* { */
/*   OMX_ERRORTYPE rc = OMX_ErrorNone; */

/*   if (store_data (ap_prc)) */
/*     { */
/*       int op_error = 0; */
/*       OpusFileCallbacks op_cbacks = { read_cback, NULL, NULL, NULL }; */

/*       ap_prc->p_opus_dec_ */
/*           = op_open_callbacks (ap_prc, &op_cbacks, NULL, 0, &op_error); */

/*       if (0 != op_error) */
/*         { */
/*           TIZ_ERROR (handleOf (ap_prc), */
/*                      "Unable to open the opus file handle (op_error = %d).",
 */
/*                      op_error); */
/*           ap_prc->store_offset_ = 0; */
/*         } */
/*       else */
/*         { */
/*           TIZ_TRACE (handleOf (ap_prc), */
/*                      "demuxer_inited = TRUE - store_offset [%d]", */
/*                      ap_prc->store_offset_); */
/*           ap_prc->demuxer_inited_ = true; */
/*           tiz_buffer_advance (ap_prc->p_store_, ap_prc->store_offset_); */
/*           ap_prc->store_offset_ = 0; */
/*         } */
/*     } */
/*   else */
/*     { */
/*       rc = OMX_ErrorInsufficientResources; */
/*     } */

/*   return rc; */
/* } */

/* static OMX_ERRORTYPE transform_buffer (webmdmuxsrc_prc_t *ap_prc) */
/* { */
/*   OMX_ERRORTYPE rc = OMX_ErrorNone; */
/*   OMX_BUFFERHEADERTYPE *p_out = tiz_filter_prc_get_header ( */
/*       ap_prc, ARATELIA_OPUS_DECODER_OUTPUT_PORT_INDEX); */

/*   if (!store_data (ap_prc)) */
/*     { */
/*       TIZ_ERROR (handleOf (ap_prc), */
/*                  "[OMX_ErrorInsufficientResources] : " */
/*                  "Could not store all the incoming data"); */
/*       return OMX_ErrorInsufficientResources; */
/*     } */

/*   if (tiz_buffer_available (ap_prc->p_store_) == 0 || NULL == p_out) */
/*     { */
/*       TIZ_TRACE (handleOf (ap_prc), "store bytes [%d] OUT HEADER [%p]", */
/*                  tiz_buffer_available (ap_prc->p_store_), p_out); */

/*       /\* Propagate the EOS flag to the next component *\/ */
/*       if (tiz_buffer_available (ap_prc->p_store_) == 0 && p_out */
/*           && tiz_filter_prc_is_eos (ap_prc)) */
/*         { */
/*           p_out->nFlags |= OMX_BUFFERFLAG_EOS; */
/*           tiz_filter_prc_release_header ( */
/*               ap_prc, ARATELIA_OPUS_DECODER_OUTPUT_PORT_INDEX); */
/*           tiz_filter_prc_update_eos_flag (ap_prc, false); */
/*         } */
/*       return OMX_ErrorNotReady; */
/*     } */

/*   assert (ap_prc); */
/*   assert (ap_prc->p_opus_dec_); */

/*   { */
/*     unsigned char *p_pcm = p_out->pBuffer + p_out->nOffset; */
/*     const long len = p_out->nAllocLen; */
/*     int samples_read */
/*         = op_read_float_stereo (ap_prc->p_opus_dec_, (float *)p_pcm, len); */
/*     TIZ_TRACE (handleOf (ap_prc), "samples_read [%d] ", samples_read); */

/*     if (samples_read > 0) */
/*       { */
/*         p_out->nFilledLen = 2 * samples_read * sizeof(float); */
/*         (void)tiz_filter_prc_release_header ( */
/*             ap_prc, ARATELIA_OPUS_DECODER_OUTPUT_PORT_INDEX); */
/*       } */
/*     else */
/*       { */
/*         switch (samples_read) */
/*           { */
/*             case OP_HOLE: */
/*               { */
/*                 TIZ_NOTICE (handleOf (ap_prc), */
/*                             "[OP_HOLE] : " */
/*                             "While decoding the input stream."); */
/*               } */
/*               break; */
/*             default: */
/*               { */
/*                 TIZ_ERROR (handleOf (ap_prc), */
/*                            "[OMX_ErrorStreamCorruptFatal] : " */
/*                            "While decoding the input stream."); */
/*                 rc = OMX_ErrorStreamCorruptFatal; */
/*               } */
/*           }; */
/*       } */
/*   } */

/*   return rc; */
/* } */

static void
ne_log_cback (nestegg * ctx, unsigned int severity, char const * fmt, ...)
{
  va_list ap;
  char const * sev = NULL;

#if !defined(DEBUG)
  if (severity < NESTEGG_LOG_WARNING)
    return;
#endif

  switch (severity)
    {
      case NESTEGG_LOG_DEBUG:
        sev = "debug:   ";
        break;
      case NESTEGG_LOG_WARNING:
        sev = "warning: ";
        break;
      case NESTEGG_LOG_CRITICAL:
        sev = "critical:";
        break;
      default:
        sev = "unknown: ";
    }

  fprintf (stderr, "%p %s ", (void *) ctx, sev);

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);

  fprintf (stderr, "\n");
}

static void
update_cache_size (webmdmuxsrc_prc_t * ap_prc)
{
  assert (ap_prc);
  assert (ap_prc->bitrate_ > 0);
  ap_prc->cache_bytes_ = ((ap_prc->bitrate_ * 1000) / 8)
                         * ARATELIA_WEBM_DEMUXER_DEFAULT_CACHE_SECONDS;
  if (ap_prc->p_trans_)
    {
      tiz_urltrans_set_internal_buffer_size (ap_prc->p_trans_,
                                             ap_prc->cache_bytes_);
    }
}

static OMX_ERRORTYPE
obtain_uri (webmdmuxsrc_prc_t * ap_prc)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  const long pathname_max = PATH_MAX + NAME_MAX;

  assert (ap_prc);
  assert (!ap_prc->p_uri_);

  ap_prc->p_uri_
    = tiz_mem_calloc (1, sizeof (OMX_PARAM_CONTENTURITYPE) + pathname_max + 1);

  if (!ap_prc->p_uri_)
    {
      TIZ_ERROR (handleOf (ap_prc),
                 "Error allocating memory for the content uri struct");
      rc = OMX_ErrorInsufficientResources;
    }
  else
    {
      ap_prc->p_uri_->nSize
        = sizeof (OMX_PARAM_CONTENTURITYPE) + pathname_max + 1;
      ap_prc->p_uri_->nVersion.nVersion = OMX_VERSION;

      tiz_check_omx (tiz_api_GetParameter (
        tiz_get_krn (handleOf (ap_prc)), handleOf (ap_prc),
        OMX_IndexParamContentURI, ap_prc->p_uri_));
      TIZ_NOTICE (handleOf (ap_prc), "URI [%s]", ap_prc->p_uri_->contentURI);
      /* Verify we are getting an http scheme */
      if (strncasecmp ((const char *) ap_prc->p_uri_->contentURI, "http://", 7)
            != 0
          && strncasecmp ((const char *) ap_prc->p_uri_->contentURI, "https://",
                          8)
               != 0)
        {
          rc = OMX_ErrorContentURIError;
        }
    }

  return rc;
}

static void
obtain_audio_encoding_from_headers (webmdmuxsrc_prc_t * ap_prc,
                                    const char * ap_header, const size_t a_size)
{
  (void) ap_prc;
  (void) ap_header;
  (void) a_size;
}

static inline void
delete_uri (webmdmuxsrc_prc_t * ap_prc)
{
  assert (ap_prc);
  tiz_mem_free (ap_prc->p_uri_);
  ap_prc->p_uri_ = NULL;
}

static OMX_ERRORTYPE
release_buffer (webmdmuxsrc_prc_t * ap_prc)
{
  assert (ap_prc);

  if (ap_prc->p_outhdr_)
    {
      TIZ_NOTICE (handleOf (ap_prc), "releasing HEADER [%p] nFilledLen [%d]",
                  ap_prc->p_outhdr_, ap_prc->p_outhdr_->nFilledLen);
      tiz_check_omx (tiz_krn_release_buffer (
        tiz_get_krn (handleOf (ap_prc)),
        ARATELIA_WEBM_DEMUXER_SOURCE_PORT_0_INDEX, ap_prc->p_outhdr_));
      ap_prc->p_outhdr_ = NULL;
    }
  return OMX_ErrorNone;
}

static void
buffer_filled (OMX_BUFFERHEADERTYPE * ap_hdr, void * ap_arg)
{
  webmdmuxsrc_prc_t * p_prc = ap_arg;
  assert (p_prc);
  assert (ap_hdr);
  assert (p_prc->p_outhdr_ == ap_hdr);
  ap_hdr->nOffset = 0;
  (void) release_buffer (p_prc);
}

static OMX_BUFFERHEADERTYPE *
buffer_emptied (OMX_PTR ap_arg)
{
  webmdmuxsrc_prc_t * p_prc = ap_arg;
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
          if (OMX_ErrorNone == (tiz_krn_claim_buffer (
                                 tiz_get_krn (handleOf (p_prc)),
                                 ARATELIA_WEBM_DEMUXER_SOURCE_PORT_0_INDEX, 0,
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
  webmdmuxsrc_prc_t * p_prc = ap_arg;
  assert (p_prc);
  assert (ap_ptr);

  if (p_prc->auto_detect_on_)
    {
      obtain_audio_encoding_from_headers (p_prc, ap_ptr, a_nbytes);
    }
}

static bool
data_available (OMX_PTR ap_arg, const void * ap_ptr, const size_t a_nbytes)
{
  webmdmuxsrc_prc_t * p_prc = ap_arg;
  bool pause_needed = false;
  assert (p_prc);
  assert (ap_ptr);

  /*   if (p_prc->auto_detect_on_ && a_nbytes > 0) */
  /*     { */
  /*       p_prc->auto_detect_on_ = false; */

  /*       /\* This will pause the http transfer *\/ */
  /*       pause_needed = true; */

  /*       if (OMX_AUDIO_CodingOGA == p_prc->audio_coding_type_) */
  /*         { */
  /*           /\* Try to identify the actual codec from the ogg stream *\/ */
  /*           p_prc->audio_coding_type_ */
  /*             = identify_ogg_codec (p_prc, (unsigned char *)ap_ptr,
   * a_nbytes); */
  /*           if (OMX_AUDIO_CodingUnused != p_prc->audio_coding_type_) */
  /*             { */
  /*               set_audio_coding_on_port (p_prc); */
  /*               set_audio_info_on_port (p_prc); */
  /*             } */
  /*         } */
  /*       /\* And now trigger the OMX_EventPortFormatDetected and */
  /*          OMX_EventPortSettingsChanged events or a */
  /*          OMX_ErrorFormatNotDetected event *\/ */
  /*       send_port_auto_detect_events (p_prc); */
  /*     } */
  return pause_needed;
}

static bool
connection_lost (OMX_PTR ap_arg)
{
  /*   webmdmuxsrc_prc_t *p_prc = ap_arg; */
  /*   assert (p_prc); */
  /*   prepare_for_port_auto_detection (p_prc); */
  /* Return true to indicate that the automatic reconnection procedure needs to
     be started */
  return true;
}

static OMX_ERRORTYPE
prepare_for_port_auto_detection (webmdmuxsrc_prc_t * ap_prc)
{
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  assert (ap_prc);

  TIZ_INIT_OMX_PORT_STRUCT (port_def,
                            ARATELIA_WEBM_DEMUXER_SOURCE_PORT_0_INDEX);
  tiz_check_omx (
    tiz_api_GetParameter (tiz_get_krn (handleOf (ap_prc)), handleOf (ap_prc),
                          OMX_IndexParamPortDefinition, &port_def));
  ap_prc->audio_coding_type_ = port_def.format.audio.eEncoding;
  ap_prc->auto_detect_on_
    = (OMX_AUDIO_CodingAutoDetect == ap_prc->audio_coding_type_) ? true : false;
  return OMX_ErrorNone;
}

/*
 * webmdmuxsrcprc
 */

static void *
webmdmuxsrc_prc_ctor (void * ap_prc, va_list * app)
{
  webmdmuxsrc_prc_t * p_prc
    = super_ctor (typeOf (ap_prc, "webmdmuxsrcprc"), ap_prc, app);
  assert (p_prc);
  p_prc->p_outhdr_ = NULL;
  p_prc->p_uri_ = NULL;
  p_prc->p_trans_ = NULL;
  p_prc->eos_ = false;
  p_prc->port_disabled_ = false;
  p_prc->uri_changed_ = false;
  p_prc->auto_detect_on_ = false;
  p_prc->audio_coding_type_ = OMX_AUDIO_CodingUnused;
  p_prc->bitrate_ = ARATELIA_WEBM_DEMUXER_DEFAULT_BIT_RATE_KBITS;
  update_cache_size (p_prc);
  p_prc->p_ne_ = NULL;
  p_prc->ne_io_.read = ne_io_read;
  p_prc->ne_io_.seek = ne_io_seek;
  p_prc->ne_io_.tell = ne_io_tell;
  p_prc->ne_io_.userdata = p_prc;
  return p_prc;
}

static void *
webmdmuxsrc_prc_dtor (void * ap_obj)
{
  (void) webmdmuxsrc_prc_deallocate_resources (ap_obj);
  return super_dtor (typeOf (ap_obj, "webmdmuxsrcprc"), ap_obj);
}

/* static OMX_ERRORTYPE */
/* webmdmuxsrc_prc_read_buffer (const void *ap_obj, OMX_BUFFERHEADERTYPE * p_hdr)
 */
/* { */
/*   return OMX_ErrorNone; */
/* } */

/*
 * from tizsrv class
 */

static OMX_ERRORTYPE
webmdmuxsrc_prc_allocate_resources (void * ap_prc, OMX_U32 a_pid)
{
  webmdmuxsrc_prc_t * p_prc = ap_prc;
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  assert (p_prc);
  assert (!p_prc->p_ne_);
  assert (!p_prc->p_uri_);

  tiz_check_omx (obtain_uri (p_prc));

  on_nestegg_error_ret_omx_oom (
    nestegg_init (&p_prc->p_ne_, p_prc->ne_io_, ne_log_cback, -1));

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
      = tiz_urltrans_init (&(p_prc->p_trans_), p_prc, p_prc->p_uri_,
                           ARATELIA_WEBM_DEMUXER_COMPONENT_NAME,
                           ARATELIA_WEBM_DEMUXER_VIDEO_PORT_MIN_BUF_SIZE,
                           ARATELIA_WEBM_DEMUXER_DEFAULT_RECONNECT_TIMEOUT,
                           buffer_cbacks, info_cbacks, io_cbacks, timer_cbacks);
  }
  return rc;
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_deallocate_resources (void * ap_prc)
{
  webmdmuxsrc_prc_t * p_prc = ap_prc;
  assert (p_prc);
  tiz_urltrans_destroy (p_prc->p_trans_);
  p_prc->p_trans_ = NULL;
  delete_uri (p_prc);
  if (p_prc->p_ne_)
    {
      nestegg_destroy (p_prc->p_ne_);
      p_prc->p_ne_ = NULL;
    }
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_prepare_to_transfer (void * ap_prc, OMX_U32 a_pid)
{
  webmdmuxsrc_prc_t * p_prc = ap_prc;
  assert (ap_prc);
  p_prc->eos_ = false;
  tiz_urltrans_cancel (p_prc->p_trans_);
  tiz_urltrans_set_internal_buffer_size (p_prc->p_trans_, p_prc->cache_bytes_);
  return prepare_for_port_auto_detection (p_prc);
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_transfer_and_process (void * ap_prc, OMX_U32 a_pid)
{
  webmdmuxsrc_prc_t * p_prc = ap_prc;
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  assert (p_prc);
  if (p_prc->auto_detect_on_)
    {
      rc = tiz_urltrans_start (p_prc->p_trans_);
    }
  return rc;
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_stop_and_return (void * ap_prc)
{
  webmdmuxsrc_prc_t * p_prc = ap_prc;
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
webmdmuxsrc_prc_buffers_ready (const void * ap_prc)
{
  webmdmuxsrc_prc_t * p_prc = (webmdmuxsrc_prc_t *) ap_prc;
  assert (p_prc);
  return tiz_urltrans_on_buffers_ready (p_prc->p_trans_);
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_io_ready (void * ap_prc, tiz_event_io_t * ap_ev_io, int a_fd,
                          int a_events)
{
  webmdmuxsrc_prc_t * p_prc = ap_prc;
  assert (p_prc);
  return tiz_urltrans_on_io_ready (p_prc->p_trans_, ap_ev_io, a_fd, a_events);
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_timer_ready (void * ap_prc, tiz_event_timer_t * ap_ev_timer,
                             void * ap_arg, const uint32_t a_id)
{
  webmdmuxsrc_prc_t * p_prc = ap_prc;
  assert (p_prc);
  return tiz_urltrans_on_timer_ready (p_prc->p_trans_, ap_ev_timer);
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_pause (const void * ap_obj)
{
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_resume (const void * ap_obj)
{
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_port_flush (const void * ap_obj, OMX_U32 TIZ_UNUSED (a_pid))
{
  webmdmuxsrc_prc_t * p_prc = (webmdmuxsrc_prc_t *) ap_obj;
  if (p_prc->p_trans_)
    {
      tiz_urltrans_flush_buffer (p_prc->p_trans_);
    }
  return release_buffer (p_prc);
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_port_disable (const void * ap_obj, OMX_U32 TIZ_UNUSED (a_pid))
{
  webmdmuxsrc_prc_t * p_prc = (webmdmuxsrc_prc_t *) ap_obj;
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
  return release_buffer ((webmdmuxsrc_prc_t *) ap_obj);
}

static OMX_ERRORTYPE
webmdmuxsrc_prc_port_enable (const void * ap_prc, OMX_U32 a_pid)
{
  webmdmuxsrc_prc_t * p_prc = (webmdmuxsrc_prc_t *) ap_prc;
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

/*
 * webmdmuxsrc_prc_class
 */

static void *
webmdmuxsrc_prc_class_ctor (void * ap_prc, va_list * app)
{
  /* NOTE: Class methods might be added in the future. None for now. */
  return super_ctor (typeOf (ap_prc, "webmdmuxsrcprc_class"), ap_prc, app);
}

/*
 * initialization
 */

void *
webmdmuxsrc_prc_class_init (void * ap_tos, void * ap_hdl)
{
  void * tizprc = tiz_get_type (ap_hdl, "tizprc");
  void * webmdmuxsrcprc_class = factory_new
    /* TIZ_CLASS_COMMENT: class type, class name, parent, size */
    (classOf (tizprc), "webmdmuxsrcprc_class", classOf (tizprc),
     sizeof (webmdmuxsrc_prc_class_t),
     /* TIZ_CLASS_COMMENT: */
     ap_tos, ap_hdl,
     /* TIZ_CLASS_COMMENT: class constructor */
     ctor, webmdmuxsrc_prc_class_ctor,
     /* TIZ_CLASS_COMMENT: stop value*/
     0);
  return webmdmuxsrcprc_class;
}

void *
webmdmuxsrc_prc_init (void * ap_tos, void * ap_hdl)
{
  void * tizprc = tiz_get_type (ap_hdl, "tizprc");
  void * webmdmuxsrcprc_class = tiz_get_type (ap_hdl, "webmdmuxsrcprc_class");
  TIZ_LOG_CLASS (webmdmuxsrcprc_class);
  void * webmdmuxsrcprc = factory_new
    /* TIZ_CLASS_COMMENT: class type, class name, parent, size */
    (webmdmuxsrcprc_class, "webmdmuxsrcprc", tizprc, sizeof (webmdmuxsrc_prc_t),
     /* TIZ_CLASS_COMMENT: */
     ap_tos, ap_hdl,
     /* TIZ_CLASS_COMMENT: class constructor */
     ctor, webmdmuxsrc_prc_ctor,
     /* TIZ_CLASS_COMMENT: class destructor */
     dtor, webmdmuxsrc_prc_dtor,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_allocate_resources, webmdmuxsrc_prc_allocate_resources,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_deallocate_resources, webmdmuxsrc_prc_deallocate_resources,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_prepare_to_transfer, webmdmuxsrc_prc_prepare_to_transfer,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_transfer_and_process, webmdmuxsrc_prc_transfer_and_process,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_stop_and_return, webmdmuxsrc_prc_stop_and_return,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_io_ready, webmdmuxsrc_prc_io_ready,
     /* TIZ_CLASS_COMMENT: */
     tiz_srv_timer_ready, webmdmuxsrc_prc_timer_ready,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_buffers_ready, webmdmuxsrc_prc_buffers_ready,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_pause, webmdmuxsrc_prc_pause,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_resume, webmdmuxsrc_prc_resume,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_port_flush, webmdmuxsrc_prc_port_flush,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_port_disable, webmdmuxsrc_prc_port_disable,
     /* TIZ_CLASS_COMMENT: */
     tiz_prc_port_enable, webmdmuxsrc_prc_port_enable,
     /* TIZ_CLASS_COMMENT: stop value */
     0);

  return webmdmuxsrcprc;
}
