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
#include "prefs_page_form_completion.h"
#include "grisbi_app.h"
#include "gsb_category.h"
#include "gsb_data_form.h"
#include "gsb_file.h"
#include "gsb_form.h"
#include "gsb_form_widget.h"
#include "imputation_budgetaire.h"
#include "gtk_combofix.h"
#include "prefs_page_metatree.h"
#include "transaction_list.h"
#include "structures.h"
#include "utils_prefs.h"
#include "utils_str.h"
#include "erreur.h"

/*END_INCLUDE*/

/*START_EXTERN*/
/*END_EXTERN*/

typedef struct _PrefsPageFormCompletionPrivate   PrefsPageFormCompletionPrivate;

struct _PrefsPageFormCompletionPrivate
{
	GtkWidget *			vbox_form_completion;

    GtkWidget *			checkbutton_combofix_case_sensitive;

	GtkWidget *			spinbutton_completion_minimum_key_length;

	GtkWidget *			hbox_automatic_completion_payee;
	GtkWidget *			checkbutton_automatic_completion_payee;
	GtkWidget *			checkbutton_automatic_erase_credit_debit;
	GtkWidget *			checkbutton_automatic_recover_splits;
	GtkWidget *			checkbutton_limit_completion_to_current_account;

	GtkWidget *			checkbutton_combofix_force_payee;
	GtkWidget *			checkbutton_combofix_mixed_sort;

	GtkWidget *			checkbutton_combofix_force_category;

};

G_DEFINE_TYPE_WITH_PRIVATE (PrefsPageFormCompletion, prefs_page_form_completion, GTK_TYPE_BOX)

/******************************************************************************/
/* Private functions                                                          */
/******************************************************************************/
/**
 * called when we change a parameter of the combofix configuration
 * update the combofix in the form if they exists
 * as we don't know what was changed, update all the parameter (not a problem
 * because very fast)
 * at this level, the w_etat->___ variable has already been changed
 *
 * \param
 *
 * \return
 **/
static void prefs_page_form_completion_update_combofix (GtkWidget *checkbutton,
														gpointer metatree_content_ptr)
{
    GtkWidget *combofix;
	gint metatree_content;
	GrisbiWinEtat *w_etat;

	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();


	metatree_content = GPOINTER_TO_INT (metatree_content_ptr);
	switch (metatree_content)
	{
		case METATREE_PAYEE:
			combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_PARTY);
			if (combofix && GTK_IS_COMBOFIX (combofix))
			{
				prefs_page_metatree_sensitive_widget ("metatree_unarchived_payees", w_etat->combofix_force_payee);
				gtk_combofix_set_properties (combofix);
			}
			break;

		case METATREE_CATEGORY:
			combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_CATEGORY);
			if (combofix && GTK_IS_COMBOFIX (combofix))
			{
				gtk_combofix_set_properties (combofix);
				gsb_category_update_combofix (TRUE);
			}
			break;

		case METATREE_BUDGET:
			combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_BUDGET);
			if (combofix && GTK_IS_COMBOFIX (combofix))
			{
				gtk_combofix_set_properties (combofix);
				gsb_budget_update_combofix (FALSE);
			}
			break;

		default:
			combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_PARTY);
			if (combofix && GTK_IS_COMBOFIX (combofix))
				gtk_combofix_set_properties (combofix);

			combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_CATEGORY);
			if (combofix && GTK_IS_COMBOFIX (combofix))
				gtk_combofix_set_properties (combofix);

			combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_BUDGET);
			if (combofix && GTK_IS_COMBOFIX (combofix))
				gtk_combofix_set_properties (combofix);
	}
}

/**
 * Appellée lorsqu'on coche la case "Automatic filling transactions from payee"
 *
 * \param
 * \param
 *
 * \return
 **/
static void prefs_page_form_completion_checkbutton_automatic_completion_payee_toggle (GtkWidget *checkbutton,
																					  PrefsPageFormCompletion *page)
{
 	PrefsPageFormCompletionPrivate *priv;

	priv = prefs_page_form_completion_get_instance_private (page);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton)))
    {
        gtk_widget_set_sensitive (priv->hbox_automatic_completion_payee, TRUE);
    }
    else
    {
		gtk_widget_set_sensitive (priv->hbox_automatic_completion_payee, FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_automatic_erase_credit_debit),
									  FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_automatic_recover_splits),
									  FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_limit_completion_to_current_account),
									  FALSE);
    }
}

/**
 * Appellée lorsqu'on modifie le spinbutton completion_minimum_key_length
 *
 * \param
 * \param
 *
 * \return
 **/
static void prefs_page_form_completion_spinbutton_completion_minimum_key_length_change (GtkWidget *spinbutton,
																						gpointer null)
{
    GtkWidget *combofix;
	GtkWidget *entry;
	GtkEntryCompletion *completion;
	GrisbiAppConf *a_conf;

	a_conf = grisbi_app_get_a_conf ();
	combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_PARTY);
	if (combofix && GTK_IS_COMBOFIX (combofix))
	{
		entry = gtk_combofix_get_entry (GTK_COMBOFIX (combofix));
		completion = gtk_entry_get_completion (GTK_ENTRY (entry));
		gtk_entry_completion_set_minimum_key_length (completion, a_conf->completion_minimum_key_length);
	}
	combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_CATEGORY);
	if (combofix && GTK_IS_COMBOFIX (combofix))
	{
		entry = gtk_combofix_get_entry (GTK_COMBOFIX (combofix));
		completion = gtk_entry_get_completion (GTK_ENTRY (entry));
		gtk_entry_completion_set_minimum_key_length (completion, a_conf->completion_minimum_key_length);
	}
	combofix = gsb_form_widget_get_widget (TRANSACTION_FORM_BUDGET);
	if (combofix && GTK_IS_COMBOFIX (combofix))
	{
		entry = gtk_combofix_get_entry (GTK_COMBOFIX (combofix));
		completion = gtk_entry_get_completion (GTK_ENTRY (entry));
		gtk_entry_completion_set_minimum_key_length (completion, a_conf->completion_minimum_key_length);
	}
}

/**
 * Création de la page de gestion des form_completion
 *
 * \param prefs
 *
 * \return
 */
static void prefs_page_form_completion_setup_form_completion_page (PrefsPageFormCompletion *page)
{
	GtkWidget *head_page;
	gboolean is_loading;
	GrisbiAppConf *a_conf;
	GrisbiWinEtat *w_etat;
	PrefsPageFormCompletionPrivate *priv;

	devel_debug (NULL);

	priv = prefs_page_form_completion_get_instance_private (page);
	is_loading = grisbi_win_file_is_loading ();
	a_conf = grisbi_app_get_a_conf ();
	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();

	/* On récupère le nom de la page */
	head_page = utils_prefs_head_page_new_with_title_and_icon (_("Form completion"), "gsb-form-32.png");
	gtk_box_pack_start (GTK_BOX (priv->vbox_form_completion), head_page, FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (priv->vbox_form_completion), head_page, 0);

	/* initialise le bouton nombre minimal de caractères avant propositions dans les champs de saisie */
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->spinbutton_completion_minimum_key_length),
							   a_conf->completion_minimum_key_length);

	/* set active widgets if is_loading = TRUE else set sensitive widget*/
	if (is_loading)
	{
		/* set w_etat->combofix_mixed_sort */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_combofix_mixed_sort),
									  w_etat->combofix_mixed_sort);
		/* set w_etat->combofix_case_sensitive */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_combofix_case_sensitive),
									  w_etat->combofix_case_sensitive);
		/* set w_etat->combofix_force_payee */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_combofix_force_payee),
									  w_etat->combofix_force_payee);
		/* set w_etat->combofix_force_category */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_combofix_force_category),
									  w_etat->combofix_force_category);
	}
	else
	{
		gtk_widget_set_sensitive (priv->checkbutton_combofix_mixed_sort, FALSE);
		gtk_widget_set_sensitive (priv->checkbutton_combofix_case_sensitive, FALSE);
		gtk_widget_set_sensitive (priv->checkbutton_combofix_force_payee, FALSE);
		gtk_widget_set_sensitive (priv->checkbutton_combofix_force_category, FALSE);
	}

	/* set a_conf->automatic_completion_payee */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_automatic_completion_payee),
								  a_conf->automatic_completion_payee);
	if (!a_conf->automatic_completion_payee)
	{
		gtk_widget_set_sensitive (priv->hbox_automatic_completion_payee, FALSE);
	}
	else
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_automatic_erase_credit_debit),
									  a_conf->automatic_erase_credit_debit);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_automatic_recover_splits),
									  a_conf->automatic_recover_splits);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->checkbutton_limit_completion_to_current_account),
									  a_conf->limit_completion_to_current_account);
	}

    /* Connect signal checkbutton_combofix_case_sensitive */
    g_signal_connect (priv->checkbutton_combofix_case_sensitive,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &w_etat->combofix_case_sensitive);

    g_signal_connect_after (priv->checkbutton_combofix_case_sensitive,
							"toggled",
							G_CALLBACK (prefs_page_form_completion_update_combofix),
							GINT_TO_POINTER (-1));

	/* callback for spinbutton_completion_minimum_key_length_ */
    g_signal_connect (priv->spinbutton_completion_minimum_key_length,
					  "value-changed",
					  G_CALLBACK (utils_prefs_spinbutton_changed),
					  &a_conf->completion_minimum_key_length);

    g_signal_connect_after (priv->spinbutton_completion_minimum_key_length,
							"value-changed",
							G_CALLBACK (prefs_page_form_completion_spinbutton_completion_minimum_key_length_change),
							NULL);

	/* Connect signal checkbutton_automatic_completion_payee */
    g_signal_connect (priv->checkbutton_automatic_completion_payee,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &a_conf->automatic_completion_payee);

    g_signal_connect_after (priv->checkbutton_automatic_completion_payee,
							"toggled",
							G_CALLBACK (prefs_page_form_completion_checkbutton_automatic_completion_payee_toggle),
							page);

    /* Connect signal checkbutton_checkbutton_automatic_erase_credit_debit */
    g_signal_connect (priv->checkbutton_automatic_erase_credit_debit,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &a_conf->automatic_erase_credit_debit);

	/* Connect signal checkbutton_checkbutton_automatic_recover_splits */
	g_signal_connect (priv->checkbutton_automatic_recover_splits,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &a_conf->automatic_recover_splits);

    /* Connect signal checkbutton_checkbutton_limit_completion_to_current_account */
    g_signal_connect (priv->checkbutton_limit_completion_to_current_account,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &a_conf->limit_completion_to_current_account);

    /* Connect signal checkbutton_combofix_force_payee */
    g_signal_connect (priv->checkbutton_combofix_force_payee,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &w_etat->combofix_force_payee);

    g_signal_connect_after (priv->checkbutton_combofix_force_payee,
							"toggled",
							G_CALLBACK (prefs_page_form_completion_update_combofix),
							GINT_TO_POINTER (METATREE_PAYEE));

    /* Connect signal checkbutton_combofix_mixed_sort */
    g_signal_connect (priv->checkbutton_combofix_mixed_sort,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &w_etat->combofix_mixed_sort);

    g_signal_connect_after (priv->checkbutton_combofix_mixed_sort,
							"toggled",
							G_CALLBACK (prefs_page_form_completion_update_combofix),
							GINT_TO_POINTER (METATREE_CATEGORY));

    /* Connect signal checkbutton_combofix_force_category */
    g_signal_connect (priv->checkbutton_combofix_force_category,
					  "toggled",
					  G_CALLBACK (utils_prefs_page_checkbutton_changed),
					  &w_etat->combofix_force_category);

    g_signal_connect_after (priv->checkbutton_combofix_force_category,
							"toggled",
							G_CALLBACK (prefs_page_form_completion_update_combofix),
							GINT_TO_POINTER (METATREE_CATEGORY));

}

/******************************************************************************/
/* Fonctions propres à l'initialisation des fenêtres                          */
/******************************************************************************/
static void prefs_page_form_completion_init (PrefsPageFormCompletion *page)
{
	gtk_widget_init_template (GTK_WIDGET (page));

	prefs_page_form_completion_setup_form_completion_page (page);
}

static void prefs_page_form_completion_dispose (GObject *object)
{
	G_OBJECT_CLASS (prefs_page_form_completion_parent_class)->dispose (object);
}

static void prefs_page_form_completion_class_init (PrefsPageFormCompletionClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = prefs_page_form_completion_dispose;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
												 "/org/gtk/grisbi/prefs/prefs_page_form_completion.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, vbox_form_completion);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_combofix_case_sensitive);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, spinbutton_completion_minimum_key_length);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_automatic_completion_payee);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, hbox_automatic_completion_payee);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_automatic_erase_credit_debit);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_automatic_recover_splits);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_limit_completion_to_current_account);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_combofix_force_payee);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_combofix_mixed_sort);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), PrefsPageFormCompletion, checkbutton_combofix_force_category);
}

/******************************************************************************/
/* Public functions                                                           */
/******************************************************************************/
PrefsPageFormCompletion *prefs_page_form_completion_new (GrisbiPrefs *win)
{
  return g_object_new (PREFS_PAGE_FORM_COMPLETION_TYPE, NULL);
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

