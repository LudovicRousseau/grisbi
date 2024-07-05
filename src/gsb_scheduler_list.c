/* ************************************************************************** */
/*                                                                            */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)            */
/*          2004-2008 Benjamin Drieu (bdrieu@april.org)                       */
/*      2009 Thomas Peel (thomas.peel@live.fr)                                */
/*          2008-2021 Pierre Biava (grisbi@pierre.biava.name)                 */
/*          https://www.grisbi.org/                                           */
/*                                                                            */
/*  This program is free software; you can redistribute it and/or modify      */
/*  it under the terms of the GNU General Public License as published by      */
/*  the Free Software Foundation; either version 2 of the License, or         */
/*  (at your option) any later version.                                       */
/*                                                                            */
/*  This program is distributed in the hope that it will be useful,           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/*  GNU General Public License for more details.                              */
/*                                                                            */
/*  You should have received a copy of the GNU General Public License         */
/*  along with this program; if not, write to the Free Software               */
/*  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/*                                                                            */
/* ************************************************************************** */

/* \file gsb_scheduler_list.c : functions for the scheduled list */



#include "config.h"

#include "include.h"
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <stdbool.h>

/*START_INCLUDE*/
#include "gsb_scheduler_list.h"
#include "bet_data_finance.h"
#include "bet_finance_ui.h"
#include "dialog.h"
#include "grisbi_app.h"
#include "gsb_automem.h"
#include "gsb_calendar.h"
#include "gsb_data_account.h"
#include "gsb_data_category.h"
#include "gsb_data_currency.h"
#include "gsb_data_payee.h"
#include "gsb_data_scheduled.h"
#include "gsb_dirs.h"
#include "gsb_file.h"
#include "gsb_form.h"
#include "gsb_form_scheduler.h"
#include "gsb_real.h"
#include "gsb_rgba.h"
#include "gsb_scheduler.h"
#include "gsb_transactions_list.h"
#include "menu.h"
#include "mouse.h"
#include "navigation.h"
#include "structures.h"
#include "traitement_variables.h"
#include "utils.h"
#include "utils_dates.h"
#include "utils_real.h"
#include "utils_str.h"
#include "erreur.h"
/*END_INCLUDE*/

/*START_GLOBAL*/
/* lists of number of scheduled transactions taken or to be taken */
GSList *				scheduled_transactions_to_take;
GSList *				scheduled_transactions_taken;
/*END_GLOBAL*/

/*START_EXTERN*/
/*END_EXTERN*/

/*START_STATIC*/
/* the total of % of scheduled columns can be > 100 because all the columns are not showed at the same time */
static gint scheduler_col_width_init[SCHEDULER_COL_VISIBLE_COLUMNS] = {10, 12, 36, 12, 12, 24, 12};

/* used to save and restore the width of the scheduled list */
static gint					scheduler_col_width[SCHEDULER_COL_VISIBLE_COLUMNS];
static gint					scheduler_current_tree_view_width = 0;

/* set the tree view and models as static, we can access to them
 * by the functions gsb_scheduler_list_get_tree_view...
 * don't call them directly */
static GtkWidget *			tree_view_scheduler_list;
static GtkTreeModel *		tree_model_scheduler_list;
static GtkTreeModelSort *	tree_model_sort_scheduler_list;
static GtkSortType			sort_type;
static GtkTreeViewColumn *	scheduler_list_column[SCHEDULER_COL_VISIBLE_COLUMNS];
static gint					last_scheduled_number;

/* première ope planifiee de la liste */
static gint					first_scheduler_list_number = -1;

/* toolbar */
static GtkWidget *			scheduler_toolbar;

/* Used to display/hide comments in scheduler list */
static GtkWidget *			scheduler_display_hide_notes = NULL;

/* here are the 3 buttons on the scheduler toolbar
 * which can been unsensitive or sensitive */
static GtkWidget *			scheduler_button_execute = NULL;
static GtkWidget *			scheduler_button_delete = NULL;
static GtkWidget *			scheduler_button_edit = NULL;

/* popup view menu */
static const gchar *		periodicity_names[] = {N_("Unique view"), N_("Week view"), N_("Month view"),
												   N_("Two months view"), N_("Quarter view"),
												   N_("Year view"), N_("Custom view"), NULL,};

static gboolean				view_menu_block_cb = FALSE;

static const gchar *		j_m_a_names[] = {N_("days"), N_("weeks"), N_("months"), N_("years"), NULL};
/*END_STATIC*/

/******************************************************************************/
/* Private functions                                                          */
/******************************************************************************/
/**
 * called when the user choose a custom periodicity on the toolbar
 *
 * \param
 *
 * \return TRUE : did his choice, FALSE : cancel the choice
 **/
static gboolean gsb_scheduler_list_popup_custom_periodicity_dialog (void)
{
    GtkWidget *combobox;
	GtkWidget *button_cancel;
	GtkWidget *button_apply;
    GtkWidget *dialog;
    GtkWidget *entry;
    GtkWidget *hbox;
    GtkWidget *hbox2;
    GtkWidget *label;
    GtkWidget *paddingbox;
    int i;
	GrisbiWinEtat *w_etat;

	w_etat = grisbi_win_get_w_etat ();
    dialog = gtk_dialog_new_with_buttons (_("Show scheduled transactions"),
										  GTK_WINDOW (grisbi_app_get_active_window (NULL)),
										  GTK_DIALOG_MODAL,
										  NULL, NULL,
										  NULL);

	button_cancel = gtk_button_new_with_label (_("Cancel"));
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_cancel, GTK_RESPONSE_CANCEL);
	gtk_widget_set_can_default (button_cancel, TRUE);

	button_apply = gtk_button_new_with_label (_("Apply"));
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_apply, GTK_RESPONSE_APPLY);
	gtk_widget_set_can_default (button_apply, TRUE);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    /* Ugly dance to avoid side effects on dialog's vbox. */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (dialog_get_content_area (dialog)), hbox, FALSE, FALSE, 0);
	paddingbox = new_paddingbox_with_title (hbox, TRUE, _("Scheduler frequency"));
    gtk_container_set_border_width (GTK_CONTAINER (hbox), MARGIN_BOX);
    gtk_container_set_border_width (GTK_CONTAINER (paddingbox), MARGIN_BOX);

    hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (paddingbox), hbox2, FALSE, FALSE, 0);

    label = gtk_label_new (_("Show transactions for the next: "));
    gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);
    entry = gsb_automem_spin_button_new (&w_etat->affichage_echeances_perso_nb_libre, NULL, NULL);
    gtk_box_pack_start (GTK_BOX (hbox2), entry, FALSE, FALSE, MARGIN_BOX);

    /* combobox for userdefined frequency */
    combobox = gtk_combo_box_text_new ();
    gtk_box_pack_start (GTK_BOX (hbox2), combobox, FALSE, FALSE, 0);

    for (i = 0; j_m_a_names[i]; i++)
    {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combobox), g_dgettext (NULL, j_m_a_names[i]));
    }
    gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), w_etat->affichage_echeances_perso_j_m_a);

    gtk_widget_show_all (dialog);

    switch (gtk_dialog_run (GTK_DIALOG (dialog)))
    {
		case GTK_RESPONSE_APPLY:
			w_etat->affichage_echeances_perso_j_m_a = gtk_combo_box_get_active (GTK_COMBO_BOX (combobox));
			w_etat->affichage_echeances_perso_nb_libre = utils_str_atoi (gtk_entry_get_text (GTK_ENTRY(entry)));
			gtk_widget_destroy (dialog);
			return TRUE;
    }

    gtk_widget_destroy (dialog);
    return FALSE;
}

/**
 * called from the toolbar to change the scheduler view
 *
 * \param periodicity 	the new view wanted
 * \param item		not used
 *
 * \return FALSE
 **/
static void gsb_scheduler_list_change_scheduler_view (GtkWidget *item,
													  gpointer pointer_periodicity)
{
    gchar *tmp_str;
    gint periodicity;
	GrisbiWinEtat *w_etat;

	w_etat = grisbi_win_get_w_etat ();
    periodicity = GPOINTER_TO_INT (pointer_periodicity);
    if (view_menu_block_cb || periodicity == w_etat->affichage_echeances)
        return;

    if (periodicity == SCHEDULER_PERIODICITY_CUSTOM_VIEW)
    {
        if (!gsb_scheduler_list_popup_custom_periodicity_dialog ())
            return;
    }

    tmp_str = g_strconcat (_("Scheduled transactions"), " : ",
						   g_dgettext (NULL, periodicity_names[periodicity]),
						   NULL);
    grisbi_win_headings_update_title (tmp_str);
    grisbi_win_headings_update_suffix ("");
    g_free (tmp_str);

    w_etat->affichage_echeances = periodicity;
    gsb_scheduler_list_fill_list ();
    gsb_scheduler_list_set_background_color (gsb_scheduler_list_get_tree_view ());
    gsb_scheduler_list_select (-1);

    gsb_file_set_modified (TRUE);

    return;
}

/**
 * Called to edit a specific transaction but the number of transaction
 * is passed via a pointer (by g_signal_connect)
 *
 * \param scheduled_number a pointer which is the number of the transaction
 *
 * \return FALSE
 **/
static gboolean gsb_scheduler_list_edit_transaction_by_pointer (gint *scheduled_number)
{
	gint number;

	number = GPOINTER_TO_INT (scheduled_number);
	devel_debug_int (number);

	if (number == -1)
		gsb_scheduler_list_select (-1);
	gsb_scheduler_list_edit_transaction (number);

	return FALSE;
}

/**
 * change the showed information on the list :
 * either show the frequency and mode of the scheduled
 * either show the notes
 *
 * \param
 *
 * \return FALSE
 */
static gboolean gsb_scheduler_list_show_notes (GtkWidget *item)
{
	GrisbiWinEtat *w_etat;

	w_etat = grisbi_win_get_w_etat ();
	if (scheduler_display_hide_notes)
	{
		GrisbiAppConf *a_conf;

		a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
		if (a_conf->display_toolbar != GTK_TOOLBAR_ICONS)
        {
            if (w_etat->affichage_commentaire_echeancier)
                gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Frequency/Mode"));
            else
                gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), _("Notes"));
        }

        if (w_etat->affichage_commentaire_echeancier)
            gtk_widget_set_tooltip_text (GTK_WIDGET (item),
										 _("Display the frequency and mode of scheduled transactions"));
        else
            gtk_widget_set_tooltip_text (GTK_WIDGET (item),
										 _("Display the notes of scheduled transactions"));
	}

    gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN (scheduler_list_column[COL_NB_FREQUENCY]),
									  !w_etat->affichage_commentaire_echeancier);
    gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN (scheduler_list_column[COL_NB_MODE]),
									  !w_etat->affichage_commentaire_echeancier);
    gtk_tree_view_column_set_visible (GTK_TREE_VIEW_COLUMN (scheduler_list_column[COL_NB_NOTES]),
									  w_etat->affichage_commentaire_echeancier);

    w_etat->affichage_commentaire_echeancier = !w_etat->affichage_commentaire_echeancier;

    return FALSE;
}

/**
 * Pop up a menu with several actions to apply to current scheduled.
 *
 * \param
 *
 * \return
 **/
static void gsb_scheduler_list_popup_scheduled_context_menu (GtkWidget *tree_view,
															 GtkTreePath *path)
{
    GtkWidget *menu;
    GtkWidget *menu_item;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint scheduled_number;
	gint virtual_transaction;
	GrisbiWinEtat *w_etat;

	w_etat = grisbi_win_get_w_etat ();
	menu = gtk_menu_new ();

	/* Récupération des données de la ligne sélectionnée */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get (model,
						&iter,
						SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_transaction,
						SCHEDULER_COL_NB_TRANSACTION_NUMBER, &scheduled_number,
						-1);

	if (virtual_transaction)
		scheduled_number = 0;

    /* Edit transaction */
	menu_item = GTK_WIDGET (utils_menu_item_new_from_image_label ("gtk-edit-16.png", _("Edit transaction")));
    g_signal_connect_swapped (G_OBJECT (menu_item),
							  "activate",
                        	  G_CALLBACK (gsb_scheduler_list_edit_transaction_by_pointer),
                        	  GINT_TO_POINTER (scheduled_number));
    if (scheduled_number <= 0)
        gtk_widget_set_sensitive (menu_item, FALSE);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    /* Clone transaction */
    menu_item = GTK_WIDGET (utils_menu_item_new_from_image_label ("gtk-copy-16.png", _("Clone transaction")));
    g_signal_connect (G_OBJECT (menu_item),
                       "activate",
                       G_CALLBACK (gsb_scheduler_list_clone_selected_scheduled),
                       GINT_TO_POINTER (scheduled_number));
    if (scheduled_number <= 0)
        gtk_widget_set_sensitive (menu_item, FALSE);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    /* Separator */
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

    /* New transaction */
    menu_item = GTK_WIDGET (utils_menu_item_new_from_image_label  ("gsb-new-transaction-16.png",
																   _("New transaction")));
    g_signal_connect_swapped (G_OBJECT (menu_item),
                        	  "activate",
                        	  G_CALLBACK (gsb_scheduler_list_edit_transaction_by_pointer),
                        	  GINT_TO_POINTER (-1));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    /* Delete transaction */
    menu_item = GTK_WIDGET (utils_menu_item_new_from_image_label ("gtk-delete-16.png", _("Delete transaction")));
    g_signal_connect (G_OBJECT (menu_item),
                       "activate",
                       G_CALLBACK (gsb_scheduler_list_delete_scheduled_transaction_by_menu),
                       NULL);

    if (scheduled_number <= 0)
        gtk_widget_set_sensitive (menu_item, FALSE);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    /* Separator */
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

    /* Display/hide notes */
    if (w_etat->affichage_commentaire_echeancier)
        menu_item = GTK_WIDGET (utils_menu_item_new_from_image_label ("gsb-comments-16.png", _("Displays notes")));
    else
        menu_item = GTK_WIDGET (utils_menu_item_new_from_image_label ("gsb-comments-16.png",
																	  _("Displays Frequency/Mode")));

    g_signal_connect_swapped (G_OBJECT (menu_item),
                        	  "activate",
                        	  G_CALLBACK (gsb_scheduler_list_show_notes),
                        	  scheduler_display_hide_notes);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    /* Separator */
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

    /* Execute transaction */
    menu_item = utils_menu_item_new_from_image_label ("gtk-execute-16.png",_("Execute transaction"));
    g_signal_connect_swapped (G_OBJECT (menu_item),
                        	  "activate",
                        	  G_CALLBACK (gsb_scheduler_list_execute_transaction),
                        	  NULL);
    if (scheduled_number <= 0)
        gtk_widget_set_sensitive (menu_item, FALSE);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    /* Finish all. */
    gtk_widget_show_all (menu);

	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

/**
 * called when the selection of the list change
 *
 * \param selection
 * \param null not used
 *
 * \return FALSE
 **/
static void gsb_scheduler_list_select_line (GtkWidget *tree_view,
											GtkTreePath *path)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
	gint virtual_transaction;
    gint tmp_number = 0;
    gint account_number;
	GrisbiAppConf *a_conf;

	devel_debug (NULL);
	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();

	/* Récupération des données de la ligne sélectionnée */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get (model,
						&iter,
						SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_transaction,
						SCHEDULER_COL_NB_TRANSACTION_NUMBER, &tmp_number,
						-1);

	if (virtual_transaction)
		tmp_number = 0;

	/* protect last_scheduled_number because when refill the list, set selection to 0 and so last_scheduled_number... */
	last_scheduled_number = tmp_number;

    /* if a_conf->show_transaction_selected_in_form => edit the scheduled transaction */
    if (tmp_number != 0 && a_conf->show_transaction_selected_in_form)
            gsb_scheduler_list_edit_transaction (tmp_number);
    else if (tmp_number == 0)
    {
        gsb_form_scheduler_clean ();
        account_number = gsb_data_scheduled_get_account_number (tmp_number);
        gsb_form_clean (account_number);
    }

    /* sensitive/unsensitive the button execute */
    gtk_widget_set_sensitive (scheduler_button_execute,
							  (tmp_number > 0)
							  && !gsb_data_scheduled_get_mother_scheduled_number (tmp_number));

    /* sensitive/unsensitive the button edit */
    gtk_widget_set_sensitive (scheduler_button_edit, (tmp_number > 0));

    /* sensitive/unsensitive the button delete */
    gtk_widget_set_sensitive (scheduler_button_delete, (tmp_number > 0));

    gsb_menu_set_menus_select_scheduled_sensitive (tmp_number > 0);
}

/**
 * called when we press a button on the list
 *
 * \param tree_view
 * \param ev
 *
 * \return FALSE
 **/
static gboolean gsb_scheduler_list_button_press (GtkWidget *tree_view,
												 GdkEventButton *ev)
{
	if (ev->button == RIGHT_BUTTON)
	{
        GtkTreePath *path = NULL;

		/* show the popup */
        if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view ), ev->x, ev->y, &path, NULL, NULL, NULL))
        {
            gsb_scheduler_list_popup_scheduled_context_menu (tree_view, path);
            gtk_tree_path_free (path);
        }
	}

    else if (ev->type == GDK_2BUTTON_PRESS)
    {
         /* if double-click => edit the scheduled transaction */
        gint current_scheduled_number;

        current_scheduled_number = gsb_scheduler_list_get_current_scheduled_number ();

        if (current_scheduled_number)
            gsb_scheduler_list_edit_transaction (current_scheduled_number);
    }
	else if (ev->button == LEFT_BUTTON)
	{
        GtkTreePath *path = NULL;

		/* select line */
        if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view ), ev->x, ev->y, &path, NULL, NULL, NULL))
        {
            gsb_scheduler_list_select_line (tree_view, path);
            gtk_tree_path_free (path);
        }
	}

    return FALSE;
}

/**
 * affichage du popup view menu
 *
 * \param button
 * \param event
 * \param user data
 *
 * \return
 */
static gboolean gsb_scheduler_list_popup_scheduler_view (GtkWidget *button,
														 GdkEvent  *event,
														 gpointer   user_data)
{
    GtkWidget *menu, *item;
    GSList *group = NULL;
    gint nbre_echeances;
    gint i;
	GrisbiWinEtat *w_etat;

	w_etat = grisbi_win_get_w_etat ();
    view_menu_block_cb = TRUE;
    menu = gtk_menu_new ();

    /* on enlève la ligne blanche */
    nbre_echeances = gtk_tree_model_iter_n_children (tree_model_scheduler_list, NULL) - 1;

    for (i = 0 ; periodicity_names[i] ; i++)
    {
        gchar *tmp_str;

        if (i == w_etat->affichage_echeances)
        {
            if (i == SCHEDULER_PERIODICITY_CUSTOM_VIEW)
            {
                tmp_str = g_strdup_printf ("%s (%d - %d %s)",
										   g_dgettext (NULL, periodicity_names[i]),
                                		   nbre_echeances,
                                		   w_etat->affichage_echeances_perso_nb_libre,
                                		   g_dgettext (NULL,
                                		   j_m_a_names[w_etat->affichage_echeances_perso_j_m_a]));
            }
            else
            {
                tmp_str = g_strdup_printf ("%s (%d)",
										   g_dgettext (NULL, periodicity_names[i]),
										   nbre_echeances);
            }
            item = gtk_radio_menu_item_new_with_label (group, tmp_str);

            g_free (tmp_str);
        }
        else
            item = gtk_radio_menu_item_new_with_label (group, g_dgettext (NULL, periodicity_names[i]));

        group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
        if (i == w_etat->affichage_echeances)
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);

        g_signal_connect (G_OBJECT (item),
						  "toggled",
						  G_CALLBACK (gsb_scheduler_list_change_scheduler_view),
						  GINT_TO_POINTER (i));

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    gtk_widget_show_all (menu);
	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
    view_menu_block_cb = FALSE;

    return TRUE;
}

/**
 * default sorting function for scheduler list :
 * sort by date, the column number 0
 * perhaps later sort by different columns ??
 *
 * \param
 * \param
 * \param
 * \param
 *
 * \return the result of comparison
 **/
static gint gsb_scheduler_list_default_sort_function (GtkTreeModel *model,
													  GtkTreeIter *iter_1,
													  GtkTreeIter *iter_2,
													  gpointer null)
{
    GDate *date_1;
    GDate *date_2;
    gchar *date_str;
    gint number_1;
    gint number_2;
    gint return_value = 0;

    /* first, we sort by date (col 0) */
    gtk_tree_model_get (model,
                        iter_1,
                        COL_NB_DATE, &date_str,
                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, &number_1,
                        -1);
    date_1 = gsb_parse_date_string (date_str);
    g_free (date_str);

    gtk_tree_model_get (model,
                        iter_2,
                        COL_NB_DATE, &date_str,
                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, &number_2,
                        -1);
    date_2 = gsb_parse_date_string (date_str);
    g_free (date_str);

    if (number_1 == -1)
    {
        if (date_1) g_date_free (date_1);
        if (date_2) g_date_free (date_2);
        if (sort_type == GTK_SORT_ASCENDING)
            return 1;
        else
            return -1;
    }
    else if (number_2 == -1)
    {
        if (date_1) g_date_free (date_1);
        if (date_2) g_date_free (date_2);
        if (sort_type == GTK_SORT_ASCENDING)
            return -1;
        else
            return 1;
    }

    if (date_1 &&  date_2)
        return_value = g_date_compare (date_1, date_2);

    if (return_value)
    {
        if (date_1) g_date_free (date_1);
        if (date_2) g_date_free (date_2);

        return return_value;
    }
    /* if we are here it's because we are in a child of split */
    if (number_1 < 0)
    {
        if (sort_type == GTK_SORT_ASCENDING)
            return 1;
        else
            return -1;
    }
    if (number_2 < 0)
    {
        if (sort_type == GTK_SORT_ASCENDING)
            return -1;
        else
            return 1;
    }

    if (!return_value)
        return_value = number_1 - number_2;

    if (date_1) g_date_free (date_1);
    if (date_2) g_date_free (date_2);

    return return_value;
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
static gboolean gsb_scheduler_list_sort_column_clicked (GtkTreeViewColumn *tree_view_column,
														gint *column_ptr)
{
    gint current_column;
    gint new_column;

    gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (tree_model_sort_scheduler_list),
										  &current_column,
										  &sort_type);

    new_column = GPOINTER_TO_INT (column_ptr);

    /* if the new column is the same as the old one, we change the sort type */
    if (new_column == current_column)
    {
        if (sort_type == GTK_SORT_ASCENDING)
            sort_type = GTK_SORT_DESCENDING;
        else
            sort_type = GTK_SORT_ASCENDING;
    }
    else
	/* we sort by another column, so sort type by default is descending */
	sort_type = GTK_SORT_ASCENDING;

    gtk_tree_view_column_set_sort_indicator (scheduler_list_column[current_column], FALSE);
    gtk_tree_view_column_set_sort_indicator (scheduler_list_column[new_column], TRUE);

    gtk_tree_view_column_set_sort_order (scheduler_list_column[new_column], sort_type);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (tree_model_sort_scheduler_list),
										  new_column,
										  sort_type);

    gsb_scheduler_list_set_background_color (gsb_scheduler_list_get_tree_view ());

    if (last_scheduled_number > 0)
        gsb_scheduler_list_select (last_scheduled_number);
    else
        gsb_scheduler_list_select (-1);

    return FALSE;
}

/**
 *
 *
 * \param
 * \param
 * \param
 * \param
 *
 * \return
 **/
static gint gsb_scheduler_list_sort_function_by_account (GtkTreeModel *model,
														 GtkTreeIter *iter_1,
                        								 GtkTreeIter *iter_2,
                        								 gint *column_ptr)
{
    gchar *str_1;
    gchar *str_2;
    gint number_1;
    gint number_2;
    gint virtual_op_1 = 0;
    gint virtual_op_2 = 0;
    gint return_value = 0;

    /* first, we sort by account (col 0) */
    gtk_tree_model_get (model,
                        iter_1,
                        COL_NB_ACCOUNT, &str_1,
                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, &number_1,
                        SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_op_1,
                        -1);

    gtk_tree_model_get (model,
                        iter_2,
                        COL_NB_ACCOUNT, &str_2,
                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, &number_2,
                        SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_op_2,
                        -1);

    if (number_1 == -1)
    {
        if (sort_type == GTK_SORT_ASCENDING)
            return 1;
        else
            return -1;
    }
    else if (number_2 == -1)
    {
        if (sort_type == GTK_SORT_ASCENDING)
            return -1;
        else
            return 1;
    }

    if (sort_type == GTK_SORT_ASCENDING)
        return_value = virtual_op_1 - virtual_op_2;
    else
        return_value = virtual_op_2 - virtual_op_1;

    if (return_value)
        return return_value;

    if (str_1)
    {
        if (str_2)
            return_value = g_utf8_collate (str_1, str_2);
        else
        {
            g_free (str_1);
            return -1;
        }
    }
    else if (str_2)
    {
        g_free (str_2);
        if (sort_type == GTK_SORT_ASCENDING)
            return 1;
        else
            return -1;
    }

    if (return_value == 0)
        return_value = number_1 - number_2;

    g_free (str_1);
    g_free (str_2);

    return return_value;
}

/**
 *
 *
 * \param
 * \param
 * \param
 * \param
 *
 * \return
 **/
static gint gsb_scheduler_list_sort_function_by_payee (GtkTreeModel *model,
													   GtkTreeIter *iter_1,
                        							   GtkTreeIter *iter_2,
                        							   gint *column_ptr)
{
    gchar *str_1;
    gchar *str_2;
    gint number_1;
    gint number_2;
    gint virtual_op_1 = 0;
    gint virtual_op_2 = 0;
    gint return_value = 0;

    /* first, we sort by payee (col 0) */
    gtk_tree_model_get (model,
                        iter_1,
                        COL_NB_PARTY, &str_1,
                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, &number_1,
                        SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_op_1,
                        -1);

    gtk_tree_model_get (model,
                        iter_2,
                        COL_NB_PARTY, &str_2,
                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, &number_2,
                        SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_op_2,
                        -1);

    if (number_1 == -1)
    {
        if (sort_type == GTK_SORT_ASCENDING)
            return 1;
        else
            return -1;
    }
    else if (number_2 == -1)
    {
        if (sort_type == GTK_SORT_ASCENDING)
            return -1;
        else
            return 1;
    }

    if (sort_type == GTK_SORT_ASCENDING)
        return_value = virtual_op_1 - virtual_op_2;
    else
        return_value = virtual_op_2 - virtual_op_1;

    if (return_value)
        return return_value;

    if (str_1)
    {
        if (str_2)
            return_value = g_utf8_collate (str_1, str_2);
        else
        {
            g_free (str_1);
            return -1;
        }
    }
    else if (str_2)
    {
        g_free (str_2);
        if (sort_type == GTK_SORT_ASCENDING)
            return 1;
        else
            return -1;
    }

    if (return_value == 0)
        return_value = number_1 - number_2;

    g_free (str_1);
    g_free (str_2);

    return return_value;
}

/**
 * create and append the columns of the tree_view
 *
 * \param tree_view the tree_view
 *
 * \return
 **/
static void gsb_scheduler_list_create_list_columns (GtkWidget *tree_view)
{
    gint i;
    const gchar *scheduler_titles[] = {N_("Date"), N_("Account"), N_("Payee"),
									   N_("Frequency"), N_("Mode"), N_("Notes"), N_("Amount")};
    gfloat col_justs[] =
	{
		COLUMN_CENTER, COLUMN_LEFT, COLUMN_LEFT, COLUMN_CENTER,
		COLUMN_CENTER, COLUMN_LEFT, COLUMN_RIGHT, COLUMN_RIGHT
    };

    for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS ; i++)
    {
        GtkCellRenderer *cell_renderer;

        cell_renderer = gtk_cell_renderer_text_new ();

        gtk_cell_renderer_set_alignment (GTK_CELL_RENDERER (cell_renderer), col_justs[i], 0);
        switch (i)
        {
            case 0:
                scheduler_list_column[i] = gtk_tree_view_column_new_with_attributes (gettext (scheduler_titles[i]),
																					 cell_renderer,
                                            										 "text", i,
                                            										 "cell-background-rgba",
																					 SCHEDULER_COL_NB_BACKGROUND,
                                            										 "font-desc",
																					 SCHEDULER_COL_NB_FONT,
                                            										 "foreground",
																					 SCHEDULER_COL_NB_TEXT_COLOR,
									      										 	 NULL);
                gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (tree_model_sort_scheduler_list),
												 i,
                                            	 (GtkTreeIterCompareFunc) gsb_scheduler_list_default_sort_function,
                                            	 NULL,
                                            	 NULL);
                gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (scheduler_list_column[i]), TRUE);

                /* use the click to sort the list */
                g_signal_connect (G_OBJECT (scheduler_list_column[i]),
								  "clicked",
								  G_CALLBACK (gsb_scheduler_list_sort_column_clicked),
								  GINT_TO_POINTER (i));
                break;
            case 1:
                scheduler_list_column[i] = gtk_tree_view_column_new_with_attributes (gettext (scheduler_titles[i]),
                                            										 cell_renderer,
                                            										 "text", i,
                                            										 "cell-background-rgba",
																					 SCHEDULER_COL_NB_BACKGROUND,
                                            										 "font-desc",
																					 SCHEDULER_COL_NB_FONT,
                                            										 "foreground",
																					 SCHEDULER_COL_NB_TEXT_COLOR,
                                            										 NULL);
                gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (tree_model_sort_scheduler_list),
                                            	 i,
                                            	 (GtkTreeIterCompareFunc) gsb_scheduler_list_sort_function_by_account,
                                            	 NULL,
                                            	 NULL);
                gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (scheduler_list_column[i]), TRUE);

                /* use the click to sort the list */
                g_signal_connect (G_OBJECT (scheduler_list_column[i]),
								  "clicked",
								  G_CALLBACK (gsb_scheduler_list_sort_column_clicked),
								  GINT_TO_POINTER (i));
                break;
            case 2:
                scheduler_list_column[i] = gtk_tree_view_column_new_with_attributes (gettext (scheduler_titles[i]),
                                            										 cell_renderer,
                                            										 "text", i,
                                            										 "cell-background-rgba",
																					 SCHEDULER_COL_NB_BACKGROUND,
                                            										 "font-desc",
																					 SCHEDULER_COL_NB_FONT,
                                            										 "foreground",
																					 SCHEDULER_COL_NB_TEXT_COLOR,
                                            										 NULL);
                gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (tree_model_sort_scheduler_list),
                                            	 i,
                                            	 (GtkTreeIterCompareFunc) gsb_scheduler_list_sort_function_by_payee,
                                            	 NULL,
                                            	 NULL);
                gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (scheduler_list_column[i]), TRUE);

                /* use the click to sort the list */
                g_signal_connect (G_OBJECT (scheduler_list_column[i]),
								  "clicked",
								  G_CALLBACK (gsb_scheduler_list_sort_column_clicked),
								  GINT_TO_POINTER (i));
                break;
            case 3:
                scheduler_list_column[i] = gtk_tree_view_column_new_with_attributes (gettext (scheduler_titles[i]),
                                            										 cell_renderer,
                                            										 "text", i,
                                            										 "cell-background-rgba",
																					 SCHEDULER_COL_NB_BACKGROUND,
                                            										 "font-desc",
																					 SCHEDULER_COL_NB_FONT,
                                            										 "foreground",
																					 SCHEDULER_COL_NB_TEXT_COLOR,
                                            										 NULL);
                break;
            case 4:
                scheduler_list_column[i] = gtk_tree_view_column_new_with_attributes (gettext (scheduler_titles[i]),
                                            										 cell_renderer,
                                            										 "text", i,
                                            										 "cell-background-rgba",
																					 SCHEDULER_COL_NB_BACKGROUND,
                                            										 "font-desc",
																					 SCHEDULER_COL_NB_FONT,
                                            										 "foreground",
																					 SCHEDULER_COL_NB_TEXT_COLOR,
                                            										 NULL);
                break;
            case 5:
                scheduler_list_column[i] = gtk_tree_view_column_new_with_attributes (gettext (scheduler_titles[i]),
                                            										 cell_renderer,
                                            										 "text", i,
                                            										 "cell-background-rgba",
																					 SCHEDULER_COL_NB_BACKGROUND,
                                            										 "font-desc",
																					 SCHEDULER_COL_NB_FONT,
                                            										 "foreground",
																					 SCHEDULER_COL_NB_TEXT_COLOR,
                                            										 NULL);
                break;
            case 6:
                scheduler_list_column[i] = gtk_tree_view_column_new_with_attributes (gettext (scheduler_titles[i]),
                                            										 cell_renderer,
                                            										 "text", i,
                                            										 "cell-background-rgba",
																					 SCHEDULER_COL_NB_BACKGROUND,
                                            										 "foreground",
																					 SCHEDULER_COL_NB_AMOUNT_COLOR,
                                            										 "font-desc",
																					 SCHEDULER_COL_NB_FONT,
                                            										 NULL);
				gtk_cell_renderer_set_padding (GTK_CELL_RENDERER (cell_renderer), MARGIN_BOX, 0);
                break;
        }

        gtk_tree_view_column_set_alignment (GTK_TREE_VIEW_COLUMN (scheduler_list_column[i]), col_justs[i]);

        gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
                          GTK_TREE_VIEW_COLUMN (scheduler_list_column[i]));

        /* automatic and resizeable sizing */
        gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (scheduler_list_column[i]),
                          GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_resizable (GTK_TREE_VIEW_COLUMN (scheduler_list_column[i]), TRUE);
    }
}

/**
 * set the scheduler tree_model
 *
 * \param model
 *
 * \return
 **/
static void gsb_scheduler_list_set_model (GtkTreeModel *model)
{
    tree_model_scheduler_list = model;
}

/**
 * set the scheduler tree  model sort
 *
 * \param tree_model_sort
 *
 * \return
 **/
static void gsb_scheduler_list_set_sorted_model (GtkTreeModelSort *tree_model_sort)
{
    tree_model_sort_scheduler_list = tree_model_sort;
}

/**
 * create and return the tree store of the scheduler list
 *
 * \param
 *
 * \return a gtk_tree_store
 **/
static GtkTreeModel *gsb_scheduler_list_create_model (void)
{
    GtkTreeStore *store;
    GtkTreeModel *sortable;

    store = gtk_tree_store_new (SCHEDULER_COL_NB_TOTAL,
								G_TYPE_STRING,					/* COL_NB_DATE */
				 				G_TYPE_STRING,					/* COL_NB_ACCOUNT */
				 				G_TYPE_STRING,					/* COL_NB_PARTY */
				 				G_TYPE_STRING,					/* COL_NB_FREQUENCY */
				 				G_TYPE_STRING,					/* COL_NB_MODE */
				 				G_TYPE_STRING,					/* COL_NB_NOTES */
				 				G_TYPE_STRING,					/* COL_NB_AMOUNT */
				 				GDK_TYPE_RGBA,					/* SCHEDULER_COL_NB_BACKGROUND */
				 				GDK_TYPE_RGBA,					/* SCHEDULER_COL_NB_SAVE_BACKGROUND */
				 				G_TYPE_STRING,					/* SCHEDULER_COL_NB_AMOUNT_COLOR */
				 				G_TYPE_INT,						/* SCHEDULER_COL_NB_TRANSACTION_NUMBER */
				 				PANGO_TYPE_FONT_DESCRIPTION,	/* SCHEDULER_COL_NB_FONT */
				 				G_TYPE_INT,						/* SCHEDULER_COL_NB_VIRTUAL_TRANSACTION */
				 				G_TYPE_STRING);					/* SCHEDULER_COL_NB_TEXT_COLOR */

    gsb_scheduler_list_set_model (GTK_TREE_MODEL (store));
    sortable = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
    gsb_scheduler_list_set_sorted_model (GTK_TREE_MODEL_SORT (sortable));
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (tree_model_sort_scheduler_list), 0, GTK_SORT_ASCENDING);
    gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sortable),
											 (GtkTreeIterCompareFunc) gsb_scheduler_list_default_sort_function,
											 NULL,
											 NULL);

    return sortable;
}

/**
 * Create the toolbar that contains all elements needed to manipulate
 * the scheduler.
 *
 * \param
 *
 * \return A newly created hbox.
 **/
static GtkWidget *gsb_scheduler_list_create_toolbar (void)
{
    GtkWidget *toolbar;
    GtkToolItem *item;

    toolbar = gtk_toolbar_new ();

    /* new scheduled button */
    item = utils_buttons_tool_button_new_from_image_label ("gsb-new-scheduled-24.png", _("New scheduled"));
    gtk_widget_set_tooltip_text (GTK_WIDGET (item),
								 _("Prepare form to create a new scheduled transaction"));
    g_signal_connect_swapped (G_OBJECT (item),
							  "clicked",
							  G_CALLBACK (gsb_scheduler_list_edit_transaction),
							  GINT_TO_POINTER (-1));
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    /* delete button */
    scheduler_button_delete = GTK_WIDGET (utils_buttons_tool_button_new_from_image_label ("gtk-delete-24.png",
																						  _("Delete")));
    gtk_widget_set_sensitive (scheduler_button_delete, FALSE);
    gtk_widget_set_tooltip_text (GTK_WIDGET (scheduler_button_delete),
								 _("Delete selected scheduled transaction"));
    g_signal_connect (G_OBJECT (scheduler_button_delete),
                        "clicked",
                        G_CALLBACK (gsb_scheduler_list_delete_scheduled_transaction_by_menu),
                        NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (scheduler_button_delete), -1);

    /* edit button */
    scheduler_button_edit = GTK_WIDGET (utils_buttons_tool_button_new_from_image_label ("gtk-edit-24.png", _("Edit")));
    gtk_widget_set_sensitive (scheduler_button_edit, FALSE);
    gtk_widget_set_tooltip_text (GTK_WIDGET (scheduler_button_edit), _("Edit selected transaction"));
    g_signal_connect_swapped (G_OBJECT (scheduler_button_edit),
										"clicked",
                        				G_CALLBACK (gsb_scheduler_list_edit_transaction),
                        				0);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (scheduler_button_edit), -1);

    /* Display/hide comments */
    scheduler_display_hide_notes = GTK_WIDGET (utils_buttons_tool_button_new_from_image_label ("gsb-comments-24.png",
																							   _("Notes")));
    gtk_widget_set_tooltip_text (GTK_WIDGET (scheduler_display_hide_notes),
								 _("Display the notes of scheduled transactions"));
    g_signal_connect (G_OBJECT (scheduler_display_hide_notes),
                      "clicked",
                      G_CALLBACK (gsb_scheduler_list_show_notes),
                      NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (scheduler_display_hide_notes), -1);

    /* Execute transaction */
    scheduler_button_execute = GTK_WIDGET (utils_buttons_tool_button_new_from_image_label ("gtk-execute-24.png",
																						   _("Execute")));
    gtk_widget_set_sensitive (scheduler_button_execute, FALSE);
    gtk_widget_set_tooltip_text (GTK_WIDGET (scheduler_button_execute), _("Execute current scheduled transaction"));
    g_signal_connect_swapped (G_OBJECT (scheduler_button_execute),
							  "clicked",
                        	  G_CALLBACK (gsb_scheduler_list_execute_transaction),
                        	  NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (scheduler_button_execute), -1);

	/* View button */
    item = utils_buttons_tool_button_new_from_image_label ("gtk-select-color-24.png", _("View"));
    gtk_widget_set_tooltip_text (GTK_WIDGET (item),
								 _("Change display mode of scheduled transaction list"));
    g_signal_connect (G_OBJECT (item),
                      "clicked",
                      G_CALLBACK (gsb_scheduler_list_popup_scheduler_view),
                      NULL);
    gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

    return toolbar;
}

/**
 * return the scheduler tree_model
 *
 * \param
 *
 * \return the scheduler tree_model
 **/
static GtkTreeModel *gsb_scheduler_list_get_model (void)
{
    return tree_model_scheduler_list;
}

/**
 * get the iter of the scheduled transaction given in param
 *
 * \param scheduled_number
 *
 * \return a newly allocated GtkTreeIter or NULL if not found
 **/
static GtkTreeIter *gsb_scheduler_list_get_iter_from_scheduled_number (gint scheduled_number)
{
    GtkTreeIter iter;
    GtkTreeModel *model;

    model = GTK_TREE_MODEL (gsb_scheduler_list_get_model ());

    if (!scheduled_number || !model)
		return NULL;

    /* we go through the list in the model untill we find the transaction */
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
		do
		{
			GtkTreeIter iter_child;
			gint scheduled_number_tmp;

			gtk_tree_model_get (GTK_TREE_MODEL (model),
								&iter,
								SCHEDULER_COL_NB_TRANSACTION_NUMBER, &scheduled_number_tmp,
								-1);
			if (scheduled_number == scheduled_number_tmp)
				return (gtk_tree_iter_copy (&iter));

			/* gtk_tree_iter_next doesn't go in the children, so if the current transaction
			 * has children, we have to look for the transaction here, and go down into the children */
			if (gtk_tree_model_iter_children (GTK_TREE_MODEL (model), &iter_child, &iter))
			{
				/* ok so iter_child is on a split child, we go to see all the splits */
				do
				{
					gtk_tree_model_get (GTK_TREE_MODEL (model),
										&iter_child,
										SCHEDULER_COL_NB_TRANSACTION_NUMBER, &scheduled_number_tmp,
										-1);
					if (scheduled_number == scheduled_number_tmp)
						return (gtk_tree_iter_copy (&iter_child));
				}
			while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model),&iter_child));
			}
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (model),&iter));

    }
    return NULL;
}

/**
 * switch the expander of the split given in param
 *
 * \param scheduled_number the scheduled split we want to switch
 *
 * \return FALSE
 **/
static gboolean gsb_scheduler_list_switch_expander (gint scheduled_number)
{
    GtkTreeIter *iter;
    GtkTreePath *path;
    GtkTreePath *path_sorted;

    if (!gsb_data_scheduled_get_split_of_scheduled (scheduled_number)
	 || !tree_view_scheduler_list)
		return FALSE;

    iter = gsb_scheduler_list_get_iter_from_scheduled_number (scheduled_number);

    path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model_scheduler_list), iter);
    path_sorted = gtk_tree_model_sort_convert_child_path_to_path (tree_model_sort_scheduler_list, path);
    if (gtk_tree_view_row_expanded (GTK_TREE_VIEW (tree_view_scheduler_list), path_sorted))
		gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view_scheduler_list), path_sorted);
    else
		gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view_scheduler_list), path_sorted, FALSE);
    gtk_tree_path_free (path);
    gtk_tree_path_free (path_sorted);
    gtk_tree_iter_free (iter);

    return FALSE;
}

/**
 * called when a key is pressed on the scheduled transactions list
 *
 * \param tree_view
 * \param ev
 *
 * \return FALSE
 **/
static gboolean gsb_scheduler_list_key_press (GtkWidget *tree_view,
											  GdkEventKey *ev)
{
    gint scheduled_number;

    scheduled_number = gsb_scheduler_list_get_current_scheduled_number ();

    switch (ev->keyval)
    {
		case GDK_KEY_Return :		/* touches entrée */
		case GDK_KEY_KP_Enter :

			if (scheduled_number)
				gsb_scheduler_list_edit_transaction (scheduled_number);
			break;


		case GDK_KEY_Delete :               /*  del  */

			if (scheduled_number > 0)
				gsb_scheduler_list_delete_scheduled_transaction (scheduled_number, TRUE);
			break;

		case GDK_KEY_Left:
			/* if we press left, give back the focus to the tree at left */
			gtk_widget_grab_focus (gsb_gui_navigation_get_tree_view ());
			break;

		case GDK_KEY_space:
			/* space open/close a split */
			gsb_scheduler_list_switch_expander (scheduled_number);
			break;
    }
    return (FALSE);
}

/**
 * called when the size of the tree view changed, to keep the same ration
 * between the columns
 *
 * \param tree_view	    the tree view of the scheduled transactions list
 * \param allocation	the new size
 * \param null
 *
 * \return FALSE
 **/
static void gsb_scheduler_list_size_allocate (GtkWidget *tree_view,
											  GtkAllocation *allocation,
											  gpointer null)
{
    gint i;
	//~ devel_debug_int (allocation->width);

	if (gsb_gui_navigation_get_current_page () != GSB_SCHEDULER_PAGE)
		return;

	if (allocation->width == scheduler_current_tree_view_width)
    {
		gint somme = 0;
		gint tmp_width;

        /* size of the tree view didn't change, but we received an allocated signal
         * it happens several times, and especially when we change the columns,
         * so we update the colums */

        /* sometimes, when the list is not visible, he will set all the columns to 1%...
         * we block that here */
        if (gtk_tree_view_column_get_width (scheduler_list_column[0]) == 1)
            return;

        for (i=0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS -1; i++)
        {
			if (gtk_tree_view_column_get_visible (scheduler_list_column[i]))
			{
				tmp_width = (gtk_tree_view_column_get_width
							 (scheduler_list_column[i]) * 100) / allocation->width + 1;
				if (tmp_width != scheduler_col_width[i])
				{
					scheduler_col_width[i] = tmp_width;
					gsb_file_set_modified (TRUE);
				}
				somme+= scheduler_col_width[i];
			}
		}
		scheduler_col_width[i] = 100 - somme;

		return;
    }

    /* the size of the tree view changed, we keep the ration between the columns,
     * we don't set the size of the last column to avoid the calculate problems,
     * it will take the end of the width alone */
    scheduler_current_tree_view_width = allocation->width;

	for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS -1 ; i++)
    {
        gint width;

        width = (scheduler_col_width[i] * (allocation->width))/ 100;
        if (width > 0 && gtk_tree_view_column_get_visible (scheduler_list_column[i]))
            gtk_tree_view_column_set_fixed_width (scheduler_list_column[i], width);
    }

	/* update tree_view */
	gsb_scheduler_list_update_tree_view (tree_view);
}

/**
 * create and configure the tree view of the scheduled transactions
 *
 * \param
 *
 * \return the tree_view
 **/
static GtkWidget *gsb_scheduler_list_create_tree_view (void)
{
    GtkWidget * tree_view;

    tree_view = gtk_tree_view_new ();
	gtk_widget_set_name (tree_view, "tree_view");

    /* can select only one line */
    gtk_tree_selection_set_mode (GTK_TREE_SELECTION (gtk_tree_view_get_selection (GTK_TREE_VIEW(tree_view))),
								 GTK_SELECTION_SINGLE);

    g_signal_connect (G_OBJECT (tree_view),
					  "size-allocate",
                       G_CALLBACK (gsb_scheduler_list_size_allocate),
                       NULL);
    g_signal_connect (G_OBJECT (tree_view),
                       "button-press-event",
                       G_CALLBACK (gsb_scheduler_list_button_press),
                       NULL);

    g_signal_connect (G_OBJECT (tree_view),
                       "key-press-event",
                       G_CALLBACK (gsb_scheduler_list_key_press),
                       NULL);

    gtk_widget_show (tree_view);

    last_scheduled_number = -1;

    return tree_view;
}

/**
 * fill the row pointed by the iter with the content of
 * the char tab given in param
 *
 * \param store
 * \param iter
 * \param line a tab of gchar with SCHEDULER_COL_VISIBLE_COLUMNS of size, which is the text content of the line
 *
 * \return FALSE
 **/
static gboolean gsb_scheduler_list_fill_transaction_row (GtkTreeStore *store,
														 GtkTreeIter *iter,
														 gchar *line[SCHEDULER_COL_VISIBLE_COLUMNS])
{
    gchar *color_str = NULL;
    gint i;

    if (line[COL_NB_AMOUNT] && g_utf8_strchr (line[COL_NB_AMOUNT], -1, '-'))
        color_str = (gchar*)"red";

    for (i=0 ; i<SCHEDULER_COL_VISIBLE_COLUMNS ; i++)
    {
		if (!line[i])
			continue;

		if (i == 6)
            gtk_tree_store_set (store, iter, i, line[i], SCHEDULER_COL_NB_AMOUNT_COLOR, color_str, -1);
        else
            gtk_tree_store_set (store, iter, i, line[i], -1);
    }

    return FALSE;
}

/**
 * fill the char tab in the param with the transaction given in param
 *
 * \param scheduled_number
 * \param  line a tab of gchar with SCHEDULER_COL_VISIBLE_COLUMNS of size, which will contain the text of the line
 *
 * \return FALSE
 **/
static gboolean gsb_scheduler_list_fill_transaction_text (gint scheduled_number,
														  gchar *line[SCHEDULER_COL_VISIBLE_COLUMNS])
{
    if (gsb_data_scheduled_get_mother_scheduled_number (scheduled_number))
    {
		/* for child split we set all to NULL except the party, we show the category instead */
		line[COL_NB_DATE] = NULL;
		line[COL_NB_FREQUENCY] = NULL;
		line[COL_NB_ACCOUNT] = NULL;
		line[COL_NB_MODE] = NULL;

		if (gsb_data_scheduled_get_category_number (scheduled_number))
			line[COL_NB_PARTY] = gsb_data_category_get_name (gsb_data_scheduled_get_category_number
															 (scheduled_number),
															 gsb_data_scheduled_get_sub_category_number
															 (scheduled_number),
															 NULL);
		else
		{
			/* there is no category, it can be a transfer */
			if (gsb_data_scheduled_get_account_number_transfer (scheduled_number) >= 0 && scheduled_number > 0)
			{
				/* it's a transfer */
				if (gsb_data_scheduled_get_amount (scheduled_number).mantissa < 0)
					line[COL_NB_PARTY] = g_strdup_printf (_("Transfer to %s"),
														  gsb_data_account_get_name
														  (gsb_data_scheduled_get_account_number_transfer
														   (scheduled_number)));
				else
					line[COL_NB_PARTY] = g_strdup_printf (_("Transfer from %s"),
														  gsb_data_account_get_name
														  (gsb_data_scheduled_get_account_number_transfer
														   (scheduled_number)));
			}
			else
				/* it's not a transfer, so no category */
				line[COL_NB_PARTY] = NULL;
		}
    }
    else
    {
		/* fill her for normal scheduled transaction (not children) */
		gint frequency;

		line[COL_NB_DATE] = gsb_format_gdate (gsb_data_scheduled_get_date (scheduled_number));
		frequency = gsb_data_scheduled_get_frequency (scheduled_number);

		if (frequency == SCHEDULER_PERIODICITY_CUSTOM_VIEW)
		{
			switch (gsb_data_scheduled_get_user_interval (scheduled_number))
			{
				case PERIODICITY_DAYS:
					line[COL_NB_FREQUENCY] = g_strdup_printf (_("%d days"),
															  gsb_data_scheduled_get_user_entry (scheduled_number));
					break;

				case PERIODICITY_WEEKS:
					line[COL_NB_FREQUENCY] = g_strdup_printf (_("%d weeks"),
															  gsb_data_scheduled_get_user_entry (scheduled_number));
					break;

				case PERIODICITY_MONTHS:
					line[COL_NB_FREQUENCY] = g_strdup_printf (_("%d months"),
															  gsb_data_scheduled_get_user_entry (scheduled_number));
					break;

				case PERIODICITY_YEARS:
					line[COL_NB_FREQUENCY] = g_strdup_printf (_("%d years"),
															  gsb_data_scheduled_get_user_entry (scheduled_number));
			}
		}
		else
		{
			if (frequency < SCHEDULER_PERIODICITY_NB_CHOICES && frequency >= 0)
			{
				gchar * names[] = {_("Once"), _("Weekly"), _("Monthly"),
					_("Bimonthly"), _("Quarterly"), _("Yearly") };

				line[COL_NB_FREQUENCY] = g_strdup (names [frequency]);
			}
		}
		line[COL_NB_ACCOUNT] = g_strdup (gsb_data_account_get_name (gsb_data_scheduled_get_account_number
																	(scheduled_number)));
		line[COL_NB_PARTY] = g_strdup (gsb_data_payee_get_name (gsb_data_scheduled_get_party_number
																(scheduled_number), TRUE));

		if (gsb_data_scheduled_get_automatic_scheduled (scheduled_number))
			line[COL_NB_MODE]= g_strdup (_("Automatic"));
		else
			line[COL_NB_MODE] = g_strdup (_("Manual"));
    }

    /* that can be filled for mother and children of split */
    line[COL_NB_NOTES] = g_strdup (gsb_data_scheduled_get_notes (scheduled_number));

    /* if it's a white line don't fill the amount
     * (in fact fill nothing, but normally all before was set to NULL,
     * there is only the amount, we want NULL and not 0) */
    if (scheduled_number < 0)
        line[COL_NB_AMOUNT] = NULL;
    else
        line[COL_NB_AMOUNT] = utils_real_get_string_with_currency (
                        gsb_data_scheduled_get_amount (scheduled_number),
                        gsb_data_scheduled_get_currency_number (scheduled_number),
                        TRUE);

    return FALSE;
}

/**
 * the same as gsb_scheduler_list_get_iter_from_scheduled_number but
 * return a gslist of iter corresponding to that scheduled number,
 * so there is only 1 iter for the once view, but more than 1 for the other views
 * use when changin the scheduled, to change also the virtuals ones on the screen
 *
 * \param scheduled_number
 *
 * \return a gslist of pointer to the iters, need to be free, or NULL if not found
 **/
static GSList *gsb_scheduler_list_get_iter_list_from_scheduled_number (gint scheduled_number)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    gint mother_number;
    gboolean return_iter = TRUE;
    GSList *iter_list = NULL;

    if (!scheduled_number)
		return NULL;

    /* that function is called too for deleting a scheduled transaction,
     * and it can be already deleted, so we cannot call gsb_data_scheduled_... here
     * but... we need to know if it's a child split, so we call it, in all
     * the cases, if the transactions doesn't exist we will have no mother, so very good !*/

    mother_number = gsb_data_scheduled_get_mother_scheduled_number (scheduled_number);

    model = gsb_scheduler_list_get_model ();

    /* go throw the list to find the transaction */
    if (!gtk_tree_model_get_iter_first (model, &iter))
		return NULL;

    while (return_iter)
    {
		gint scheduled_transaction_buf;

		gtk_tree_model_get (model,
							&iter,
					 		SCHEDULER_COL_NB_TRANSACTION_NUMBER, &scheduled_transaction_buf,
					 		-1);

		if (scheduled_transaction_buf == scheduled_number)
		{
			iter_list = g_slist_append (iter_list, gtk_tree_iter_copy (&iter));

			/* we have found the correct iter, but we want to continue to search,
			 * so we have to go back to the mother if necessary
			 * after that scheduled_transaction_buf is on the child, not on the
			 * mother so we will continue to the next scheduled transasction */

			if (mother_number)
			{
				GtkTreeIter *child_iter;

				child_iter = gtk_tree_iter_copy (&iter);
				gtk_tree_model_iter_parent (model, &iter, child_iter);
				gtk_tree_iter_free (child_iter);
			}

		}

		if (scheduled_transaction_buf == mother_number)
		{
			GtkTreeIter *mother_iter;

			mother_iter = gtk_tree_iter_copy (&iter);
			return_iter = gtk_tree_model_iter_children (model, &iter, mother_iter);
			gtk_tree_iter_free (mother_iter);
		}
		else
			return_iter = gtk_tree_model_iter_next (model, &iter);
    }
    return iter_list;
}

/**
 * remove the orphan transactions
 *
 * \param orphan list
 *
 * \return void
 */
static void gsb_scheduler_list_remove_orphan_list (GSList *orphan_scheduled,
												   GDate *end_date)
{
    GSList *tmp_list;
    gchar *string = NULL;
    GArray *garray;
	static bool on_going = false;

    garray = g_array_new (FALSE, FALSE, sizeof (gint));
    tmp_list = orphan_scheduled;
    while (tmp_list)
    {
        gint scheduled_number;

        scheduled_number = gsb_data_scheduled_get_scheduled_number (tmp_list->data);

        if (!gsb_scheduler_list_append_new_scheduled (scheduled_number, end_date))
        {
            /* on sauvegarde le numéro de l'opération */
            g_array_append_val (garray, scheduled_number);
            if (string == NULL)
                string = utils_str_itoa (scheduled_number);
            else
			{
				gchar *tmp_str1, *tmp_str2;
				tmp_str1 = utils_str_itoa (scheduled_number);
				tmp_str2 = g_strconcat (string, " - ", tmp_str1, NULL);
				g_free(tmp_str1);
				g_free(string);
				string = tmp_str2;
			}
        }

        tmp_list = tmp_list->next;
    }

    /* if string is not null, there is still some children
     * which didn't find their mother. show them now */
    if (string)
	{
		if (! on_going)
		{
			gchar *message;
			gint result;

			on_going = true;
			message = g_strdup_printf (_("Some scheduled children didn't find their mother in the list, "
										 "this shouldn't happen and there is probably a bug behind that.\n\n"
										 "The concerned children number are:\n %s\n\n"
										 "Do you want to delete it?"),
									   string);

			result = dialogue_yes_no (message, _("Remove orphan children"), GTK_RESPONSE_CANCEL);

			if (result == TRUE)
			{
				gint i;

				for (i = 0; i < (gint) garray->len; i++)
					gsb_data_scheduled_remove_scheduled (g_array_index (garray, gint, i));

			}

			g_free (message);

			on_going = false;
		}
		g_free (string);
		g_array_free (garray, TRUE);
	}
}

/**
 * set the text red if the variance is non zero
 *
 * \param
 * \param
 *
 * \return
 **/
static gboolean gsb_scheduler_list_set_color_of_mother (gint mother_scheduled_number,
														gboolean is_red)
{
    GtkTreeIter *iter = NULL;
    gchar *color_str = NULL;
    gint i;

    iter = gsb_scheduler_list_get_iter_from_scheduled_number (mother_scheduled_number);
    if (!iter)
        return FALSE;

    if (is_red)
        color_str = (gchar*)"red";

    for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS -1 ; i++)
    {
        gtk_tree_store_set (GTK_TREE_STORE (tree_model_scheduler_list),
							iter,
							SCHEDULER_COL_NB_TEXT_COLOR, color_str,
							-1);
    }

    return TRUE;
}

/**
 * set the scheduler tree view
 *
 * \param tree_view
 *
 * \return
 **/
static void gsb_scheduler_list_set_tree_view (GtkWidget *tree_view)
{
    tree_view_scheduler_list = tree_view;
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
static void gsb_scheduler_list_set_virtual_amount_with_loan (gint scheduled_number,
															 gchar *line[SCHEDULER_COL_VISIBLE_COLUMNS],
															 gint account_number)
{
	gdouble amount;

	amount = bet_data_loan_get_other_echeance_amount (account_number);
	line[COL_NB_AMOUNT] = utils_real_get_string_with_currency (gsb_real_opposite (gsb_real_double_to_real (amount)),
															   gsb_data_scheduled_get_currency_number (scheduled_number),
															   TRUE);
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
static gboolean gsb_scheduler_list_update_white_child (gint white_line_number,
													   gint mother_scheduled_number)
{
    GtkTreeIter *iter = NULL;
    GSList *tmp_list;
    GsbReal total_split = null_real;
    GsbReal variance;
    gchar *tmp_str;

    if (!tree_model_scheduler_list)
        return FALSE;

    iter = gsb_scheduler_list_get_iter_from_scheduled_number (white_line_number);
    if (!iter)
        return FALSE;

    tmp_list = gsb_data_scheduled_get_scheduled_list ();
    while (tmp_list)
    {
        gint split_scheduled_number;

        split_scheduled_number = gsb_data_scheduled_get_scheduled_number (tmp_list->data);

        if (gsb_data_scheduled_get_mother_scheduled_number (split_scheduled_number) == mother_scheduled_number)
        {
            total_split = gsb_real_add (total_split, gsb_data_scheduled_get_amount (split_scheduled_number));
        }
        tmp_list = tmp_list->next;
    }

    variance = gsb_real_sub (gsb_data_scheduled_get_amount (mother_scheduled_number), total_split);

    /* show the variance and sub-total only if different of the scheduled */
    if (variance.mantissa)
    {
        gchar *amount_string;
        gchar *variance_string;
        gint currency_number;

        currency_number = gsb_data_scheduled_get_currency_number (mother_scheduled_number);
        amount_string = utils_real_get_string_with_currency (total_split, currency_number, TRUE);
        variance_string = utils_real_get_string_with_currency (variance, currency_number, TRUE);

        tmp_str = g_strdup_printf (_("Total: %s (variance : %s)"), amount_string, variance_string);

        g_free (amount_string);
        g_free (variance_string);

        gsb_scheduler_list_set_color_of_mother (mother_scheduled_number, TRUE);
    }
    else
    {
        tmp_str = g_strdup("");
        gsb_scheduler_list_set_color_of_mother (mother_scheduled_number, FALSE);
    }

    gtk_tree_store_set (GTK_TREE_STORE (tree_model_scheduler_list), iter, 2, tmp_str, -1);

    if (tmp_str)
        g_free (tmp_str);

    return TRUE;
}

/******************************************************************************/
/* Public functions                                                           */
/******************************************************************************/
/**
 * libère la mémoire utilisée par GSLists de création des opérations
 * arrivées à éhéance
 *
 * \param
 *
 * \return
 **/
void gsb_scheduler_list_free_variables (void)
{
    if (scheduled_transactions_to_take)
    {
        g_slist_free (scheduled_transactions_to_take);
    }
    if (scheduled_transactions_taken)
    {
        g_slist_free (scheduled_transactions_taken);
    }
}

/**
 *
 *
 * \param
 *
 * \return
 **/
void gsb_scheduler_list_init_variables (void)
{
    if (scheduled_transactions_to_take)
    {
        g_slist_free (scheduled_transactions_to_take);
        scheduled_transactions_to_take = NULL;
    }
    if (scheduled_transactions_taken)
    {
        g_slist_free (scheduled_transactions_taken);
        scheduled_transactions_taken = NULL;
    }

    /* on réinitialise tous les widgets */
    scheduler_display_hide_notes = NULL;
    scheduler_button_execute = NULL;
    scheduler_button_delete = NULL;
    scheduler_button_edit = NULL;
}

/**
 * create the scheduler list
 *
 * \param
 *
 * \return a vbox widget containing the list
 **/
GtkWidget *gsb_scheduler_list_create_list (void)
{
    GtkWidget *vbox, *scrolled_window;
    GtkWidget *tree_view;
    GtkWidget *frame;
	GtkTreeModel *tree_model;

    devel_debug (NULL);

    /* first, a vbox */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, MARGIN_BOX);

    /* frame pour la barre d'outils */
    frame = gtk_frame_new (NULL);
    gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

    /* création de la barre d'outils */
    scheduler_toolbar = gsb_scheduler_list_create_toolbar ();
    gtk_container_add (GTK_CONTAINER (frame), scheduler_toolbar);

    /* create the scrolled window */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					  GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_widget_show (scrolled_window);

    /* we create and set the tree_view in the page */
    tree_view = gsb_scheduler_list_create_tree_view ();
	gtk_widget_set_margin_end (tree_view, MARGIN_END);
    gsb_scheduler_list_set_tree_view (tree_view);
    gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

    /* set the color of selected row */
	gtk_widget_set_name (tree_view, "tree_view");

    /* create the store and set it in the tree_view */
    tree_model = gsb_scheduler_list_create_model ();
    gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), tree_model);
    g_object_unref (G_OBJECT(tree_model));

    /* create the columns */
    gsb_scheduler_list_create_list_columns (tree_view);

    /* begin by hiding the notes */
    gsb_scheduler_list_show_notes (scheduler_display_hide_notes);

	gtk_widget_show_all (vbox);

    return vbox;
}


/**
 *
 *
 *
 */
void gsb_gui_scheduler_toolbar_set_style (gint toolbar_style)
{
    gtk_toolbar_set_style (GTK_TOOLBAR (scheduler_toolbar), toolbar_style);
}


/**
 * return the scheduler tree view
 *
 * \param
 *
 * \return the scheduler tree_view
 **/
GtkWidget *gsb_scheduler_list_get_tree_view (void)
{
    return tree_view_scheduler_list;
}

/**
 * called to execute a sheduled transaction, either by the button on the toolbar,
 * either by click on the first page
 * if scheduled_number is 0, get the selected transaction
 *
 * \param scheduled_number
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_execute_transaction (gint scheduled_number)
{
    devel_debug_int (scheduled_number);

    if (!scheduled_number)
		scheduled_number = gsb_scheduler_list_get_current_scheduled_number ();

    gsb_scheduler_list_edit_transaction (scheduled_number);

    /* the only difference for now between an execution and a edition of scheduled is here :
     * set the flag to say that we execute the scheduled transaction
     * and hide the scheduler part of the form */

	/* pbiava the execute flag is a gint : set to 1 */
    g_object_set_data (G_OBJECT (gsb_form_get_form_widget ()), "execute_scheduled", GINT_TO_POINTER (1));
    gtk_widget_hide (gsb_form_get_scheduler_part ());

    return FALSE;
}

/**
 * fill the scheduled transactions list
 *
 * \return TRUE or FALSE
 **/
gboolean gsb_scheduler_list_fill_list (void)
{
    GSList *tmp_list;
    GDate *end_date;
    GtkTreeIter iter;
    GSList *orphan_scheduled = NULL;
	GrisbiAppConf *a_conf;

    devel_debug (NULL);

    /* get the last date we want to see the transactions */
    end_date = gsb_scheduler_list_get_end_date_scheduled_showed ();

	if (!tree_model_scheduler_list || !GTK_IS_TREE_STORE (tree_model_scheduler_list))
		return  FALSE;
	else
	    gtk_tree_store_clear (GTK_TREE_STORE (tree_model_scheduler_list));

    /* fill the list */
    tmp_list = gsb_data_scheduled_get_scheduled_list ();

    while (tmp_list)
    {
        gint scheduled_number;

        scheduled_number = gsb_data_scheduled_get_scheduled_number (tmp_list->data);

        if (!end_date || g_date_compare (gsb_data_scheduled_get_date (scheduled_number), end_date) <= 0)
        {
            if (!gsb_scheduler_list_append_new_scheduled (scheduled_number, end_date))
                /* the scheduled transaction was not added, add to orphan scheduledlist */
                orphan_scheduled = g_slist_append (orphan_scheduled, tmp_list->data);
        }
        tmp_list = tmp_list->next;
    }

    /* if there are some orphan sheduler (children of breakdonw which didn't find their mother */
    if (orphan_scheduled)
    {
        gsb_scheduler_list_remove_orphan_list (orphan_scheduled, end_date);
        g_slist_free (orphan_scheduled);
    }

    /* create and append the white line */
    gtk_tree_store_append (GTK_TREE_STORE (tree_model_scheduler_list), &iter, NULL);
    gtk_tree_store_set (GTK_TREE_STORE (tree_model_scheduler_list),
                        &iter,
                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, gsb_data_scheduled_new_white_line (0),
                        -1);

	/* get first scheduled of the list */
	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
	if (!a_conf->select_scheduled_in_list)
	{
		GtkWidget *tree_view;
		GtkTreeModel *model;

		tree_view = gsb_scheduler_list_get_tree_view ();

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
		if (gtk_tree_model_get_iter_first (model, &iter))
			gtk_tree_model_get (model,
								&iter,
								SCHEDULER_COL_NB_TRANSACTION_NUMBER, &first_scheduler_list_number,
								-1);
		else
			return  FALSE;
	}
	return TRUE;
}

/**
 * send a "row-changed" to all the row of the showed transactions,
 * so in fact re-draw the list and colors
 *
 * \param
 *
 * \return
 **/
gboolean gsb_scheduler_list_redraw (void)
{
    GtkTreeIter iter;
    GtkTreeModel *tree_model;

    tree_model = gsb_scheduler_list_get_model ();

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (tree_model), &iter))
    {
		do
		{
			GtkTreePath *path;
			GtkTreeIter child_iter;

			path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model), &iter);
			gtk_tree_model_row_changed (GTK_TREE_MODEL (tree_model), path, &iter);
			gtk_tree_path_free(path);

			/* update the children if necessary */
			if (gtk_tree_model_iter_children (GTK_TREE_MODEL (tree_model), &child_iter, &iter))
			{
				do
				{
					path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model), &child_iter);
					gtk_tree_model_row_changed (GTK_TREE_MODEL (tree_model), path, &child_iter);
					gtk_tree_path_free(path);
				}
				while (gtk_tree_model_iter_next (GTK_TREE_MODEL (tree_model), &child_iter));
			}
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (tree_model), &iter));
    }
    return FALSE;
}

/**
 * append the scheduled transaction to the tree_view given in param
 * if that transaction need to be appended several times (untill end_date),
 * it's done here
 *
 * \param scheduled_number
 * \param end_date
 *
 * \return TRUE : scheduled added, FALSE : not added (usually for children who didn't find their mother)
 **/
gboolean gsb_scheduler_list_append_new_scheduled (gint scheduled_number,
												  GDate *end_date)
{
	GDate *tmp_date;
    GDate *pGDateCurrent;
    GtkTreeIter *mother_iter = NULL;
    gchar *line[SCHEDULER_COL_VISIBLE_COLUMNS] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};
    gint mother_scheduled_number;
	gint fixed_date = 0;
	gint split_transaction;
	gint transfer_account = 0;
    gint virtual_transaction = 0;
	gboolean first_is_different = FALSE;

    /* devel_debug_int (scheduled_number); */
    if (!tree_model_scheduler_list)
        return FALSE;

    /* get the mother iter if needed */
    mother_scheduled_number = gsb_data_scheduled_get_mother_scheduled_number (scheduled_number);
    if (mother_scheduled_number)
    {
        gint white_line_number;

        mother_iter = gsb_scheduler_list_get_iter_from_scheduled_number (mother_scheduled_number);
        if (!mother_iter)
            /* it's a child but didn't find the mother, it can happen in old files previous to 0.6
             * where the children wer saved before the mother, return FALSE here will add that
             * child to a list to append it again later */
            return FALSE;

        white_line_number = gsb_data_scheduled_get_white_line (mother_scheduled_number);
        gsb_scheduler_list_update_white_child (white_line_number, mother_scheduled_number);
    }

	/* set the good date */
	tmp_date = gsb_data_scheduled_get_date (scheduled_number);
	pGDateCurrent = gsb_date_copy (tmp_date);
	fixed_date = gsb_data_scheduled_get_fixed_date (scheduled_number);
	if (fixed_date)
	{
		g_date_set_day (pGDateCurrent, fixed_date);
		if (!g_date_valid (pGDateCurrent))
		{
			g_date_free (pGDateCurrent);
			pGDateCurrent = gsb_date_get_last_day_of_month (tmp_date);
		}
	}

	  /* fill the text line */
	split_transaction = gsb_data_scheduled_get_split_of_scheduled (scheduled_number);
	if (split_transaction)
	{
		gint init_sch_with_loan;

		transfer_account = gsb_data_scheduled_get_account_number_transfer (scheduled_number+1);
		init_sch_with_loan = gsb_data_account_get_bet_init_sch_with_loan (transfer_account);
		if (init_sch_with_loan) /* cette transaction concerne un prêt */
		{
			first_is_different = bet_data_loan_get_loan_first_is_different (transfer_account);
			if (first_is_different) /* les autres échéances sont différentes */
			{
				GSList *children_numbers_list;
				GsbReal amount;

				amount = bet_finance_get_loan_amount_at_date (scheduled_number,
															  transfer_account,
															  pGDateCurrent,
															  FALSE);
				gsb_data_scheduled_set_amount (scheduled_number, amount);

				/* on traite les opérations filles */
				children_numbers_list = gsb_data_scheduled_get_children (scheduled_number, TRUE);
				while (children_numbers_list)
				{
					gint child_number;

					child_number = GPOINTER_TO_INT (children_numbers_list->data);
					if (child_number)
					{
						amount = bet_finance_get_loan_amount_at_date (child_number,
																	  transfer_account,
																	  pGDateCurrent,
																	  FALSE);
						gsb_data_scheduled_set_amount (child_number, amount);
					}

					children_numbers_list = children_numbers_list->next;
				}
				g_slist_free (children_numbers_list);
			}
		}
	}
    gsb_scheduler_list_fill_transaction_text (scheduled_number, line);

    do
    {
        GtkTreeIter iter;

        gtk_tree_store_append (GTK_TREE_STORE (tree_model_scheduler_list), &iter, mother_iter);

		if (scheduled_number > 0)
			gsb_scheduler_list_fill_transaction_row (GTK_TREE_STORE (tree_model_scheduler_list), &iter, line);

        /* set the number of scheduled transaction to 0 if it's not the first one
         * (when more than one showed) */
        gtk_tree_store_set (GTK_TREE_STORE (tree_model_scheduler_list),
							&iter,
							SCHEDULER_COL_NB_TRANSACTION_NUMBER, scheduled_number,
							SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, virtual_transaction,
							-1);

        /* if it's a split, we append a white line now */
        if (split_transaction && !virtual_transaction)
        {
            gint white_line_number;

            white_line_number = gsb_data_scheduled_get_white_line (scheduled_number);
            if (white_line_number == -1)
                white_line_number = gsb_data_scheduled_new_white_line (scheduled_number);

            gsb_scheduler_list_append_new_scheduled (white_line_number, end_date);
            gsb_scheduler_list_update_white_child (white_line_number, scheduled_number);
        }

        /* if it's a split, we show only one time and color the background */
        if (mother_iter)
        {
            gtk_tree_store_set (GTK_TREE_STORE (tree_model_scheduler_list),
								&iter,
								SCHEDULER_COL_NB_BACKGROUND, gsb_rgba_get_couleur ("background_split"),
								-1);
        }
        else
        {
            pGDateCurrent = gsb_scheduler_get_next_date (scheduled_number, pGDateCurrent);

            line[COL_NB_DATE] = gsb_format_gdate (pGDateCurrent);

            /* now, it's not real transactions */
			if (first_is_different && virtual_transaction == 0)
				gsb_scheduler_list_set_virtual_amount_with_loan (scheduled_number, line, transfer_account);

			virtual_transaction ++;
        }
    }
    while (pGDateCurrent && end_date && g_date_compare (end_date, pGDateCurrent) >= 0 && !mother_iter);

    if (mother_iter)
        gtk_tree_iter_free (mother_iter);

    return TRUE;
}

/**
 * remove the given scheduled transaction from the list
 * and too all the corresponding virtual transactions
 *
 * \param transaction_number
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_remove_transaction_from_list (gint scheduled_number)
{
    GSList *iter_list;

    devel_debug_int (scheduled_number);

    if (!scheduled_number || !gsb_scheduler_list_get_model ())
		return FALSE;

    /* at this level, normally the transaction is already deleted,
     * so we cannot call gsb_data_scheduled_... we have only the number
     * to remove it from the list */

    iter_list = gsb_scheduler_list_get_iter_list_from_scheduled_number (scheduled_number);

    if (iter_list)
    {
		GtkTreeStore *store;
		GSList *tmp_list;

		store = GTK_TREE_STORE (gsb_scheduler_list_get_model ());
		tmp_list = iter_list;

		while (tmp_list)
		{
			GtkTreeIter *iter;

			iter = tmp_list->data;

			gtk_tree_store_remove (store, iter);
			gtk_tree_iter_free (iter);
			tmp_list = tmp_list->next;
		}
    }
    else
    {
        gchar *tmp_str;

        tmp_str = g_strdup_printf (_("in gsb_scheduler_list_remove_transaction_from_list, "
									 "ask to remove the transaction no %d,\nbut didn't find the iter in the list...\n"
									 "It's normal if appending a new scheduled transaction, but abnormal else..."),
								   scheduled_number);
        warning_debug (tmp_str);
        g_free (tmp_str);
    }
    return FALSE;
}

/**
 * update the scheduled transaction in the list (and all the virtuals too)
 *
 * \param scheduled_number
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_update_transaction_in_list (gint scheduled_number)
{
    GtkTreeStore *store;
    GtkTreeIter iter;
    GDate *pGDateCurrent;
    gchar *line[SCHEDULER_COL_VISIBLE_COLUMNS] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};;

    devel_debug_int (scheduled_number);

    if (!scheduled_number || !gsb_scheduler_list_get_model ())
        return FALSE;

    /* the same transaction can be showed more than one time because of the different views,
     * not so difficult, go throw the list and for each iter corresponding to the scheduled
     * transaction, re-fill the line */
    store = GTK_TREE_STORE (gsb_scheduler_list_get_model ());

    pGDateCurrent = gsb_date_copy (gsb_data_scheduled_get_date (scheduled_number));

    /* fill the text line */
    gsb_scheduler_list_fill_transaction_text (scheduled_number, line);

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
    {
        do
        {
            GtkTreeIter child_iter;
            gint scheduled_number_tmp;

            gtk_tree_model_get (GTK_TREE_MODEL (store),
                                &iter,
                                SCHEDULER_COL_NB_TRANSACTION_NUMBER, &scheduled_number_tmp,
                                -1);
            if (scheduled_number_tmp == scheduled_number)
            {
                gsb_scheduler_list_fill_transaction_row (GTK_TREE_STORE (store), &iter, line);

                /* go to the next date if ever there is several lines of that scheduled */
                pGDateCurrent = gsb_scheduler_get_next_date (scheduled_number, pGDateCurrent);

                line[COL_NB_DATE] = gsb_format_gdate (pGDateCurrent);
            }

            /* i still haven't found a function to go line by line, including the children,
             * so do another do/while into the first one */
            if (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &child_iter, &iter))
            {
                gint white_line_number = 0;
                gint mother_number = 0;

				/* we are on the child */
                do
                {
                    gtk_tree_model_get (GTK_TREE_MODEL (store),
                                        &child_iter,
                                        SCHEDULER_COL_NB_TRANSACTION_NUMBER, &scheduled_number_tmp,
                                        -1);

                    if (scheduled_number_tmp < -1)
                    {
                        white_line_number = scheduled_number_tmp;
                        mother_number = gsb_data_scheduled_get_mother_scheduled_number (white_line_number);
                    }

					if (scheduled_number_tmp > 0)
					{
						GsbReal amount;
						gchar *tmp_str;
						gchar *color_str = NULL;

						amount = gsb_data_scheduled_get_amount (scheduled_number_tmp);
						tmp_str = utils_real_get_string_with_currency (amount,
																	   gsb_data_scheduled_get_currency_number
																	   (scheduled_number_tmp),
																	   TRUE);
						if (line[COL_NB_AMOUNT] && g_utf8_strchr (line[COL_NB_AMOUNT], -1, '-'))
							color_str = (gchar*)"red";
						else
						{
							g_free (color_str);
							color_str = NULL;
						}
						gtk_tree_store_set (store,
											&child_iter,
											COL_NB_AMOUNT, tmp_str,
											SCHEDULER_COL_NB_AMOUNT_COLOR, color_str,
											-1);
					}
                }
                while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &child_iter));

                gsb_scheduler_list_update_white_child (white_line_number, mother_number);
            }
        }
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
    }

    return FALSE;
}

/**
 * set the background colors of the list
 * just for normal scheduled transactions, not for children of split
 *
 * \param tree_view
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_set_background_color (GtkWidget *tree_view)
{
    GtkTreeStore *store;
    GtkTreeModel *sort_model;
    GtkTreePath *sorted_path;
    GtkTreePath *path;
    gint current_color;

    if (!tree_view)
        return FALSE;

    sort_model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
    store = GTK_TREE_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (sort_model)));

    current_color = 0;
    sorted_path = gtk_tree_path_new_first ();

    while ((path = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (sort_model), sorted_path)))
    {
        GtkTreeIter iter;
        gint virtual_transaction;

        gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, path);

        gtk_tree_model_get (GTK_TREE_MODEL (store),
							&iter,
							SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_transaction,
							-1);

        if (virtual_transaction)
            gtk_tree_store_set (store,
								&iter,
								SCHEDULER_COL_NB_BACKGROUND, gsb_rgba_get_couleur ("background_scheduled"),
								-1);
        else
        {
            gtk_tree_store_set (store,
                        		&iter,
                        		SCHEDULER_COL_NB_BACKGROUND,
                        		gsb_rgba_get_couleur_with_indice ("couleur_fond", current_color),
                        		-1);
            current_color = !current_color;
        }

        gtk_tree_path_free (path);

        /* needn't to go in a child because the color is always the same, so
         * gtk_tree_path_next is enough */
        gtk_tree_path_next (sorted_path);
    }

    return FALSE;
}

/**
 * select the given scheduled transaction
 *
 * \param scheduled_number
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_select (gint scheduled_number)
{
    GtkTreeIter *iter;
    GtkTreeIter iter_sort;
    GtkTreePath *path = NULL;
    gint mother_number;

    devel_debug_int (scheduled_number);

    /* if it's a split child, we must open the mother to select it */
    mother_number = gsb_data_scheduled_get_mother_scheduled_number (scheduled_number);
    if (mother_number)
    {
        GtkTreeIter *iter_mother;
        GtkTreeIter iter_mother_sort;

        iter_mother = gsb_scheduler_list_get_iter_from_scheduled_number (mother_number);
        if (iter_mother)
        {

            gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (tree_model_sort_scheduler_list),
															&iter_mother_sort,
															iter_mother);
            path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model_sort_scheduler_list), &iter_mother_sort);
            gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view_scheduler_list), path, TRUE);
            gtk_tree_iter_free (iter_mother);
        }
    }

    /* now can work with the transaction we want to select */
    iter = gsb_scheduler_list_get_iter_from_scheduled_number (scheduled_number);

    if (!iter)
        return FALSE;

    gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (tree_model_sort_scheduler_list),
													&iter_sort,
													iter);

    gtk_tree_selection_select_iter (GTK_TREE_SELECTION (gtk_tree_view_get_selection
														(GTK_TREE_VIEW (tree_view_scheduler_list))),
									&iter_sort);

    /* move the tree view to the selection */
    if (path == NULL)
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (tree_model_sort_scheduler_list), &iter_sort);

    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (tree_view_scheduler_list), path, NULL, FALSE, 0.0, 0.0);

    gtk_tree_iter_free (iter);
    gtk_tree_path_free (path);

    return FALSE;
}

/**
 * find the date untill we want to show the scheduled transactions
 * on the scheduled list, with the user configuration
 *
 * \param
 *
 * \return a newly allocated final date or NULL for unique view
 **/
GDate *gsb_scheduler_list_get_end_date_scheduled_showed (void)
{
    GDate *end_date;
	GrisbiAppConf *a_conf;
	GrisbiWinEtat *w_etat;

	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();
	w_etat = grisbi_win_get_w_etat ();

	/* on récupère la date du jour et la met dans end_date pour les
    * vérifications ultérieures */
    end_date = gdate_today ();

    /* on calcule la date de fin de l'affichage */
	/* pbiava ajout d'un mois supplémentaire pour voir n mois effectifs */
	/* ou une annee a partir de la date du jour */
    switch (w_etat->affichage_echeances)
    {
		case SCHEDULER_PERIODICITY_ONCE_VIEW:
			return NULL;
			break;

		case SCHEDULER_PERIODICITY_WEEK_VIEW:
			g_date_add_days (end_date, 7);
			break;

		case SCHEDULER_PERIODICITY_MONTH_VIEW:
			g_date_add_months (end_date, 1);
			end_date->day = 1;
			g_date_subtract_days (end_date, 2);
			break;

		case SCHEDULER_PERIODICITY_TWO_MONTHS_VIEW:
			g_date_add_months (end_date, 3);
			end_date->day = 1;
			g_date_subtract_days (end_date, 1);
			break;

		case SCHEDULER_PERIODICITY_TRIMESTER_VIEW:
			g_date_add_months (end_date, 4);
			end_date->day = 1;
			g_date_subtract_days (end_date, 1);
			break;

		case SCHEDULER_PERIODICITY_YEAR_VIEW:
			g_date_add_months (end_date, 13);
			end_date->day = 1;
			end_date->month = 1;
			g_date_subtract_days (end_date, 1);
			break;

		case SCHEDULER_PERIODICITY_CUSTOM_VIEW:
			switch (w_etat->affichage_echeances_perso_j_m_a)
			{
				case PERIODICITY_DAYS:
					g_date_add_days (end_date, w_etat->affichage_echeances_perso_nb_libre);
					break;

				case PERIODICITY_WEEKS:
					g_date_add_days (end_date, w_etat->affichage_echeances_perso_nb_libre * 7);
					break;

				case PERIODICITY_MONTHS:
					g_date_add_months (end_date, w_etat->affichage_echeances_perso_nb_libre + 1);
					if (a_conf->execute_scheduled_of_month)
					{
						GDate *tmp_date;

						tmp_date = end_date;
						end_date = gsb_date_get_last_day_of_month (tmp_date);
						g_date_free (tmp_date);
					}
					break;

				case PERIODICITY_YEARS:
					g_date_add_years (end_date, w_etat->affichage_echeances_perso_nb_libre);
					break;
			}
    }

    return end_date;
}

/**
 * get the current selected transaction and return it
 * if it's a virtual transaction, return 0
 *
 * \param
 *
 * \return the current scheduled transaction number
 **/
gint gsb_scheduler_list_get_current_scheduled_number (void)
{
    GList *list_tmp;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkWidget *tree_view;
    gint scheduled_number;
    gint virtual_transaction;

    tree_view = gsb_scheduler_list_get_tree_view ();
    list_tmp = gtk_tree_selection_get_selected_rows (gtk_tree_view_get_selection
													 (GTK_TREE_VIEW (tree_view)),
													 &model);

    if (!list_tmp)
		return 0;

    gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, list_tmp->data);
    gtk_tree_model_get (GTK_TREE_MODEL (model),
						&iter,
						SCHEDULER_COL_NB_TRANSACTION_NUMBER, &scheduled_number,
						SCHEDULER_COL_NB_VIRTUAL_TRANSACTION, &virtual_transaction,
						-1);

    g_list_free_full (list_tmp, (GDestroyNotify) gtk_tree_path_free);

    if (virtual_transaction)
		return 0;
    else
		return scheduled_number;
}

/**
 * edit the scheduling transaction given in param
 *
 * \param scheduled_number the number, -1 if new scheduled transaction or < -1 if new child of split
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_edit_transaction (gint scheduled_number)
{
    devel_debug_int (scheduled_number);
    if (scheduled_number == 0)
	{
		gint tmp_number;

		tmp_number = gsb_scheduler_list_get_current_scheduled_number ();
        gsb_form_fill_by_transaction (tmp_number, FALSE, TRUE);
		last_scheduled_number = tmp_number;
	}
    else
	{
        gsb_form_fill_by_transaction (scheduled_number, FALSE, TRUE);
		last_scheduled_number = scheduled_number;
	}

    return FALSE;
}

/**
 * delete the current selected transaction, but called by menu
 * just call gsb_scheduler_list_delete_scheduled_transaction with show_warning = TRUE
 * because cannot do that by the signal
 *
 * \param button
 * \param
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_delete_scheduled_transaction_by_menu (GtkWidget *button,
																  gpointer null)
{
    gsb_scheduler_list_delete_scheduled_transaction (0, TRUE);
    return FALSE;
}

/**
 * delete the scheduled transaction
 *
 * \param scheduled_number the transaction to delete
 * \param show_warning TRUE to warn, FALSE to delete directly
 * 		!!this don't affect the question to delete only the occurence or the whole scheduled transaction
 * 		it affects only for children of split, and especially deleting the white line child
 *
 * \return FALSE
 **/
gboolean gsb_scheduler_list_delete_scheduled_transaction (gint scheduled_number,
														  gboolean show_warning)
{
    gchar *tmp_str;
    gint mother_number = 0;
    gint result;
    gint msg_no = 0;
	GrisbiWinRun *w_run;

    devel_debug_int (scheduled_number);
	w_run = (GrisbiWinRun *) grisbi_win_get_w_run ();

    if (!scheduled_number)
        scheduled_number = gsb_scheduler_list_get_current_scheduled_number ();

    /* return for white line only if show_warning is set
     * (means the action is not automatic) */
    if (scheduled_number <= 0 && show_warning)
		return FALSE;

    mother_number = gsb_data_scheduled_get_mother_scheduled_number (scheduled_number);

    /* show a warning */
    if (show_warning)
    {
		if (mother_number)
        {
            /* ask all the time for a child */
            tmp_str = g_strdup_printf (_("Do you really want to delete the child of the "
										 "scheduled transaction with party '%s' ?"),
									   gsb_data_payee_get_name (gsb_data_scheduled_get_party_number
															    (scheduled_number),
															    FALSE));
            if (!dialogue_conditional_yes_no_with_items ("tab_delete_msg", "delete-child-scheduled", tmp_str))
            {
                g_free (tmp_str);

				return FALSE;
            }
            g_free (tmp_str);
        }
        else
        {
            /* for a normal scheduled, ask only if no frequency, else, it will
             * have another dialog to delete the occurence or the transaction */
            tmp_str = g_strdup_printf (_("Do you really want to delete the scheduled "
										 "transaction with party '%s' ?"),
									   gsb_data_payee_get_name (gsb_data_scheduled_get_party_number
															    (scheduled_number),
															    FALSE));
            if (!gsb_data_scheduled_get_frequency (scheduled_number)
				&& !dialogue_conditional_yes_no_with_items ("tab_delete_msg", "delete-scheduled", tmp_str))
            {
                g_free (tmp_str);

				return FALSE;
            }
            g_free (tmp_str);
        }
    }

    /* split with child of split or normal scheduled,
     * for a child, we directly delete it, for mother, ask
     * for just that occurrence or the complete transaction */
    if (mother_number)
    {
        gint white_line_number;

        /* !!important to remove first from the list... */
        gsb_scheduler_list_remove_transaction_from_list (scheduled_number);
        gsb_data_scheduled_remove_scheduled (scheduled_number);

        white_line_number = gsb_data_scheduled_get_white_line (mother_number);
        gsb_scheduler_list_update_white_child (white_line_number, mother_number);
    }
    else
    {
        /* ask if we want to remove only the current one (so only change the date
         * for the next) or all (so remove the transaction */
        if (gsb_data_scheduled_get_frequency (scheduled_number))
        {
            GtkWidget *checkbox;
            GtkWidget *dialog = NULL;
            GtkWidget *vbox;
            gchar *occurrences;
			ConditionalMsg *warning;

			warning = (ConditionalMsg*) dialogue_get_tab_delete_msg ();
            msg_no = dialogue_conditional_yes_no_get_no_struct (warning,
																"delete-scheduled-occurrences");

            if (msg_no < 0)
				return FALSE;

            if ((warning+msg_no)->hidden)
				result = (warning+msg_no)->default_answer;
            else
            {
				GtkWidget *button_all;
				GtkWidget *button_cancel;
				GtkWidget *button_one;

				gchar *tmp_date;
				tmp_str = utils_real_get_string (gsb_data_scheduled_get_amount (scheduled_number));
				tmp_date = gsb_format_gdate (gsb_data_scheduled_get_date (scheduled_number));
				occurrences = g_strdup_printf (_("Do you want to delete just this occurrence or "
												 "the whole scheduled transaction?\n\n%s : %s [%s %s]"),
											   tmp_date,
											   gsb_data_payee_get_name (gsb_data_scheduled_get_party_number
																		(scheduled_number),
																		FALSE),
											   tmp_str,
											   gsb_data_currency_get_name (gsb_data_scheduled_get_currency_number
																		   (scheduled_number)));
				g_free (tmp_str);
				g_free(tmp_date);

				dialog = dialogue_special_no_run (GTK_MESSAGE_QUESTION,
												  GTK_BUTTONS_NONE,
												  occurrences,
												  _("Delete this scheduled transaction?"));

				button_cancel = gtk_button_new_with_label (_("Cancel"));
				gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_cancel, 2);

				button_all = gtk_button_new_with_label (_("All the occurrences"));
				gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_all, 1);

				button_one = gtk_button_new_with_label (_("Only this one"));
				gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button_one, 0);
				gtk_widget_set_can_default (button_cancel, TRUE);
				gtk_dialog_set_default_response (GTK_DIALOG (dialog), 2);

				vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

				checkbox = gtk_check_button_new_with_label (_("Keep this choice and no longer see this message?"));
				g_signal_connect (G_OBJECT (checkbox),
								  "toggled",
								  G_CALLBACK (dialogue_update_struct_message),
								  (warning+msg_no));
				gtk_box_pack_start (GTK_BOX (vbox), checkbox, TRUE, TRUE, MARGIN_BOX);
				gtk_widget_show_all (dialog);

				result = gtk_dialog_run (GTK_DIALOG (dialog));

				(warning+msg_no)->default_answer = result;
				g_free (occurrences);
				gtk_widget_destroy (dialog);
            }
        }
        else
            result = 1;

        switch (result)
        {
            case 0:
            if (gsb_scheduler_increase_scheduled (scheduled_number))
                gsb_scheduler_list_update_transaction_in_list (scheduled_number);
            break;

            case 1:
            /* !!important to remove first from the list... */
            gsb_scheduler_list_remove_transaction_from_list (scheduled_number);
			if (gsb_data_scheduled_get_split_of_scheduled (scheduled_number))
			{
				gsb_data_scheduled_remove_child_scheduled (scheduled_number);
			}
            gsb_data_scheduled_remove_scheduled (scheduled_number);
            break;
        }
    }

    gsb_scheduler_list_set_background_color (gsb_scheduler_list_get_tree_view ());

    gsb_calendar_update ();
    w_run->mise_a_jour_liste_echeances_manuelles_accueil = TRUE;

    gsb_file_set_modified (TRUE);

    return FALSE;
}

/**
 * Clone selected transaction if any.  Update user interface as well.
 *
 * \param menu_item
 * \param scheduled_number
 *
 * \return FALSE
 */
gboolean gsb_scheduler_list_clone_selected_scheduled (GtkWidget *menu_item,
													  gint *scheduled_number)
{
    gint new_scheduled_number;
    gint tmp_scheduled_number;

    if (scheduled_number == NULL)
        tmp_scheduled_number = gsb_scheduler_list_get_current_scheduled_number ();
    else
        tmp_scheduled_number = GPOINTER_TO_INT (scheduled_number);

    new_scheduled_number = gsb_data_scheduled_new_scheduled ();

    gsb_data_scheduled_copy_scheduled (tmp_scheduled_number, new_scheduled_number);

    if (gsb_data_scheduled_get_split_of_scheduled (tmp_scheduled_number))
    {
        GSList *tmp_list;

        tmp_list = g_slist_copy (gsb_data_scheduled_get_scheduled_list ());

        while (tmp_list)
        {
            gint split_scheduled_number;

            split_scheduled_number = gsb_data_scheduled_get_scheduled_number (tmp_list->data);

            if (gsb_data_scheduled_get_mother_scheduled_number (split_scheduled_number) == tmp_scheduled_number)
            {
                gint new_number;

                new_number = gsb_data_scheduled_new_scheduled ();
                gsb_data_scheduled_copy_scheduled (split_scheduled_number, new_number);
                gsb_data_scheduled_set_mother_scheduled_number (new_number, new_scheduled_number);
            }

            tmp_list = tmp_list->next;
        }
    }

    gsb_scheduler_list_fill_list ();
    gsb_scheduler_list_set_background_color (gsb_scheduler_list_get_tree_view ());
    gsb_scheduler_list_select (new_scheduled_number);

    gsb_file_set_modified (TRUE);

    return FALSE;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gboolean gsb_scheduler_list_set_largeur_col (void)
{
    gint i;
    gint width;

    for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS ; i++)
    {
        width = (scheduler_col_width[i] * (scheduler_current_tree_view_width)) / 100;
        if (width > 0)
            gtk_tree_view_column_set_fixed_width (scheduler_list_column[i], width);
    }

    return FALSE;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
GtkWidget *gsb_scheduler_list_get_toolbar (void)
{
    return scheduler_toolbar;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
GSList *gsb_scheduler_list_get_scheduled_transactions_taken (void)
{
	return scheduled_transactions_taken;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
GSList *gsb_scheduler_list_get_scheduled_transactions_to_take (void)
{
	return scheduled_transactions_to_take;
}

/**
 * Initialise le tableau des largeurs de colonnes du treeview des opérations
 *
 * \param description 	Chaine contenant la largeurs des colonnes
 * 						Si NULL utilise les donnes par défaut.
 *
 * \return
 **/
void gsb_scheduler_list_init_tab_width_col_treeview (const gchar *description)
{
    gint i;

	if (description && strlen (description))
	{
		gchar **pointeur_char;
		gint somme = 0; 			/* calcul du % de la dernière colonne */

		/* the transactions columns are xx-xx-xx-xx and we want to set in scheduler_col_width[1-2-3...] */
		pointeur_char = g_strsplit (description, "-", 0);
		if (g_strv_length (pointeur_char) != SCHEDULER_COL_VISIBLE_COLUMNS)
		{
			/* defaut value for width of columns */
			for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS ; i++)
				scheduler_col_width[i] = scheduler_col_width_init[i];
			g_strfreev (pointeur_char);

			return;
		}

		for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS -1 ; i++)
		{
			if (strlen ((const gchar *) pointeur_char[i]) == 0)
				scheduler_col_width[i] = scheduler_col_width_init[i];
			else
				scheduler_col_width[i] = utils_str_atoi (pointeur_char[i]);

			/* on n'affiche que 6 colonnes donc on laisse libre sa taille */
			if (i != SCHEDULER_COL_VISIBLE_COLUMNS -2)
				somme+= scheduler_col_width[i];
		}
		scheduler_col_width[i] = 100 - somme;
		g_strfreev (pointeur_char);

		/* si scheduler_col_width[i] est < scheduler_col_width_init[i] on reinitialise la largeur des colonnes */
		if (scheduler_col_width[i] < scheduler_col_width_init[i])
		{
			/* defaut value for width of columns */
			for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS; i++)
			{
				scheduler_col_width[i] = scheduler_col_width_init[i];
			}
		}
	}
	else
	{
		for (i = 0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS; i++)
		{
			scheduler_col_width[i] = scheduler_col_width_init[i];
		}
	}
}

/**
 * retourne une chaine formatée des largeurs de colonnes du treeview prévisions
 *
 * \param
 *
 * \return a newly allocated chain to be released
 **/
gchar *gsb_scheduler_list_get_largeur_col_treeview_to_string (void)
{
    gchar *first_string_to_free;
    gchar *second_string_to_free;
	gchar *tmp_str = NULL;
	gint i = 0;

    for (i=0 ; i < SCHEDULER_COL_VISIBLE_COLUMNS ; i++)
    {
        if (tmp_str)
        {
			first_string_to_free = tmp_str;
			second_string_to_free = utils_str_itoa (scheduler_col_width[i]);
            tmp_str = g_strconcat (first_string_to_free, "-", second_string_to_free,  NULL);
            g_free (first_string_to_free);
            g_free (second_string_to_free);
        }
        else
            tmp_str  = utils_str_itoa (scheduler_col_width[i]);
    }

	return tmp_str;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
void gsb_scheduler_list_update_tree_view (GtkWidget *tree_view)
{
	GrisbiAppConf *a_conf;

	devel_debug (NULL);
	a_conf = (GrisbiAppConf *) grisbi_app_get_a_conf ();

	gsb_scheduler_list_fill_list ();
	gsb_scheduler_list_set_background_color (tree_view);

	if (!a_conf->select_scheduled_in_list && last_scheduled_number == -1)
		last_scheduled_number = first_scheduler_list_number;

	gsb_scheduler_list_select (last_scheduled_number);

}

/**
 *
 *
 * \param
 *
 * \return
 **/
void gsb_scheduler_list_set_current_tree_view_width (gint new_tree_view_width)
{
	scheduler_current_tree_view_width = new_tree_view_width;
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
