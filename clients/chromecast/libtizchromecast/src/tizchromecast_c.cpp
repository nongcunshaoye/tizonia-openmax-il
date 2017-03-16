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
 * @file   tizchromecast_c.cpp
 * @author Juan A. Rubio <juan.rubio@aratelia.com>
 *
 * @brief  Tizonia - Simple Chromecast client library (c wrapper)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include "tizchromecast.hpp"
#include "tizchromecast_c.h"

struct tiz_chromecast
{
  tizchromecast *p_proxy_;
};

static void chromecast_free_data (tiz_chromecast_t *ap_chromecast)
{
  if (ap_chromecast)
    {
      delete ap_chromecast->p_proxy_;
      ap_chromecast->p_proxy_ = NULL;
    }
}

static int chromecast_alloc_data (tiz_chromecast_t *ap_chromecast,
                                  const char *ap_name_or_ip)
{
  int rc = 0;
  assert (ap_chromecast);
  try
    {
      ap_chromecast->p_proxy_ = new tizchromecast (ap_name_or_ip);
    }
  catch (...)
    {
      chromecast_free_data (ap_chromecast);
      rc = 1;
    }
  return rc;
}

extern "C" int tiz_chromecast_init (tiz_chromecast_ptr_t *app_chromecast,
                                const char *ap_name_or_ip)
{
  tiz_chromecast_t *p_chromecast = NULL;
  int rc = 1;

  assert (app_chromecast);
  assert (ap_name_or_ip);

  if ((p_chromecast = (tiz_chromecast_t *)calloc (1, sizeof(tiz_chromecast_t))))
    {
      if (!chromecast_alloc_data (p_chromecast, ap_name_or_ip))
        {
          tizchromecast *p_gm = p_chromecast->p_proxy_;
          assert (p_gm);
          if (!p_gm->init () && !p_gm->start ())
            {
              // all good
              rc = 0;
            }
        }

      if (0 != rc)
        {
          chromecast_free_data (p_chromecast);
          free (p_chromecast);
          p_chromecast = NULL;
        }
    }

  *app_chromecast = p_chromecast;

  return rc;
}

extern "C" void tiz_chromecast_destroy (tiz_chromecast_t *ap_chromecast)
{
  if (ap_chromecast)
    {
      tizchromecast *p_gm = ap_chromecast->p_proxy_;
      if (p_gm)
        {
          p_gm->stop ();
          p_gm->deinit ();
        }
      chromecast_free_data (ap_chromecast);
      free (ap_chromecast);
    }
}
