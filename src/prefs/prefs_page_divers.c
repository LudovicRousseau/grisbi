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
#include "prefs_page_divers.h"
#include "bet_finance_ui.h"
#include "dialog.h"
#include "grisbi_app.h"
#include "gsb_account.h"
#include "gsb_automem.h"
#include "gsb_dirs.h"
#include "gsb_locale.h"
#include "gsb_regex.h"
#include "gsb_rgba.h"
#include "navigation.h"
#include "structures.h"
#include "utils_dates.h"
#include "utils_prefs.h"
#include "utils_real.h"
#include "erreur.h"

/*END_INCLUDE*/

/*START_EXTERN*/
/*END_EXTERN*/

typedef struct _PrefsPageDiversPrivate   PrefsPageDiversPrivate;

struct _PrefsPageDiversPrivate
{
	GtkWidget *			vbox_divers;

	/* generalities */
    GtkWidget *			grid_generalities;
	GtkWidget *			button_reset_prefs_window;
	GtkWidget *			entry_web_browser;
	GtkWidget *			hbox_display_pdf;
	GtkWidget *			label_prefs_settings;
	GtkWidget *			notebook_divers;
	GtkWidget *			radiobutton_display_html;
	GtkWidget *			radiobutton_display_pdf;

	/* scheduler */
	GtkWidget *         vbox_launch_scheduler;
	GtkWidget *			checkbutton_scheduler_set_default_account;
	GtkWidget *			checkbutton_scheduler_set_fixed_date;
	GtkWidget *			checkbutton_scheduler_set_fixed_day;
	GtkWidget *         hbox_launch_scheduler_set_default_account;
	GtkWidget *			hbox_launch_scheduler_nb_days_before_scheduled;
	GtkWidget *         hbox_launch_scheduler_set_fixed_date;
	GtkWidget *         hbox_launch_scheduler_set_fixed_day;
	GtkWidget *			radiobutton_select_first_scheduled;
	GtkWidget *			radiobutton_select_last_scheduled;
    GtkWidget *         spinbutton_nb_days_before_scheduled;
	GtkWidget *         spinbutton_scheduler_fixed_day;

	/* localization */
	GtkWidget *			box_choose_date_format;
	GtkWidget *			box_choose_numbers_format;
	GtkWidget *			combo_choose_language;
	GtkWidget *			combo_choose_decimal_point;
	GtkWidget *			combo_choose_thousands_separator;
	GtkWidget *			radiobutton_choose_date_1;
	GtkWidget *			radiobutton_choose_date_2;
	GtkWidget *			radiobutton_choose_date_3;
	GtkWidget *			radiobutton_choose_date_4;

};

G_DEFINE_TYPE_WITH_PRIVATE (PrefsPageDivers, prefs_page_divers, GTK_TYPE_BOX)

/******************************************************************************/
/* Private functions                                                          */
/******************************************************************************/
/**
 *
 *
 * \param
 * \param
 * \param
 *
 * \return
 **/
static gboolean prefs_page_divers_radiobutton_help_press_event (GtkWidget *button,
																GdkEvent  *event,
																GrisbiAppConf *a_conf)
{
	gint button_number;

	button_number = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "pointer"));
	if (button_number == 1)
		a_conf->display_help = 0;
	else
		a_conf->display_help = 1;

	return FALSE;
}
/**
 *
 *
 * \param
 *
 * \return
 **/
static void prefs_page_divers_button_reset_prefs_window_clicked (GtkButton *button,
																 GrisbiPrefs *prefs)
{
	GtkWidget *paned_prefs;
 	GrisbiAppConf *a_conf;

	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
	paned_prefs = grisbi_prefs_get_prefs_hpaned (GRISBI_PREFS (prefs));
	a_conf->prefs_height = PREFS_WIN_MIN_HEIGHT;
	a_conf->prefs_panel_width = PREFS_PANED_MIN_WIDTH;

	if (a_conf->low_definition_screen)
	{
		a_conf->prefs_width = PREFS_WIN_MIN_WIDTH_LOW;
		gtk_widget_set_size_request(GTK_WIDGET (prefs), PREFS_WIN_MIN_WIDTH_LOW, PREFS_WIN_MIN_HEIGHT);
	}
	else
	{
		a_conf->prefs_width = PREFS_WIN_MIN_WIDTH_HIGH;
		gtk_widget_set_size_request(GTK_WIDGET (prefs), PREFS_WIN_MIN_WIDTH_HIGH, PREFS_WIN_MIN_HEIGHT);
	}
	gtk_paned_set_position (GTK_PANED (paned_prefs), PREFS_PANED_MIN_WIDTH);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
static void prefs_page_divers_choose_thousands_sep_changed (GtkComboBoxText *widget,
															gpointer null)
{
    GtkWidget *combo_box;
    GtkWidget *entry;
    gchar *str_capital;
    const gchar *text;
	GrisbiWinEtat *w_etat;

	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();

    text = gtk_combo_box_text_get_active_text (widget);
    combo_box = g_object_get_data (G_OBJECT (widget), "separator");

    if (g_strcmp0 (text, "' '") == 0)
    {
        gsb_locale_set_mon_thousands_sep (" ");
    }
    else if (g_strcmp0 (text, ".") == 0)
    {

        gsb_locale_set_mon_thousands_sep (".");
        if (g_strcmp0 (gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box)), ".") == 0)
        {
            gsb_locale_set_mon_decimal_point (",");
            gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 1);
        }
    }
    else if (g_strcmp0 (text, ",") == 0)
    {

        gsb_locale_set_mon_thousands_sep (",");
        if (g_strcmp0 (gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box)), ",") == 0)
        {
            gsb_locale_set_mon_decimal_point (".");
            gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
        }
    }
    else
        gsb_locale_set_mon_thousands_sep (NULL);

    /* reset capital */
    entry = bet_finance_ui_get_capital_entry ();
    str_capital = utils_real_get_string_with_currency (gsb_real_double_to_real (
                    w_etat->bet_capital),
                    w_etat->bet_currency,
                    FALSE);

    gtk_entry_set_text (GTK_ENTRY (entry), str_capital);
    g_free (str_capital);

    gsb_gui_navigation_update_localisation (1);
}

static void prefs_page_divers_choose_decimal_point_changed (GtkComboBoxText *widget,
															gpointer null)
{
    GtkWidget *combo_box;
    GtkWidget *entry;
    gchar *str_capital;
    const gchar *text;
	GrisbiWinEtat *w_etat;

	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();

	text = gtk_combo_box_text_get_active_text (widget);
    combo_box = g_object_get_data (G_OBJECT (widget), "separator");

    if (g_strcmp0 (text, ",") == 0)
    {
        gsb_locale_set_mon_decimal_point (",");

        if (g_strcmp0 (gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box)), ",") == 0)
        {
            gsb_locale_set_mon_thousands_sep (" ");
            gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
        }
    }
    else
    {
        gsb_locale_set_mon_decimal_point (".");
        if (g_strcmp0 (gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box)), ".") == 0)
        {
            gsb_locale_set_mon_thousands_sep (",");
            gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 2);
        }
    }

    /* reset capital */
    entry = bet_finance_ui_get_capital_entry ();
    str_capital = utils_real_get_string_with_currency (gsb_real_double_to_real (
                    w_etat->bet_capital),
                    w_etat->bet_currency,
                    FALSE);

    gtk_entry_set_text (GTK_ENTRY (entry), str_capital);
    g_free (str_capital);

    gsb_gui_navigation_update_localisation (1);
}

static void prefs_page_divers_choose_number_format_init (PrefsPageDivers *page)
{
    gchar *mon_decimal_point;
    gchar *mon_thousands_sep;
	PrefsPageDiversPrivate *priv;

	priv = prefs_page_divers_get_instance_private (page);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo_choose_thousands_separator), "' '");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo_choose_thousands_separator), ".");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo_choose_thousands_separator), ",");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (priv->combo_choose_thousands_separator), "''");

    mon_decimal_point = gsb_locale_get_mon_decimal_point ();
    if (strcmp (mon_decimal_point, ",") == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_choose_decimal_point), 1);
    else
        gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_choose_decimal_point), 0);
    g_free (mon_decimal_point);

    mon_thousands_sep = gsb_locale_get_mon_thousands_sep ();
    if (mon_thousands_sep == NULL)
        gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_choose_thousands_separator), 3);
    else if (strcmp (mon_thousands_sep, ".") == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_choose_thousands_separator), 1);
    else if (strcmp (mon_thousands_sep, ",") == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_choose_thousands_separator), 2);
    else
        gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_choose_thousands_separator), 0);

    if (mon_thousands_sep)
        g_free (mon_thousands_sep);

    g_object_set_data (G_OBJECT (priv->combo_choose_decimal_point), "separator", priv->combo_choose_thousands_separator);
    g_object_set_data (G_OBJECT (priv->combo_choose_thousands_separator), "separator", priv->combo_choose_decimal_point);

	g_signal_connect (G_OBJECT (priv->combo_choose_decimal_point),
					  "changed",
					  G_CALLBACK (prefs_page_divers_choose_decimal_point_changed),
					  NULL);
    g_signal_connect (G_OBJECT (priv->combo_choose_thousands_separator),
                        "changed",
                        G_CALLBACK (prefs_page_divers_choose_thousands_sep_changed),
                        NULL);
}

static gboolean prefs_page_divers_choose_date_format_toggle (GtkToggleButton *togglebutton,
															 GdkEventButton *event,
															 gpointer null)
{
    const gchar *format_date;
	GrisbiWinRun *w_run;

	w_run = grisbi_win_get_w_run ();

    format_date = g_object_get_data (G_OBJECT (togglebutton), "pointer");
    gsb_date_set_format_date (format_date);
	gsb_regex_init_variables ();

	if (grisbi_win_file_is_loading () && !w_run->new_account_file)
		gsb_gui_navigation_update_localisation (0);

    return FALSE;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
static void  prefs_page_divers_choose_date_format_init (PrefsPageDivers *page)
{
    gchar *format_date;
	PrefsPageDiversPrivate *priv;

	priv = prefs_page_divers_get_instance_private (page);

    format_date = g_strdup ("%d/%m/%Y");
    g_object_set_data_full (G_OBJECT (priv->radiobutton_choose_date_1), "pointer", format_date, g_free);

    format_date = g_strdup ("%m/%d/%Y");
    g_object_set_data_full (G_OBJECT (priv->radiobutton_choose_date_2), "pointer", format_date, g_free);

    format_date = g_strdup ("%d.%m.%Y");
    g_object_set_data_full (G_OBJECT (priv->radiobutton_choose_date_3), "pointer", format_date, g_free);

    format_date = g_strdup ("%Y-%m-%d");
    g_object_set_data_full (G_OBJECT (priv->radiobutton_choose_date_4), "pointer", format_date, g_free);

    format_date = gsb_date_get_format_date ();
    if (format_date)
    {
        if (strcmp (format_date, "%m/%d/%Y") == 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->radiobutton_choose_date_2), TRUE);
        else if (strcmp (format_date, "%d.%m.%Y") == 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->radiobutton_choose_date_3), TRUE);
        else if (strcmp (format_date, "%Y-%m-%d") == 0)
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->radiobutton_choose_date_4), TRUE);

        g_free (format_date);
    }

    g_signal_connect (G_OBJECT (priv->radiobutton_choose_date_1),
                        "button-release-event",
                        G_CALLBACK (prefs_page_divers_choose_date_format_toggle),
                        NULL);
    g_signal_connect (G_OBJECT (priv->radiobutton_choose_date_2),
                        "button-release-event",
                        G_CALLBACK (prefs_page_divers_choose_date_format_toggle),
                        NULL);
    g_signal_connect (G_OBJECT (priv->radiobutton_choose_date_3),
                        "button-release-event",
                        G_CALLBACK (prefs_page_divers_choose_date_format_toggle),
                        NULL);
    g_signal_connect (G_OBJECT (priv->radiobutton_choose_date_4),
                        "button-release-event",
                        G_CALLBACK (prefs_page_divers_choose_date_format_toggle),
                        NULL);
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
static gboolean prefs_page_divers_scheduler_warm_button_changed (GtkWidget *checkbutton,
																 PrefsPageDivers *page)
{
    gboolean *value;

    value = g_object_get_data (G_OBJECT (checkbutton), "pointer");
    if (value)
    {
		PrefsPageDiversPrivate *priv;

		*value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton));
		priv = prefs_page_divers_get_instance_private (page);
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)))
		{
			gtk_widget_set_sensitive (GTK_WIDGET (priv->spinbutton_nb_days_before_scheduled), FALSE);
			gtk_widget_set_sensitive (GTK_WIDGET (priv->hbox_launch_scheduler_set_fixed_day), TRUE);
		}
		else
		{
			gtk_widget_set_sensitive (GTK_WIDGET (priv->spinbutton_nb_days_before_scheduled), TRUE);
			gtk_widget_set_sensitive (GTK_WIDGET (priv->hbox_launch_scheduler_set_fixed_day), FALSE);
		}
	}

    return FALSE;
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
static gboolean prefs_page_divers_scheduler_set_fixed_day_changed (GtkWidget *checkbutton,
																   GtkWidget *widget)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)))
		gtk_widget_set_sensitive (GTK_WIDGET (widget), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);

    return FALSE;
}

/**
 * callback called when changing the account from the button
 *
 * \param button
 *
 * \return FALSE
 * */
static gboolean prefs_page_divers_scheduler_change_account (GtkWidget *combo)
{
	GrisbiWinEtat *w_etat;

	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();

	w_etat->scheduler_default_account_number = gsb_account_get_combo_account_number (combo);

    return FALSE;
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
static void prefs_page_divers_choose_language_changed (GtkComboBox *combo,
													   PrefsPageDivers *page)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (combo, &iter))
	{
		GtkTreeModel *model;
		gchar *string;
		gchar *tmp_str;
		gchar *hint;
		gint index;
		GrisbiAppConf *a_conf;

		a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
		gtk_tree_model_get (model, &iter, 0, &string, 1, &index, -1);
		if (index == 0)
		{
			a_conf->language_chosen = NULL;
		}
		else
		{
			a_conf->language_chosen = string;
		}

		tmp_str = g_strdup (_("You will have to restart Grisbi for the new language to take effect."));
		hint = g_strdup_printf (_("Changes the language of Grisbi for \"%s\"!"), string);
        dialogue_warning_hint (tmp_str , hint);
	}
}

/**
 *
 *
 * \param
 *
 * \return
 **/
static gint prefs_page_divers_choose_language_list_new (GtkWidget *combo,
														GrisbiAppConf *a_conf)
{
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	GSList *list = NULL;
	GSList *tmp_list;
	GDir *dir;
	const gchar *str_color;
	const gchar *dirname;
	const gchar *locale_dir;
	gint i = 0;
	gint activ_index = 0;

	locale_dir = gsb_dirs_get_locale_dir ();
	dir = g_dir_open (locale_dir, 0, NULL);
	if (!dir)
		return 0;

	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
	str_color = gsb_rgba_get_couleur_to_string ("text_gsetting_option_normal");

	while ((dirname = g_dir_read_name (dir)) != NULL)
    {
		gchar *filename;

		filename = g_build_filename (locale_dir,
									 dirname,
									 "LC_MESSAGES",
									 GETTEXT_PACKAGE ".mo",
									 NULL);
		if (g_file_test (filename, G_FILE_TEST_EXISTS))
		{
			list = g_slist_insert_sorted (list, g_strdup (dirname), (GCompareFunc) g_strcmp0);
		}

	  g_free (filename);
	}

	g_dir_close (dir);
	tmp_list = list;

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("System Language"), 1, i, 2, str_color, -1);

	i++;

	while (tmp_list)
	{
		if (g_strcmp0 (a_conf->language_chosen, tmp_list->data) == 0)
			activ_index = i;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, 0, (gchar *) tmp_list->data, 1, i, 2, str_color, -1);

        i++;
		tmp_list = tmp_list->next;
    }

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo),
									renderer,
									"text", 0,
									"foreground", 2,
									NULL);

	g_free ((gpointer)str_color);

	return activ_index;
}

/**
 *
 *
 * \param
 * \param
 * \param
 *
 * \return
 **/
static gboolean prefs_page_divers_radiobutton_first_last_press_event (GtkWidget *button,
																	  GdkEvent  *event,
																	  GrisbiAppConf *a_conf)
{
	//~ devel_debug (NULL);
	gint button_number;

	button_number = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "pointer"));
	if (button_number == 1)
		a_conf->select_scheduled_in_list = FALSE;
	else
		a_conf->select_scheduled_in_list = TRUE;

	return FALSE;
}
/**
 * Création de la page de gestion des divers
 *
 * \param prefs
 *
 * \return
 */
static void prefs_page_divers_setup_divers_page (PrefsPageDivers *page,
												 GrisbiPrefs *win)
{
	GtkWidget *head_page;
	GtkWidget *image;
    GtkWidget *vbox_button;
    GtkWidget *combo;
	gchar *config_file;
	gint combo_index;
	gboolean is_loading;
	GrisbiAppConf *a_conf;
	GrisbiWinEtat *w_etat;
	PrefsPageDiversPrivate *priv;

	devel_debug (NULL);

	priv = prefs_page_divers_get_instance_private (page);
	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();
	is_loading = grisbi_win_file_is_loading ();

	/* On récupère le nom de la page */
	head_page = utils_prefs_head_page_new_with_title_and_icon (_("Various settings"), "gsb-generalities-32.png");
	gtk_box_pack_start (GTK_BOX (priv->vbox_divers), head_page, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (priv->vbox_divers), head_page, 0);

	/* page generalities */
    /* set the variables for programs */
	gsb_automem_entry_new_from_ui (priv->entry_web_browser, &a_conf->browser_command, NULL, NULL);

	/* set help display */
	image = gtk_image_new_from_resource ("/org/gtk/grisbi/images/gsb-pdf.svg");
	gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
	gtk_container_add (GTK_CONTAINER (priv->hbox_display_pdf), image);
	gtk_box_reorder_child (GTK_BOX (priv->hbox_display_pdf), image, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->radiobutton_display_pdf), a_conf->display_help);
	g_object_set_data (G_OBJECT (priv->radiobutton_display_html), "pointer", GINT_TO_POINTER (1));
	g_object_set_data (G_OBJECT (priv->radiobutton_display_pdf), "pointer", GINT_TO_POINTER (2));

	/* set config file */
	config_file = g_strconcat (_("File: "), gsb_dirs_get_grisbirc_filename (), NULL);
	gtk_label_set_text	(GTK_LABEL (priv->label_prefs_settings), config_file);
	g_free (config_file);

	/* set signal button_reset_prefs_window */
	g_signal_connect (priv->button_reset_prefs_window,
					  "clicked",
					  G_CALLBACK (prefs_page_divers_button_reset_prefs_window_clicked),
					  win);

	/* set the scheduled variables */
	vbox_button = gsb_automem_radiobutton_gsettings_new (_("Warn/Execute the scheduled transactions arriving at expiration date"),
													_("Warn/Execute the scheduled transactions of the month"),
													&a_conf->execute_scheduled_of_month,
													(GCallback) prefs_page_divers_scheduler_warm_button_changed,
													page);

	gtk_box_pack_start (GTK_BOX (vbox_button), priv->hbox_launch_scheduler_nb_days_before_scheduled, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (vbox_button), priv->hbox_launch_scheduler_nb_days_before_scheduled, 1);
	gtk_box_pack_start (GTK_BOX (priv->vbox_launch_scheduler), vbox_button, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (priv->vbox_launch_scheduler), vbox_button, 0);

	/* initialise le bouton nombre de jours avant alerte execution */
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->spinbutton_nb_days_before_scheduled),
							   a_conf->nb_days_before_scheduled);

	/* Init checkbutton set fixed day */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_scheduler_set_fixed_day),
								  a_conf->scheduler_set_fixed_day);

	/* Init spinbutton_scheduler_fixed_day */
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->spinbutton_scheduler_fixed_day),
							   a_conf->scheduler_fixed_day);

	/* Init checkbutton set fixed date */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_scheduler_set_fixed_date),
								  w_etat->scheduler_set_fixed_date);
	gtk_widget_set_sensitive (priv->checkbutton_scheduler_set_fixed_date, is_loading);

	/* Adding and init widgets select defaut compte */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_scheduler_set_default_account),
								  w_etat->scheduler_set_default_account);

	if (is_loading)
		combo = gsb_account_create_combo_list (G_CALLBACK (prefs_page_divers_scheduler_change_account), NULL, FALSE);
	else
		combo = utils_prefs_create_combo_list_indisponible ();

	gtk_widget_set_size_request (combo, FORM_COURT_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (priv->hbox_launch_scheduler_set_default_account), combo, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (priv->checkbutton_scheduler_set_default_account), "widget", combo);

	if (w_etat->scheduler_set_default_account)
		gsb_account_set_combo_account_number (combo, w_etat->scheduler_default_account_number);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (combo), FALSE);

	gtk_widget_show (combo);

	/* sensitive widgets */
	if (a_conf->execute_scheduled_of_month)
	{
		gtk_widget_set_sensitive (GTK_WIDGET (priv->spinbutton_nb_days_before_scheduled), FALSE);
		gtk_widget_set_sensitive (priv->hbox_launch_scheduler_set_fixed_day, TRUE);
	}
	else
	{
		gtk_widget_set_sensitive (priv->hbox_launch_scheduler_set_fixed_day, FALSE);
	}

	if (!a_conf->scheduler_set_fixed_day)
		gtk_widget_set_sensitive (GTK_WIDGET (priv->spinbutton_scheduler_fixed_day), FALSE);

	/* select first or last scheduler transaction */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->radiobutton_select_last_scheduled),
													 a_conf->select_scheduled_in_list);
	g_object_set_data (G_OBJECT (priv->radiobutton_select_first_scheduled), "pointer", GINT_TO_POINTER (1));
	g_object_set_data (G_OBJECT (priv->radiobutton_select_last_scheduled), "pointer", GINT_TO_POINTER (2));


	/* Connect signal */
	/* callback select help display */
	g_signal_connect (G_OBJECT (priv->radiobutton_display_html),
					  "button-press-event",
					  G_CALLBACK (prefs_page_divers_radiobutton_help_press_event),
					  a_conf);
	g_signal_connect (G_OBJECT (priv->radiobutton_display_pdf),
					  "button-press-event",
					  G_CALLBACK (prefs_page_divers_radiobutton_help_press_event),
					  a_conf);

    /* callback for spinbutton_nb_days_before_scheduled */
    g_signal_connect (priv->spinbutton_nb_days_before_scheduled,
					  "value-changed",
					  G_CALLBACK (utils_prefs_spinbutton_changed),
					  &a_conf->nb_days_before_scheduled);

    /* callback for checkbutton_scheduler_set_fixed_day */
    g_signal_connect (priv->checkbutton_scheduler_set_fixed_day,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &a_conf->scheduler_set_fixed_day);

	g_signal_connect_after (G_OBJECT (priv->checkbutton_scheduler_set_fixed_day),
							"toggled",
							G_CALLBACK (prefs_page_divers_scheduler_set_fixed_day_changed),
							priv->spinbutton_scheduler_fixed_day);

    /* callback for spinbutton_scheduler_fixed_day */
    g_signal_connect (priv->spinbutton_scheduler_fixed_day,
					  "value-changed",
					  G_CALLBACK (utils_prefs_spinbutton_changed),
					  &a_conf->scheduler_fixed_day);

	/* callback for checkbutton_scheduler_set_fixed_date */
	g_signal_connect (priv->checkbutton_scheduler_set_fixed_date,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &w_etat->scheduler_set_fixed_date);

    /* callback for checkbutton_scheduler_set_default_account */
    g_signal_connect (priv->checkbutton_scheduler_set_default_account,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &w_etat->scheduler_set_default_account);

	/* set the localization parameters */
	/* set the language */
	combo_index = prefs_page_divers_choose_language_list_new (priv->combo_choose_language, a_conf);

	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_choose_language), combo_index);
	g_signal_connect (G_OBJECT (priv->combo_choose_language),
					  "changed",
					  G_CALLBACK (prefs_page_divers_choose_language_changed),
					  NULL);

	/* set callback to select first or last scheduler transaction */
	g_signal_connect (G_OBJECT (priv->radiobutton_select_first_scheduled),
					  "button-press-event",
					  G_CALLBACK (prefs_page_divers_radiobutton_first_last_press_event),
					  a_conf);
	g_signal_connect (G_OBJECT (priv->radiobutton_select_last_scheduled),
					  "button-press-event",
					  G_CALLBACK (prefs_page_divers_radiobutton_first_last_press_event),
					  a_conf);


	/* set the others parameters */
	prefs_page_divers_choose_date_format_init (page);
	prefs_page_divers_choose_number_format_init (page);

	if (is_loading == FALSE)
	{
		gtk_widget_set_sensitive (priv->hbox_launch_scheduler_set_default_account, FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (combo), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->box_choose_date_format), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->box_choose_numbers_format), FALSE);
	}
	else
	{
		GrisbiWinRun *w_run = NULL;

		w_run = (GrisbiWinRun *) grisbi_win_get_w_run ();
		if (w_run->prefs_divers_tab)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook_divers),  w_run->prefs_divers_tab);

		/* set signal notebook_css_rules */
		g_signal_connect (G_OBJECT (priv->notebook_divers),
						  "switch-page",
						  (GCallback) utils_prefs_page_notebook_switch_page,
						  &w_run->prefs_divers_tab);
	}
}

/******************************************************************************/
/* Fonctions propres à l'initialisation des fenêtres                          */
/******************************************************************************/
static void prefs_page_divers_init (PrefsPageDivers *page)
{
	gtk_widget_init_template (GTK_WIDGET (page));
}

static void prefs_page_divers_dispose (GObject *object)
{
	G_OBJECT_CLASS (prefs_page_divers_parent_class)->dispose (object);
}

static void prefs_page_divers_class_init (PrefsPageDiversClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = prefs_page_divers_dispose;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
												 "/org/gtk/grisbi/prefs/prefs_page_divers.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, vbox_divers);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, grid_generalities);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, notebook_divers);

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, button_reset_prefs_window);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, entry_web_browser);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, label_prefs_settings);

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, hbox_display_pdf);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_display_html);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_display_pdf);

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, vbox_launch_scheduler);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, hbox_launch_scheduler_set_default_account);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, checkbutton_scheduler_set_default_account);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, hbox_launch_scheduler_set_fixed_date);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, checkbutton_scheduler_set_fixed_date);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, hbox_launch_scheduler_set_fixed_day);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, checkbutton_scheduler_set_fixed_day);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, spinbutton_scheduler_fixed_day);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, hbox_launch_scheduler_nb_days_before_scheduled);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, spinbutton_nb_days_before_scheduled);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_select_first_scheduled);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_select_last_scheduled);

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, box_choose_date_format);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, combo_choose_language);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, box_choose_numbers_format);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, combo_choose_decimal_point);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, combo_choose_thousands_separator);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_choose_date_1);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_choose_date_2);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_choose_date_3);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageDivers, radiobutton_choose_date_4);
}

/******************************************************************************/
/* Public functions                                                           */
/******************************************************************************/
PrefsPageDivers *prefs_page_divers_new (GrisbiPrefs *win)
{
	PrefsPageDivers *page;

	page = g_object_new (PREFS_PAGE_DIVERS_TYPE, NULL);
	prefs_page_divers_setup_divers_page (page, win);

	return page;
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

