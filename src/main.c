/* *******************************************************************************/
/*                                 GRISBI                                        */
/*              Programme de gestion financière personnelle                      */
/*                              license : GPLv2                                  */
/*                                                                               */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)               */
/*                      2003-2008 Benjamin Drieu (bdrieu@april.org)              */
/*          2008-2023 Pierre Biava (grisbi@pierre.biava.name)                    */
/*          https://www.grisbi.org/                                              */
/*                                                                               */
/*     This program is free software; you can redistribute it and/or modify      */
/*     it under the terms of the GNU General Public License as published by      */
/*     the Free Software Foundation; either version 2 of the License, or         */
/*     (at your option) any later version.                                       */
/*                                                                               */
/*     This program is distributed in the hope that it will be useful,           */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/*     GNU General Public License for more details.                              */
/*                                                                               */
/*     You should have received a copy of the GNU General Public License         */
/*     along with this program; if not, write to the Free Software               */
/*     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/*                                                                               */
/* *******************************************************************************/

#include "config.h"

#include "include.h"
#include <gtk/gtk.h>
#include <libintl.h>

#ifdef HAVE_GOFFICE
#include <goffice/goffice.h>
#endif /* HAVE_GOFFICE */


#ifdef G_OS_WIN32
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <unistd.h>
#endif /* WIN32 */

/*START_INCLUDE*/
#include "grisbi_app.h"
#include "gsb_dirs.h"
#include "gsb_locale.h"
#include "structures.h"
#include "erreur.h"

#ifdef __APPLE__
# include "grisbi_osx.h"
#endif
/*END_INCLUDE*/

/*START_EXTERN*/
/*END_EXTERN*/

/**
 * Main function
 *
 * @param argc number of arguments
 * @param argv arguments
 *
 * @return Nothing
 */
int main (int argc, char **argv)
{
	gint status;
	GSList *goffice_plugins_dirs = NULL;

#ifdef EARLY_DEBUG
	debug_start_log_file ();
#endif

#ifdef __APPLE__
	goffice_plugins_dirs = grisbi_osx_init (&argc, argv);
#endif

	 /* On force l'utilisation de X11 en attendant que grisbi fonctionne correctement sous wayland */
#ifdef GDK_WINDOWING_WAYLAND
	#ifdef GDK_WINDOWING_X11
		gdk_set_allowed_backends ("x11");
	#else
		return (1);
	#endif
#endif

	/* On commence par initialiser les répertoires */
	gsb_dirs_init (argv[0]);

	/* Setup locale/gettext */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, gsb_dirs_get_locale_dir ());
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

#ifdef HAVE_GOFFICE
	/* initialisation libgoffice */
	libgoffice_init ();
	/* Initialize plugins manager */
	go_plugins_init (NULL, NULL, NULL, goffice_plugins_dirs, TRUE, GO_TYPE_PLUGIN_LOADER_MODULE);
	/* Note: go_plugins_init will g_slist_free(goffice_plugins_dirs) ! */
#else
	(void) goffice_plugins_dirs; /* Avoid warning */
#endif /* HAVE_GOFFICE */

	/* on execute la boucle principale de grisbi */
	status = g_application_run (G_APPLICATION (grisbi_app_new ()), argc, argv);

#ifdef HAVE_GOFFICE
	/* liberation libgoffice */
	libgoffice_shutdown ();
#endif /* HAVE_GOFFICE */

	return status;
}

