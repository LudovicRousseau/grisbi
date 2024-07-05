/* ************************************************************************** */
/*                                                                            */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)            */
/*          2003-2008 Benjamin Drieu (bdrieu@april.org)                       */
/*                      2009-2021 Pierre Biava (grisbi@pierre.biava.name)     */
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

/**
 * \file traitement_variables.c
 * works with global variables of grisbi (initialisation...)
 */

#include "config.h"

#include "include.h"
#include <glib/gi18n.h>

/*START_INCLUDE*/
#include "traitement_variables.h"
#include "bet_data.h"
#include "bet_data_finance.h"
#include "bet_future.h"
#ifdef HAVE_GOFFICE
#include "bet_graph.h"
#endif /* HAVE_GOFFICE */
#include "bet_tab.h"
#include "categories_onglet.h"
#include "custom_list.h"
#include "export_csv.h"
#include "grisbi_win.h"
#include "gsb_bank.h"
#include "gsb_calendar.h"
#include "gsb_currency.h"
#include "gsb_data_account.h"
#include "gsb_data_archive.h"
#include "gsb_data_archive_store.h"
#include "gsb_data_bank.h"
#include "gsb_data_budget.h"
#include "gsb_data_category.h"
#include "gsb_data_currency.h"
#include "gsb_data_currency_link.h"
#include "gsb_data_fyear.h"
#include "gsb_data_import_rule.h"
#include "gsb_data_partial_balance.h"
#include "gsb_data_payee.h"
#include "gsb_data_payment.h"
#include "gsb_data_print_config.h"
#include "gsb_data_reconcile.h"
#include "gsb_data_report_amout_comparison.h"
#include "gsb_data_report.h"
#include "gsb_data_report_text_comparison.h"
#include "gsb_data_scheduled.h"
#include "gsb_data_transaction.h"
#include "gsb_form_scheduler.h"
#include "gsb_form_widget.h"
#include "gsb_fyear.h"
#include "gsb_locale.h"
#include "gsb_real.h"
#include "gsb_regex.h"
#include "gsb_report.h"
#include "gsb_rgba.h"
#include "gsb_scheduler_list.h"
#include "gsb_select_icon.h"
#include "gsb_transactions_list.h"
#include "import.h"
#include "imputation_budgetaire.h"
#include "menu.h"
#include "navigation.h"
#include "structures.h"
#include "tiers_onglet.h"
#include "transaction_model.h"
#include "utils_dates.h"
#include "utils_str.h"
#include "erreur.h"
/*END_INCLUDE*/

/*START_STATIC*/
/*END_STATIC*/

/*START_EXTERN*/
extern GtkTreeModel *	bank_list_model;
extern gint 			id_timeout;
extern GSList *			orphan_child_transactions;
extern gint 			scheduler_current_tree_view_width;
/*END_EXTERN*/

/******************************************************************************/
/* Private functions                                                          */
/******************************************************************************/
/******************************************************************************/
/* Public functions                                                           */
/******************************************************************************/
/**
 * initialisation of all the variables of grisbi
 * if some are not empty, free them before set it to NULL
 *
 * \param
 *
 * \return
 * */
void init_variables (void)
{
	GrisbiWinEtat *w_etat;
	GrisbiWinRun *w_run;

    devel_debug (NULL);

	/* raz some variables of structure w_run */
	w_run = grisbi_win_get_w_run ();
	w_run->account_number_is_0 = FALSE;
	w_run->prefs_expand_tree = TRUE;
	if (w_run->prefs_selected_row)
		g_free (w_run->prefs_selected_row);
	w_run->prefs_selected_row = g_strdup ("0:0");

	/* initialise l'ordre des pages du panneau de gauche */
    gsb_gui_navigation_init_pages_list ();

    /* if ever there is still something from the previous list,
     * erase now */
    transaction_model_set_model (NULL);

    gsb_data_account_init_variables ();
    gsb_data_transaction_init_variables ();
    gsb_data_payee_init_variables (TRUE);
    payees_init_variables_list ();
    gsb_data_category_init_variables (TRUE);
    categories_init_variables_list ();
    gsb_data_budget_init_variables (TRUE);
    budgetary_lines_init_variables_list ();
    gsb_data_report_init_variables ();
    gsb_data_report_amount_comparison_init_variables ();
    gsb_data_report_text_comparison_init_variables ();
    gsb_data_scheduled_init_variables ();
    gsb_scheduler_list_init_variables ();
    gsb_data_currency_init_variables ();
    gsb_data_currency_link_init_variables ();
    gsb_data_fyear_init_variables ();
    gsb_data_bank_init_variables ();
    gsb_data_reconcile_init_variables ();
    gsb_data_payment_init_variables ();
    gsb_data_archive_init_variables ();
    gsb_data_archive_store_init_variables ();
    gsb_data_import_rule_init_variables ();
    gsb_import_associations_init_variables ();
    gsb_data_partial_balance_init_variables ();

    gsb_currency_init_variables ();
    gsb_fyear_init_variables ();
    gsb_report_init_variables ();
    gsb_regex_init_variables ();
	gsb_transactions_list_free_titles_tips_col_list_ope ();

    gsb_data_print_config_init ();

    /* no bank in memory for now */
    gsb_bank_free_combo_list_model ();

	w_run = (GrisbiWinRun *) grisbi_win_get_w_run ();
    w_run->mise_a_jour_liste_comptes_accueil = FALSE;
    w_run->mise_a_jour_liste_echeances_manuelles_accueil = FALSE;
    w_run->mise_a_jour_liste_echeances_auto_accueil = FALSE;
    w_run->mise_a_jour_soldes_minimaux = FALSE;
    w_run->mise_a_jour_fin_comptes_passifs = FALSE;

    orphan_child_transactions = NULL;

	/* raz variables of  w_etat */
	w_etat = (GrisbiWinEtat *) grisbi_win_get_w_etat ();
    w_etat->affichage_echeances = SCHEDULER_PERIODICITY_ONCE_VIEW;
    w_etat->affichage_echeances_perso_nb_libre = 0;
    w_etat->affichage_echeances_perso_j_m_a = PERIODICITY_DAYS;

	/* raz variables of etat */
    if (w_etat->name_logo && strlen (w_etat->name_logo))
        g_free (w_etat->name_logo);
    w_etat->name_logo = NULL;
    w_etat->utilise_logo = 1;

    gsb_select_icon_init_logo_variables ();

    w_etat->retient_affichage_par_compte = 0;
	w_etat->use_icons_file_dir = FALSE;

    /* reconcile (etat) */
    w_run->reconcile_account_number = -1;
    g_free (w_run->reconcile_final_balance);
    if (w_run->reconcile_new_date)
        g_date_free (w_run->reconcile_new_date);
    w_run->reconcile_final_balance = NULL;
    w_run->reconcile_new_date = NULL;

    gsb_transactions_list_set_current_tree_view_width (0);
	gsb_scheduler_list_set_current_tree_view_width (0);

	/* initialisations des tableaux de la liste des opérations */
    gsb_transactions_list_init_tab_affichage_ope (NULL);

	/* by default, the display of lines is 1, 1-2, 1-2-3 */
    w_run->display_one_line = 0;
    w_run->display_two_lines = 0;
    w_run->display_three_lines = 0;

    w_etat->import_files_nb_days = 2;
    w_etat->get_fyear_by_value_date = FALSE;

    /* init default combofix values */
    w_etat->combofix_mixed_sort = FALSE;
    w_etat->combofix_case_sensitive = FALSE;
    w_etat->combofix_force_payee = FALSE;
    w_etat->combofix_force_category = FALSE;

    /* the main notebook is set to NULL,
     * important because it's the checked variable in a new file
     * to know if the widgets are created or not */
    grisbi_win_free_general_notebook ();

	/* free account_property_page */
	grisbi_win_free_account_property_page ();

    /* defaut value for width and align of columns */
    gsb_transactions_list_init_tab_align_col_treeview (NULL);
    gsb_transactions_list_init_tab_width_col_treeview (NULL);
    gsb_scheduler_list_init_tab_width_col_treeview (NULL);

    gsb_gui_navigation_init_tree_view ();

    /* free the form */
    gsb_form_widget_free_list ();
    gsb_form_scheduler_free_list ();
	gsb_date_free_last_date ();		/* fix crash bug 2236 */

		/* divers */
	w_etat->affichage_commentaire_echeancier = 0;	/* RAZ option utile si chargement d'un deuxième fichier */
    w_etat->get_fyear_by_value_date = 0;           /* By default use transaction-date */

    /* remove the timeout if necessary */
    if (id_timeout)
    {
    g_source_remove (id_timeout);
    id_timeout = 0;
   }

    /* initializes the variables for the estimate balance module */
    /* création de la liste des données à utiliser dans le tableau de résultats */
    bet_data_variables_init ();
    /* initialisation des boites de dialogue */
    bet_future_initialise_dialog (TRUE);
    w_etat->bet_debut_period = 1;
    /* defaut value for width of columns */
    bet_array_init_largeur_col_treeview (NULL);

    bet_data_finance_data_simulator_init ();
	bet_data_loan_delete_all_loans ();

#ifdef HAVE_GOFFICE
    bet_graph_set_options_variables (NULL);
#endif /* HAVE_GOFFICE */

}

/**
 * Free allocations of grisbi variables
 *
 * */
void free_variables (void)
{
	GrisbiWinEtat *w_etat;

	devel_debug (NULL);
	w_etat = grisbi_win_get_w_etat ();

	/* free functions */
    gsb_data_archive_init_variables ();
    gsb_data_archive_store_init_variables ();
    gsb_data_bank_init_variables ();
    gsb_data_budget_init_variables (FALSE);
    gsb_data_category_init_variables (FALSE);
    gsb_data_currency_init_variables ();
    gsb_data_currency_link_init_variables ();
    gsb_data_fyear_init_variables ();
    gsb_data_import_rule_init_variables ();
    gsb_data_partial_balance_init_variables ();
    gsb_data_payee_init_variables (FALSE);
    gsb_data_payment_init_variables ();
    gsb_data_print_config_free ();
    gsb_data_reconcile_init_variables ();
    gsb_data_report_amount_comparison_init_variables ();
    gsb_data_report_init_variables ();
    gsb_data_report_text_comparison_init_variables ();
    gsb_data_scheduled_init_variables ();
	gsb_data_transaction_init_variables ();
    gsb_currency_init_variables ();
    gsb_fyear_init_variables ();
	gsb_gui_navigation_free_pages_list ();
    gsb_import_associations_init_variables ();
    gsb_report_init_variables ();
	gsb_regex_destroy ();
    gsb_scheduler_list_init_variables ();
	gsb_select_icon_init_logo_variables ();
	gsb_transactions_list_free_titles_tips_col_list_ope ();

	/* free account list */
	gsb_data_account_init_variables ();

	/* free the form */
    gsb_form_widget_free_list_without_widgets ();
    gsb_form_scheduler_free_list ();

	/* variables generales */
	if (w_etat->accounting_entity)
		g_free (w_etat->accounting_entity);
	if (w_etat->adr_common)
		g_free (w_etat->adr_common);
	if (w_etat->adr_secondary)
		g_free (w_etat->adr_secondary);
	if (w_etat->date_format)
		g_free (w_etat->date_format);

	/* reset csv separator */
	gsb_csv_export_set_csv_separator (NULL);
	if (w_etat->csv_separator)
		g_free (w_etat->csv_separator);

	/* raz variables of etat */
	if (w_etat->name_logo && strlen (w_etat->name_logo))
		g_free (w_etat->name_logo);

    /* free the variables for the estimate balance module */
    bet_data_variables_free ();
    bet_future_initialise_dialog (FALSE);
    bet_array_init_largeur_col_treeview (NULL);
	bet_data_loan_delete_all_loans ();

#ifdef HAVE_GOFFICE
    bet_graph_free_options_variables ();
#endif /* HAVE_GOFFICE */
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
