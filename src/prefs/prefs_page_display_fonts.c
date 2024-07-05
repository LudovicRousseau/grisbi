/* *******************************************************************************/
/*                                 GRISBI                                        */
/*              Programme de gestion financière personnelle                      */
/*                              license : GPLv2                                  */
/*                                                                               */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)               */
/*                      2003-2008 Benjamin Drieu (bdrieu@april.org)              */
/*          2008-2020 Pierre Biava (grisbi@pierre.biava.name)                    */
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

#include <errno.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

/*START_INCLUDE*/
#include "prefs_page_display_fonts.h"
#include "accueil.h"
#include "gsb_automem.h"
#include "custom_list.h"
#include "dialog.h"
#include "grisbi_app.h"
#include "gsb_data_account.h"
#include "gsb_dirs.h"
#include "gsb_file.h"
#include "gsb_file_save.h"
#include "gsb_rgba.h"
#include "gsb_scheduler_list.h"
#include "gsb_select_icon.h"
#include "navigation.h"
#include "structures.h"
#include "transaction_list.h"
#include "utils_files.h"
#include "utils_prefs.h"
#include "widget_css_rules.h"
#include "erreur.h"

/*END_INCLUDE*/

/*START_EXTERN*/
/*END_EXTERN*/

typedef struct _PrefsPageDisplayFontsPrivate   PrefsPageDisplayFontsPrivate;

struct _PrefsPageDisplayFontsPrivate
{
	GtkWidget *			vbox_display_fonts;

    GtkWidget *			checkbutton_display_logo;
    GtkWidget *         hbox_display_logo;
    GtkWidget *         button_display_logo;
	GtkWidget *			preview_display_logo;
    GtkWidget *         vbox_display_logo;				/* sert à invalider le choix quand pas de fichier chargé */

    GtkWidget *			checkbutton_display_fonts;
    GtkWidget *         hbox_display_fonts;

	GtkWidget *			combo_force_theme;
	GtkWidget *			label_theme_selected;

    GtkWidget *         box_css_rules;
	GtkWidget *			w_css_rules;
};

G_DEFINE_TYPE_WITH_PRIVATE (PrefsPageDisplayFonts, prefs_page_display_fonts, GTK_TYPE_BOX)

/******************************************************************************/
/* Private functions                                                          */
/******************************************************************************/
/**
 * Modifie manuellement le thème de grisbi quand la détection automatique
 * ne fonctionne pas.
 *
 * \param
 * \param
 * \param
 *
 * \return FALSE
 **/
static void	prefs_page_display_fonts_combo_force_theme_changed (GtkWidget *combo,
																PrefsPageDisplayFonts *page)
{
	GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
    {
		GtkTreeModel *model;
		gint value;
		GrisbiAppConf *a_conf;

		a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 1, &value, -1);

		if (value)
		{
			if (value == 1)						/* c'est un theme standard */
				a_conf->force_type_theme = 0;
			else
				a_conf->force_type_theme = value;	/* dark theme ou light theme */
			a_conf->use_type_theme = value;

			grisbi_app_window_style_updated (GTK_WIDGET (grisbi_app_get_active_window (NULL)), GINT_TO_POINTER (TRUE));
		}
		else
		{
			a_conf->force_type_theme = 0;
			a_conf->use_type_theme = 0;
		}

	}
}

/**
 * update the font in all the transactions in the list
 *
 * \param
 * \param
 *
 * \return
 * */
static void prefs_page_display_fonts_update_fonte_listes (gchar *fontname,
                                                          GrisbiAppConf *a_conf)
{
    GValue value = G_VALUE_INIT;
    gchar *font;

    if (a_conf->custom_fonte_listes)
		font = fontname;
    else
	{
		font = NULL;
		return;
	}

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, font);
    transaction_list_update_column (CUSTOM_MODEL_FONT, &value);
}

/**
 * update the preview of the logo file chooser
 *
 * \param file_chooser
 * \param preview
 *
 * \return FALSE
 * */
static void prefs_page_display_fonts_change_logo_accueil (GtkWidget *file_selector,
                                                          PrefsPageDisplayFonts *page)
{
	GtkWidget *logo_accueil;
	GtkWidget *preview;
    GdkPixbuf *pixbuf;
	const gchar *selected_filename;
	gchar *chemin_logo;
	GrisbiWinEtat *w_etat;
	PrefsPageDisplayFontsPrivate *priv;

	if (!gsb_data_account_get_number_of_accounts ())
		return;

	priv = prefs_page_display_fonts_get_instance_private (page);
    selected_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_selector));

	/* on supprime d'abord l'ancien logo */
	gtk_widget_destroy (priv->preview_display_logo);

	/* on change le logo dans le bouton */
	chemin_logo = g_strstrip (g_strdup (selected_filename));
	logo_accueil = gsb_main_page_get_logo_accueil ();
	if (logo_accueil && GTK_IS_WIDGET (logo_accueil))
		gtk_widget_hide (logo_accueil);

	/* create logo */
	pixbuf = gdk_pixbuf_new_from_file (chemin_logo, NULL);
	if (!pixbuf)
	{
		pixbuf = gsb_select_icon_get_default_logo_pixbuf ();
	}
	else
	{
		w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();
		if (g_strcmp0 (g_path_get_dirname (chemin_logo), gsb_dirs_get_pixmaps_dir ()) == 0)
		{
			gchar *name_logo;

			name_logo = g_path_get_basename (chemin_logo);
			if (g_strcmp0 (name_logo, "grisbi.svg") != 0)
				w_etat->name_logo = chemin_logo;
			else
				w_etat->name_logo = NULL;
		}
		else
		{
			if (w_etat->name_logo && strlen (w_etat->name_logo))
			{
				g_free (w_etat->name_logo);
				w_etat->name_logo = chemin_logo;
			}
		}
	}

	gsb_select_icon_set_logo_pixbuf (pixbuf);

	/* mis à jour du bouton contenant le logo */
	preview = gtk_image_new_from_pixbuf (gdk_pixbuf_scale_simple (pixbuf,
																  LOGO_WIDTH,
																  LOGO_HEIGHT,
																  GDK_INTERP_BILINEAR));
	gtk_widget_show (preview);
	gtk_container_add (GTK_CONTAINER (priv->button_display_logo), preview);
	priv->preview_display_logo = preview;

	/* Update homepage logo */
	gtk_widget_destroy (logo_accueil);

	logo_accueil = gtk_image_new_from_pixbuf (gdk_pixbuf_scale_simple (pixbuf,
																	   LOGO_WIDTH,
																	   LOGO_HEIGHT,
																	   GDK_INTERP_BILINEAR));
	gsb_main_page_set_logo_accueil (logo_accueil);

	/* modify the icon of grisbi (set in the panel of gnome or other) */
	gtk_window_set_default_icon (pixbuf);

	/* Mark file as modified */
	utils_prefs_gsb_file_set_modified ();
}

/**
 * update the preview of the log file chooser
 *
 * \param file_chooser
 * \param preview
 *
 * \return FALSE
 * */
static gboolean prefs_page_display_fonts_update_preview_logo (GtkFileChooser *file_chooser,
															  GtkWidget *preview)
{
  gchar *filename;
  GdkPixbuf *pixbuf;
  gboolean have_preview = FALSE;

  filename = gtk_file_chooser_get_preview_filename (file_chooser);
  if (!filename)
      return FALSE;

  pixbuf = gdk_pixbuf_new_from_file_at_size (filename, LOGO_WIDTH, LOGO_HEIGHT, NULL);
  g_free (filename);

	if (pixbuf)
	{
	  	have_preview = TRUE;
		gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
		g_object_unref (pixbuf);
	}
	gtk_file_chooser_set_preview_widget_active (file_chooser, have_preview);

	return FALSE;
}

/**
 *
 *
 * \param
 *
 * \return
 * */
static gboolean prefs_page_display_fonts_logo_accueil_changed (PrefsPageDisplayFonts *page)
{
	GtkWidget *prefs_dialog;
	GtkWidget *button_cancel;
	GtkWidget *button_open;
    GtkWidget *file_selector;
    GtkWidget *preview;
    gchar *tmp_last_directory;
	GrisbiWinEtat *w_etat;

	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();

	prefs_dialog = grisbi_win_get_prefs_dialog (NULL);
    file_selector = gtk_file_chooser_dialog_new (_("Select a new logo"),
												 GTK_WINDOW (prefs_dialog),
												 GTK_FILE_CHOOSER_ACTION_OPEN,
												 NULL, NULL,
												 NULL);

	button_cancel = gtk_button_new_with_label (_("Cancel"));
	gtk_dialog_add_action_widget (GTK_DIALOG (file_selector), button_cancel, GTK_RESPONSE_CANCEL);
	gtk_widget_set_can_default (button_cancel, TRUE);

	button_open = gtk_button_new_with_label (_("Open"));
	gtk_dialog_add_action_widget (GTK_DIALOG (file_selector), button_open, GTK_RESPONSE_OK);
	gtk_widget_set_can_default (button_open, TRUE);

	if (w_etat->name_logo)
	{
		gchar *dirname;

		dirname = g_path_get_dirname (w_etat->name_logo);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_selector), dirname);
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (file_selector), w_etat->name_logo);
	}
    else
	{
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_selector),
											 gsb_dirs_get_pixmaps_dir ());
	}

    gtk_window_set_position (GTK_WINDOW (file_selector), GTK_WIN_POS_CENTER_ON_PARENT);

    /* create the preview */
    preview = gtk_image_new ();
    gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (file_selector), preview);
    g_signal_connect (G_OBJECT (file_selector),
                        "update-preview",
                        G_CALLBACK (prefs_page_display_fonts_update_preview_logo),
                        preview);

	gtk_widget_show_all (file_selector);

    switch (gtk_dialog_run (GTK_DIALOG (file_selector)))
    {
		case GTK_RESPONSE_OK:
			prefs_page_display_fonts_change_logo_accueil (file_selector, page);
			tmp_last_directory = utils_files_selection_get_last_directory (GTK_FILE_CHOOSER (file_selector), TRUE);
			gsb_file_update_last_path (tmp_last_directory);
			g_free (tmp_last_directory);

		default:
			break;
    }

	gtk_widget_destroy (file_selector);

	return (FALSE);
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
static void prefs_page_display_fonts_init_combo_force_theme (PrefsPageDisplayFonts *page)
{
    GtkListStore *store = NULL;
    GtkCellRenderer *renderer;
	const gchar *text_force_theme[] = {N_("Automatic selection"),
									   N_("Force the use of clear theme"),
									   N_("Force the use of dark theme"),
									   N_("Force the use of light theme"),
									   NULL};
	gchar *tmp_label;
	gint i = 0;
	GrisbiAppConf *a_conf;
	PrefsPageDisplayFontsPrivate *priv;

	devel_debug (NULL);
	priv = prefs_page_display_fonts_get_instance_private (page);
	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();

	/* set label with the name of selected theme */
	tmp_label = g_strdup_printf (_("The automatically selected theme is: '%s'"), a_conf->current_theme);
	gtk_label_set_label (GTK_LABEL (priv->label_theme_selected), tmp_label);
	g_free (tmp_label);

	/* set store for combo */
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    while (text_force_theme[i])
    {
        GtkTreeIter iter;
        gchar *string;

        string = gettext (text_force_theme[i]);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, 0, string, 1, i, -1);

        i++;
    }

    gtk_combo_box_set_model (GTK_COMBO_BOX (priv->combo_force_theme), GTK_TREE_MODEL (store));

	/* set properties of combo */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->combo_force_theme), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->combo_force_theme),
									renderer,
                                    "text", 0,
                                    NULL);

    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_force_theme), a_conf->force_type_theme);
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 * */
static gboolean prefs_page_display_fonts_utilise_logo_checked (GtkWidget *check_button,
															   GtkWidget *hbox)
{
	GrisbiWinEtat *w_etat;

	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();
	w_etat->utilise_logo = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button));
    gtk_widget_set_sensitive (hbox, w_etat->utilise_logo);

    if (w_etat->utilise_logo)
    {
		GtkWidget *logo_accueil;

		/* 	on recharge l'ancien logo */
		logo_accueil = gsb_main_page_get_logo_accueil ();
        if (GTK_IS_WIDGET (logo_accueil))
		{
            gtk_widget_hide (logo_accueil);
		}
        else
        {
            GdkPixbuf *pixbuf = NULL;

            /* Update homepage logo */
            pixbuf = gsb_select_icon_get_logo_pixbuf ();
            if (pixbuf == NULL)
            {
                pixbuf = gsb_select_icon_get_default_logo_pixbuf ();
				logo_accueil =  gtk_image_new_from_pixbuf (pixbuf);
				g_object_unref (G_OBJECT (pixbuf));
            }
			else
			{
            	logo_accueil =  gtk_image_new_from_pixbuf (pixbuf);
			}
            if (logo_accueil)
				gsb_main_page_set_logo_accueil (logo_accueil);
			else
				gsb_main_page_set_logo_accueil (NULL);
        }
    }
	else
		gsb_main_page_set_logo_accueil (NULL);

    utils_prefs_gsb_file_set_modified ();

    return (FALSE);
}

/**
 * Création de la page de gestion des display_fonts
 *
 * \param prefs
 *
 * \return
 **/
static void prefs_page_display_fonts_setup_page (PrefsPageDisplayFonts *page)
{
	GtkWidget *head_page;
	GtkWidget *font_button;
	GtkWidget *preview;
	GdkPixbuf *pixbuf = NULL;
	gboolean is_loading;
	GrisbiAppConf *a_conf;
	GrisbiWinEtat *w_etat;
	GrisbiWinRun *w_run = NULL;
	PrefsPageDisplayFontsPrivate *priv;

	devel_debug (NULL);
	priv = prefs_page_display_fonts_get_instance_private (page);
	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();

	/* On récupère le nom de la page */
	head_page = utils_prefs_head_page_new_with_title_and_icon (_("Fonts & logo"), "gsb-fonts-32.png");
	gtk_box_pack_start (GTK_BOX (priv->vbox_display_fonts), head_page, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (priv->vbox_display_fonts), head_page, 0);

    /* set the elements for logo */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_display_logo), w_etat->utilise_logo);

	/* le logo est grisé ou non suivant qu'on l'utilise ou pas */
    gtk_widget_set_sensitive (priv->hbox_display_logo, w_etat->utilise_logo);

	/* set the logo */
	pixbuf = gsb_select_icon_get_logo_pixbuf ();

    if (!pixbuf)
    {
		pixbuf = gsb_select_icon_get_default_logo_pixbuf ();
        preview = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);
    }
    else
    {
		preview = gtk_image_new_from_pixbuf (pixbuf);
    }
	priv->preview_display_logo = preview;

	gtk_container_add (GTK_CONTAINER (priv->button_display_logo), preview);
    g_signal_connect_swapped (G_OBJECT (priv->button_display_logo),
							  "clicked",
							  G_CALLBACK (prefs_page_display_fonts_logo_accueil_changed),
							  page);

    /* Connect signal */
    g_signal_connect (priv->checkbutton_display_logo,
					  "toggled",
					  G_CALLBACK (prefs_page_display_fonts_utilise_logo_checked),
					  priv->hbox_display_logo);

	/* set the elements for fonts */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_display_fonts),
								  a_conf->custom_fonte_listes);

	/* Create font button */
    font_button = utils_prefs_fonts_create_button (&a_conf->font_string,
                                                   G_CALLBACK (prefs_page_display_fonts_update_fonte_listes),
                                                   a_conf);
	g_object_set_data (G_OBJECT (priv->checkbutton_display_fonts), "widget", font_button);
    gtk_box_pack_start (GTK_BOX (priv->hbox_display_fonts), font_button, FALSE, FALSE, 0);

    if (!a_conf->custom_fonte_listes)
    {
        gtk_widget_set_sensitive (font_button, FALSE);
    }
	else
		gtk_widget_set_sensitive (font_button, TRUE);

	g_signal_connect (priv->checkbutton_display_fonts,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &a_conf->custom_fonte_listes);

	/* set the themes buttons */
	prefs_page_display_fonts_init_combo_force_theme (page);

	/* Connect signal combo_force_theme */
    g_signal_connect (priv->combo_force_theme,
					  "changed",
					  G_CALLBACK (prefs_page_display_fonts_combo_force_theme_changed),
					  page);

	/* set css rules */
	priv->w_css_rules = GTK_WIDGET (widget_css_rules_new (GTK_WIDGET (page)));
	gtk_box_pack_start (GTK_BOX (priv->box_css_rules), priv->w_css_rules, TRUE, TRUE, 0);

	/* set memorisation de l'onglet selectionne si 1 fichier chargé */
	is_loading = grisbi_win_file_is_loading ();
	if (is_loading)
	{
		GtkWidget *notebook;

		w_run = (GrisbiWinRun *) grisbi_win_get_w_run ();
		notebook = widget_css_rules_get_notebook (priv->w_css_rules);
		if (w_run->prefs_css_rules_tab)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),  w_run->prefs_css_rules_tab);

		/* set signal notebook_css_rules */
		g_signal_connect (G_OBJECT (notebook),
						  "switch-page",
						  (GCallback) utils_prefs_page_notebook_switch_page,
						  &w_run->prefs_css_rules_tab);
	}
}

/******************************************************************************/
/* Fonctions propres à l'initialisation des fenêtres                          */
/******************************************************************************/
static void prefs_page_display_fonts_init (PrefsPageDisplayFonts *page)
{
	gtk_widget_init_template (GTK_WIDGET (page));

	prefs_page_display_fonts_setup_page (page);
}

static void prefs_page_display_fonts_dispose (GObject *object)
{
	G_OBJECT_CLASS (prefs_page_display_fonts_parent_class)->dispose (object);
}

static void prefs_page_display_fonts_class_init (PrefsPageDisplayFontsClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = prefs_page_display_fonts_dispose;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
												 "/org/gtk/grisbi/prefs/prefs_page_display_fonts.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, vbox_display_fonts);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, button_display_logo);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, checkbutton_display_logo);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, hbox_display_logo);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, vbox_display_logo);

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, checkbutton_display_fonts);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, hbox_display_fonts);

	//~ gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, box_select_theme);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, label_theme_selected);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, combo_force_theme);

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDisplayFonts, box_css_rules);
}

/******************************************************************************/
/* Public functions                                                           */
/******************************************************************************/
/**
 *
 *
 * \param
 *
 * \return
 **/
PrefsPageDisplayFonts *prefs_page_display_fonts_new (GrisbiPrefs *win)
{
	return g_object_new (PREFS_PAGE_DISPLAY_FONTS_TYPE, NULL);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
/* Local Variables: */
/* c-basic-offset: 4 */
/* End: */

