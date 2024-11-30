/* ************************************************************************** */
/*                                                                            */
/*     Copyright (C) 2007 Dominique Parisot                                   */
/*          zionly@free.org                                                   */
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

#include "config.h"

#include "include.h"
#include <glib/gi18n.h>

/*START_INCLUDE*/
#include "bet_data.h"
#include "bet_data_finance.h"
#include "bet_future.h"
#include "bet_hist.h"
#include "bet_tab.h"
#include "dialog.h"
#include "grisbi_win.h"
#include "gsb_data_account.h"
#include "gsb_data_budget.h"
#include "gsb_data_category.h"
#include "gsb_data_mix.h"
#include "gsb_data_partial_balance.h"
#include "gsb_data_scheduled.h"
#include "gsb_data_transaction.h"
#include "gsb_file.h"
#include "gsb_file_save.h"
#include "gsb_scheduler_list.h"
#include "gsb_transactions_list.h"
#include "navigation.h"
#include "structures.h"
#include "traitement_variables.h"
#include "utils_dates.h"
#include "utils_real.h"
#include "utils_str.h"
#include "erreur.h"
/*END_INCLUDE*/


/*START_STATIC*/
/*END_STATIC*/


/*START_EXTERN*/
/*END_EXTERN*/


/* pointeurs définis en fonction du type de données catégories ou IB */
gint 	(*ptr_div)			(gint transaction_num, gboolean is_transaction);
gint 	(*ptr_sub_div)		(gint transaction_num, gboolean is_transaction);
gint 	(*ptr_type)			(gint no_div);
gchar*	(*ptr_div_name)		(gint div_num, gint sub_div, const gchar *return_value_error);


/* liste des div et sub_div cochées dans la vue des divisions */
static GHashTable *		bet_hist_list;

/** the hashtable which contains all the bet_future structures */
static GHashTable *		bet_futur_list;
static gint				futur_number;

/** the hashtable for account_balance */
static GHashTable *		bet_transfert_list;
static gint				transfert_number;

/******************************************************************************/
/* Private functions                                                          */
/******************************************************************************/
/**
 *
 *
 * \param
 *
 * \return
 **/
static gint bet_data_get_current_bet_page (void)
{
	GtkWidget *account_page;
	gint current_page;

	account_page = grisbi_win_get_account_page ();
	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (account_page));

	return current_page;
}

/**
 * retourne la clef de recherche de la division passée en paramètre.
 *
 * \param
 * \param
 *
 * \return
 **/
static gchar *bet_data_get_key (gint account_number,
								gint div_number)
{
	gchar *key;
	gchar *str_to_free; /* only to avoid memory leaks */
	gchar *div_number_str;

	div_number_str = utils_str_itoa (div_number);

	if (account_number == 0)
		key = g_strconcat ("0:", div_number_str, NULL);
	else
	{
		str_to_free = utils_str_itoa (account_number);
		key = g_strconcat (str_to_free, ":", div_number_str, NULL);

		g_free (str_to_free);
	}

	g_free (div_number_str);

	return key;
}

/* FUTUR_DATA */
/**
 *
 *
 * \param
 *
 * \return
 **/
static FuturData *bet_data_future_copy_struct (FuturData *sfd)
{
	FuturData *new_sfd;

	new_sfd = bet_data_future_struct_init ();

	if (!new_sfd)
	{
		dialogue_error_memory ();

		return NULL;
	}

	new_sfd->number = sfd->number;
	new_sfd->account_number = sfd->account_number;

	if (g_date_valid (sfd->date))
		new_sfd->date = gsb_date_copy (sfd->date);
	else
		new_sfd->date = NULL;

	new_sfd->amount = gsb_real_new (sfd->amount.mantissa, sfd->amount.exponent);
	new_sfd->fyear_number = sfd->fyear_number;
	new_sfd->payment_number = sfd->payment_number;

	new_sfd->party_number = sfd->party_number;
	new_sfd->is_transfert = sfd->is_transfert;
	new_sfd->account_transfert = sfd->account_transfert;
	new_sfd->category_number = sfd->category_number;
	new_sfd->sub_category_number = sfd->sub_category_number;
	new_sfd->budgetary_number = sfd->budgetary_number;
	new_sfd->sub_budgetary_number = sfd->sub_budgetary_number;
	new_sfd->notes = g_strdup (sfd->notes);

	new_sfd->frequency = sfd->frequency;
	new_sfd->user_interval = sfd->user_interval;
	new_sfd->user_entry = sfd->user_entry;

	if (sfd->limit_date && g_date_valid (sfd->limit_date))
		new_sfd->limit_date = gsb_date_copy (sfd->limit_date);
	else
		new_sfd->limit_date = NULL;

	new_sfd->mother_row = sfd->mother_row;

	return new_sfd;
}

/**
 * find and return the next date after the given date for the given futur data
 *
 *
 * \param FuturData
 * \param date 			the current date, we want the next one after that one
 * \param
 *
 * \return a newly allocated date, the next date or NULL if over the limit
 **/
static GDate *bet_data_futur_get_next_date (FuturData *sfd,
											const GDate *date,
											const GDate *date_max)
{
	GDate *return_date;

	if (!sfd || !sfd->frequency || !date || !g_date_valid (date))
		return NULL;

	/* we don't change the initial date */
	return_date = gsb_date_copy (date);

	switch (sfd->frequency)
	{
		case SCHEDULER_PERIODICITY_ONCE_VIEW:

			return NULL;
			break;

		case SCHEDULER_PERIODICITY_WEEK_VIEW:
			g_date_add_days (return_date, 7);
			break;

		case SCHEDULER_PERIODICITY_MONTH_VIEW:
			g_date_add_months (return_date, 1);
			break;

		case SCHEDULER_PERIODICITY_TWO_MONTHS_VIEW:
			g_date_add_months (return_date, 2);
			break;

		case SCHEDULER_PERIODICITY_TRIMESTER_VIEW:
			g_date_add_months (return_date, 3);
			break;

		case SCHEDULER_PERIODICITY_YEAR_VIEW:
			g_date_add_years (return_date, 1);
			break;

		case SCHEDULER_PERIODICITY_CUSTOM_VIEW:
			if (sfd->user_entry <= 0)
			{
				g_date_free (return_date);

				return NULL;
			}

			switch (sfd->user_interval)
			{
				case PERIODICITY_DAYS:
					g_date_add_days (return_date, sfd->user_entry);
					break;

				case PERIODICITY_WEEKS:
					g_date_add_days (return_date, sfd->user_entry * 7);
					break;

				case PERIODICITY_MONTHS:
					g_date_add_months (return_date, sfd->user_entry);
					break;

				case PERIODICITY_YEARS:
					g_date_add_years (return_date, sfd->user_entry);
					break;
			}
			break;
	}

	if (sfd->limit_date)
	{

		if (g_date_compare (return_date, sfd->limit_date) > 0)
		{
			g_date_free (return_date);
			return_date = NULL;
		}
	}
	else
	{
		if (g_date_compare (return_date, date_max) > 0)
		{
			g_date_free (return_date);
			return_date = NULL;
		}
	}

	return (return_date);
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
static void bet_data_future_set_account_number (gpointer key,
												gpointer value,
												gpointer user_data)
{
	FuturData *sfd = (FuturData *) value;

	if (sfd->account_number == 0)
		sfd->account_number = GPOINTER_TO_INT (user_data);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
static void bet_data_future_set_max_number (gint number)
{
	if (number > futur_number)
		futur_number = number;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
void bet_data_future_struct_free (FuturData *sfd)
{
	if (sfd->date)
		g_date_free (sfd->date);
	if (sfd->limit_date)
		g_date_free (sfd->limit_date);
	if (sfd->notes)
		g_free (sfd->notes);

	g_free (sfd);
}

/* HISTORIQUE_DATA */
/**
 *
 *
 * \param
 * \param
 * \param
 *
 * \return
 **/
static void bet_data_hist_set_account_number (gpointer key,
											  gpointer value,
											  gpointer user_data)
{
	HistData *shd = (HistData *) value;

	if (shd->account_nb == 0)
		shd->account_nb = GPOINTER_TO_INT (user_data);
}

/**
 * Ajout des données à la division et création de la sous division si elle
 * n'existe pas.
 *
 * \param
 * \param
 * \param
 * \param
 * \param
 *
 * \return
 **/
static gboolean bet_data_hist_update_data (HistData *shd,
										   gint transaction_number,
										   gint sub_div,
										   gint type_de_transaction,
										   GsbReal amount)
{
	BetRange *sbr = (BetRange*) shd->sbr;
	gchar *key;
	HistData *tmp_shd = NULL;

	switch (type_de_transaction)
	{
		case 0:
			sbr-> current_balance = gsb_real_add (sbr->current_balance, amount);
			break;
		case 1:
			sbr-> current_fyear = gsb_real_add (sbr->current_fyear, amount);
			break;
		case 2:
			sbr-> current_balance = gsb_real_add (sbr->current_balance, amount);
			sbr-> current_fyear = gsb_real_add (sbr->current_fyear, amount);
			break;
	}

	if (sub_div < 1)
		return FALSE;

	key = utils_str_itoa (sub_div);
	if ((tmp_shd = g_hash_table_lookup (shd->sub_div_list, key)))
	{
		bet_data_hist_update_data (tmp_shd, transaction_number, -1, type_de_transaction, amount);
		g_free (key);
	}
	else
	{
		tmp_shd = bet_data_hist_struct_init ();
		tmp_shd->div_number = sub_div;
		tmp_shd->account_nb = gsb_data_transaction_get_account_number (transaction_number);
		bet_data_hist_update_data (tmp_shd, transaction_number, -1, type_de_transaction, amount);
		g_hash_table_insert (shd->sub_div_list, key, tmp_shd);
	}

	return FALSE;
}

/* TRANSFERT_DATA */
/**
 *
 *
 * \param
 * \param
 * \param
 *
 * \return
 **/
static void bet_data_transfert_set_account_number (gpointer key,
												   gpointer value,
												   gpointer user_data)
{
	TransfertData *transfert = (TransfertData *) value;

	if (transfert->main_account_number == 0)
		transfert->main_account_number = GPOINTER_TO_INT (user_data);
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
void bet_data_bet_range_struct_free (BetRange *sbr)
{
	g_free (sbr);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
BetRange *bet_data_bet_range_struct_init (void)
{
	BetRange	*sbr;

	sbr = g_malloc0 (sizeof (BetRange));
	sbr->first_pass = TRUE;
	sbr->current_fyear = null_real;
	sbr->current_balance = null_real;

	return sbr;
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
gchar *bet_data_get_div_name (gint div_num,
							  gint sub_div,
							  const gchar *return_value_error)
{
	return ptr_div_name (div_num, sub_div, NULL);
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
gint bet_data_get_div_number (gint transaction_number,
							  gboolean is_transaction)
{
	return ptr_div (transaction_number, is_transaction);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gint bet_data_get_div_type (gint div_number)
{
	return ptr_type (div_number);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gint bet_data_get_selected_currency (void)
{
	gint selected_account;
	gint currency_number;

	selected_account = gsb_gui_navigation_get_current_account ();
	if (selected_account == -1)
		return 0;

	currency_number = gsb_data_account_get_currency (selected_account);

	return currency_number;
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
gchar *bet_data_get_str_amount_in_account_currency (GsbReal amount,
													gint account_number,
													gint line_number,
													gint origin)
{
	gchar *str_amount = NULL;
	gint account_currency;
	gint floating_point;
	GsbReal new_amount = {0, 0};

	account_currency = gsb_data_account_get_currency (account_number);
	floating_point = gsb_data_account_get_currency_floating_point (account_number);

	switch (origin)
	{
		case SPP_ORIGIN_TRANSACTION :
			new_amount = gsb_data_transaction_get_adjusted_amount_for_currency (line_number,
																				account_currency,
																				floating_point);
		break;
		case SPP_ORIGIN_SCHEDULED :
			new_amount = gsb_data_scheduled_get_adjusted_amount_for_currency (line_number,
																			  account_currency,
																			  floating_point);

		break;
		case SPP_ORIGIN_ACCOUNT :
			if (account_currency == line_number || amount.mantissa == 0)
			{
				new_amount.mantissa = amount.mantissa;
				new_amount.exponent = amount.exponent;
			}
			else
				new_amount = gsb_transfert_get_str_amount (amount,
														   account_currency,
														   line_number,
														   floating_point);

		break;
	}

	str_amount = utils_real_get_string (new_amount);

	return str_amount;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
GPtrArray *bet_data_get_strings_to_save (void)
{
	GPtrArray *tab = NULL;
	GSList *tmp_list;
	gchar *tmp_str = NULL;
	GHashTableIter iter;
	gpointer key, value;

	if (g_hash_table_size (bet_hist_list) == 0
		&& g_hash_table_size (bet_futur_list) == 0
		&& g_hash_table_size (bet_transfert_list) == 0
		&& g_slist_length (bet_data_loan_get_loan_list()) == 0)
	{
		return NULL;
	}

	/* au moins 1 table a des éléments */
	tab = g_ptr_array_new ();

	if (g_hash_table_size (bet_hist_list) > 0)
	{
		g_hash_table_iter_init (&iter, bet_hist_list);
		while (g_hash_table_iter_next (&iter, &key, &value))
		{
			HistData *shd = (HistData*) value;

			if (g_hash_table_size (shd->sub_div_list) == 0)
			{
				gchar *str_to_free;

				str_to_free = gsb_real_safe_real_to_string (shd->amount,
															gsb_data_account_get_currency_floating_point
															(shd->account_nb));
				tmp_str = g_markup_printf_escaped ("\t<Bet_historical Nb=\"%d\" Ac=\"%d\" "
												   "Ori=\"%d\" Div=\"%d\" Edit=\"%d\" Damount=\"%s\" SDiv=\"%d\" "
												   "SEdit=\"%d\" SDamount=\"%s\" />\n",
												   tab->len + 1,
												   shd->account_nb,
												   shd->origin,
												   shd->div_number,
												   shd->div_edited,
												   str_to_free,
												   0,
												   0,
												   "0.00");

				g_ptr_array_add (tab, tmp_str);
				g_free (str_to_free);
			}
			else
			{
				GHashTableIter new_iter;

				g_hash_table_iter_init (&new_iter, shd->sub_div_list);
				while (g_hash_table_iter_next (&new_iter, &key, &value))
				{
					HistData *sub_shd = (HistData*) value;
					gchar *str_to_free_1;
					gchar *str_to_free_2;
					gint floating_point;

					floating_point = gsb_data_account_get_currency_floating_point (shd->account_nb);
					str_to_free_1 = gsb_real_safe_real_to_string (shd->amount, floating_point);
					str_to_free_2 = gsb_real_safe_real_to_string (sub_shd->amount, floating_point);
					tmp_str = g_markup_printf_escaped ("\t<Bet_historical Nb=\"%d\" Ac=\"%d\" "
													   "Ori=\"%d\" Div=\"%d\" Edit=\"%d\" Damount=\"%s\" SDiv=\"%d\" "
													   "SEdit=\"%d\" SDamount=\"%s\" />\n",
													   tab->len + 1,
													   shd->account_nb,
													   shd->origin,
													   shd->div_number,
													   shd->div_edited,
													   str_to_free_1,
													   sub_shd->div_number,
													   sub_shd->div_edited,
													   str_to_free_2);

					g_ptr_array_add (tab, tmp_str);
					g_free (str_to_free_1);
					g_free (str_to_free_2);
				}
			}
		}
	}


	if (g_hash_table_size (bet_futur_list) > 0)
	{
		g_hash_table_iter_init (&iter, bet_futur_list);
		while (g_hash_table_iter_next (&iter, &key, &value))
		{
			FuturData *sfd = (FuturData*) value;
			gchar *amount;
			gchar *date;
			gchar *limit_date;

			/* set the real */
			amount = gsb_real_safe_real_to_string (sfd->amount,
							gsb_data_account_get_currency_floating_point (sfd->account_number));

			/* set the dates */
			date = gsb_format_gdate_safe (sfd->date);
			limit_date = gsb_format_gdate_safe (sfd->limit_date);

			tmp_str = g_markup_printf_escaped ("\t<Bet_future Nb=\"%d\" Dt=\"%s\" Ac=\"%d\" "
											   "Am=\"%s\" Pa=\"%d\" IsT=\"%d\" Tra=\"%d\" Ca=\"%d\" Sca=\"%d\" "
											   "Pn=\"%d\" Fi=\"%d\" Bu=\"%d\" Sbu=\"%d\" No=\"%s\" Au=\"0\" "
											   "Pe=\"%d\" Pei=\"%d\" Pep=\"%d\" Dtl=\"%s\" Mo=\"%d\" />\n",
											   sfd->number,
											   my_safe_null_str (date),
											   sfd->account_number,
											   my_safe_null_str (amount),
											   sfd->party_number,
											   sfd->is_transfert,
											   sfd->account_transfert,
											   sfd->category_number,
											   sfd->sub_category_number,
											   sfd->payment_number,
											   sfd->fyear_number,
											   sfd->budgetary_number,
											   sfd->sub_budgetary_number,
											   my_safe_null_str (sfd->notes),
											   sfd->frequency,
											   sfd->user_interval,
											   sfd->user_entry,
											   my_safe_null_str (limit_date),
											   sfd->mother_row);

			g_ptr_array_add (tab, tmp_str);

			g_free (amount);
			g_free (date);
			g_free (limit_date);
		}
	}

	if (g_hash_table_size (bet_transfert_list) > 0)
	{
		g_hash_table_iter_init (&iter, bet_transfert_list);
		while (g_hash_table_iter_next (&iter, &key, &value))
		{
			TransfertData *std = (TransfertData*) value;
			gchar *date_debit;
			gchar *date_bascule;

			/* set the dates */
			date_debit = gsb_format_gdate_safe (std->date_debit);
			date_bascule = gsb_format_gdate_safe (std->date_bascule);
			tmp_str = g_markup_printf_escaped ("\t<Bet_transfert Nb=\"%d\" Ac=\"%d\" "
											   "Ty=\"%d\" Ra=\"%d\" Rt=\"%d\" Dd=\"%d\" "
											   "Dt=\"%s\" MCbd=\"%d\" Pa=\"%d\" Pn=\"%d\" Ca=\"%d\" "
											   "Sca=\"%d\" Bu=\"%d\" Sbu=\"%d\" "
											   "Dtb=\"%s\"  CCbd=\"%d\"CPa=\"%d\" CPn=\"%d\" CCa=\"%d\" "
											   "CSca=\"%d\" CBu=\"%d\" CSbu=\"%d\" "
											   "/>\n",
											   std->number,
											   std->main_account_number,
											   std->type,
											   std->card_account_number,
											   std->replace_transaction,
											   std->direct_debit,
											   my_safe_null_str (date_debit),
											   std->main_choice_debit_day,
											   std->main_payee_number,
											   std->main_payment_number,
											   std->main_category_number,
											   std->main_sub_category_number,
											   std->main_budgetary_number,
											   std->main_sub_budgetary_number,
											   my_safe_null_str (date_bascule),
											   std->card_choice_bascule_day,
											   std->card_payee_number,
											   std->card_payment_number,
											   std->card_category_number,
											   std->card_sub_category_number,
											   std->card_budgetary_number,
											   std->card_sub_budgetary_number);

			g_ptr_array_add (tab, tmp_str);

			g_free (date_debit);
			g_free (date_bascule);
		}
	}

	tmp_list = bet_data_loan_get_loan_list ();
	while (tmp_list)
	{
		gchar *date;
		gchar *string_to_free1;
		gchar *string_to_free2;
		gchar *string_to_free3;
		gchar *string_to_free4;
		gchar *string_to_free5;
		gchar *string_to_free6;
		gchar *string_to_free7;
		gchar *string_to_free8;
		gchar *string_to_free9;
		gchar *string_to_free10;
		gint flotting_point;
		LoanStruct *s_loan;

		s_loan = tmp_list->data;

		date = gsb_format_gdate_safe (s_loan->first_date);
		flotting_point = gsb_data_account_get_currency_floating_point (s_loan->account_number);

		string_to_free1 = utils_str_dtostr (s_loan->capital, flotting_point, TRUE);
		string_to_free2 = utils_str_dtostr (s_loan->amount_fees, flotting_point, TRUE);
		string_to_free3 = utils_str_dtostr (s_loan->percentage_fees, BET_PERCENTAGE_FEES_DIGITS, TRUE);
		string_to_free4 = utils_str_dtostr (s_loan->fixed_due_amount, flotting_point, TRUE);
		string_to_free5 = utils_str_dtostr (s_loan->annual_rate, BET_TAUX_DIGITS, TRUE);
		string_to_free6 = utils_str_dtostr (s_loan->first_capital, flotting_point, TRUE);
		string_to_free7 = utils_str_dtostr (s_loan->first_fees, flotting_point, TRUE);
		string_to_free8 = utils_str_dtostr (s_loan->first_interests, flotting_point, TRUE);
		string_to_free9 = utils_str_dtostr (s_loan->other_echeance_amount, flotting_point, TRUE);
		string_to_free10 = utils_str_dtostr (s_loan->capital_du, flotting_point, TRUE);

		tmp_str = g_markup_printf_escaped ("\t<Bet_loan\n"
										   "\t\tNb=\"%d\" Ac=\"%d\" Ver=\"%d\" InCol=\"%d\"\n"
										   "\t\tCa=\"%s\" Duree=\"%d\" FDate=\"%s\" Fees=\"%s\"\n"
										   "\t\tPFees=\"%s\" FDA=\"%s\" Taux=\"%s\" TyTaux=\"%d\"\n"
										   "\t\tFEchDif=\"%d\" FCa=\"%s\" FFees=\"%s\" FIn=\"%s\" OEch=\"%s\"\n"
										   "\t\tISchWL=\"%d\" AAc=\"%d\" ASch=\"%d\" AFr=\"%d\" CaDu=\"%s\" />\n",
										   s_loan->number,
										   s_loan->account_number,
										   s_loan->version_number,
										   s_loan->invers_cols_cap_ech,
										   my_safe_null_str (string_to_free1),
										   s_loan->duree,
										   date,
										   my_safe_null_str (string_to_free2),
										   my_safe_null_str (string_to_free3),
										   my_safe_null_str (string_to_free4),
										   my_safe_null_str (string_to_free5),
										   s_loan->type_taux,
										   s_loan->first_is_different,
										   my_safe_null_str (string_to_free6),
										   my_safe_null_str (string_to_free7),
										   my_safe_null_str (string_to_free8),
										   my_safe_null_str (string_to_free9),
										   s_loan->init_sch_with_loan,
										   s_loan->associated_account,
										   s_loan->associated_scheduled,
										   s_loan->associated_frequency,
										   my_safe_null_str (string_to_free10)
										   );
		g_ptr_array_add (tab, tmp_str);
		g_free (date);
		g_free (string_to_free1);
		g_free (string_to_free2);
		g_free (string_to_free3);
		g_free (string_to_free4);
		g_free (string_to_free5);
		g_free (string_to_free6);
		g_free (string_to_free7);
		g_free (string_to_free8);
		g_free (string_to_free9);
		g_free (string_to_free10);

		tmp_list = tmp_list->next;
	}
	return tab;
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
gint bet_data_get_sub_div_nb (gint transaction_number,
							  gboolean is_transaction)
{
	return ptr_sub_div (transaction_number, is_transaction);
}

/**
 * supprime toutes les données du module pour le compte passé en paramètre.
 *
 * \param
 *
 * \return TRUE
 **/
gboolean bet_data_remove_all_bet_data (gint account_number)
{
	GHashTable *tmp_list;
	GHashTableIter iter;
	gpointer key, value;

	tmp_list = bet_hist_list;

	g_hash_table_iter_init (&iter, tmp_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		HistData *shd = (HistData *) value;

		if (account_number != shd->account_nb)
			continue;

		if (g_hash_table_size (shd->sub_div_list) == 0)
		{
			g_hash_table_iter_remove (&iter);
			g_hash_table_iter_init (&iter, tmp_list);
		}
		else
		{
			GHashTableIter new_iter;

			g_hash_table_iter_init (&new_iter, shd->sub_div_list);
			while (g_hash_table_iter_next (&new_iter, &key, &value))
			{
				g_hash_table_iter_remove (&new_iter);
				g_hash_table_iter_init (&new_iter, shd->sub_div_list);

				if (g_hash_table_size (shd->sub_div_list) == 0)
					break;
			}
		}

		if (g_hash_table_size (tmp_list) == 0)
			break;
	}

	tmp_list = bet_futur_list;

	g_hash_table_iter_init (&iter, tmp_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		FuturData *sfd = (FuturData *) value;

		if (account_number != sfd->account_number)
			continue;

		bet_data_future_remove_lines (account_number, sfd->number,
						sfd->mother_row);
		g_hash_table_iter_init (&iter, tmp_list);

		if (g_hash_table_size (tmp_list) == 0)
			break;
	}

	 tmp_list = bet_transfert_list;

	g_hash_table_iter_init (&iter, tmp_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		TransfertData *std = (TransfertData *) value;

		if (account_number != std->main_account_number)
			continue;

		g_hash_table_iter_remove (&iter);
		g_hash_table_iter_init (&iter, tmp_list);

		if (g_hash_table_size (tmp_list) == 0)
			return TRUE;
	}

	bet_data_loan_struct_loan_free_by_account (account_number);
	return TRUE;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
void bet_data_renum_account_number_0 (gint new_account_number)
{
	gpointer account_nb;

	account_nb = GINT_TO_POINTER (new_account_number);
	g_hash_table_foreach (bet_hist_list, bet_data_hist_set_account_number, account_nb);
	g_hash_table_foreach (bet_futur_list, bet_data_future_set_account_number, account_nb);
	g_hash_table_foreach (bet_transfert_list, bet_data_transfert_set_account_number, account_nb);
}

/**
 * Sélectionne les onglets du module gestion budgétaire en fonction du type de compte
 *
 * \param
 *
 * \return
 **/
void bet_data_select_bet_pages (gint account_number)
{
	GtkWidget *page;
	GtkWidget *account_page;
	gint current_page;
	BetTypeOnglets bet_show_onglets;

	devel_debug_int (account_number);
	bet_show_onglets = gsb_data_account_get_bet_show_onglets (account_number);
	account_page = grisbi_win_get_account_page ();
	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (account_page));

	switch (bet_show_onglets)
	{
		case BET_ONGLETS_PREV:
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_ESTIMATE_PAGE);
			gtk_widget_show (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_HISTORICAL_PAGE);
			gtk_widget_show (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_FINANCE_PAGE);
			gtk_widget_hide (page);
			bet_hist_g_signal_unblock_tree_view ();
			gsb_data_account_set_bet_maj (account_number, BET_MAJ_ALL);
			if (current_page < GSB_PROPERTIES_PAGE
			 &&
			 gsb_data_account_get_bet_use_budget (account_number) == FALSE)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (account_page), GSB_TRANSACTIONS_PAGE);
			if (current_page == GSB_FINANCE_PAGE)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (account_page), GSB_ESTIMATE_PAGE);
			break;
		case BET_ONGLETS_HIST:
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_ESTIMATE_PAGE);
			gtk_widget_hide (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_HISTORICAL_PAGE);
			gtk_widget_show (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_FINANCE_PAGE);
			gtk_widget_hide (page);
			if (current_page == GSB_ESTIMATE_PAGE || current_page == GSB_FINANCE_PAGE)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (account_page), GSB_TRANSACTIONS_PAGE);
			bet_hist_g_signal_block_tree_view ();
			gsb_data_account_set_bet_maj (account_number, BET_MAJ_HISTORICAL);
			break;
		case BET_ONGLETS_CAP:
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_ESTIMATE_PAGE);
			gtk_widget_hide (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_HISTORICAL_PAGE);
			gtk_widget_hide (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_FINANCE_PAGE);
			gtk_widget_show (page);
			if (current_page == GSB_ESTIMATE_PAGE || current_page == GSB_HISTORICAL_PAGE)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (account_page), GSB_TRANSACTIONS_PAGE);
			break;
		case BET_ONGLETS_SANS:
		case BET_ONGLETS_ASSET:
		default:
			if (current_page < GSB_PROPERTIES_PAGE)
				gtk_notebook_set_current_page (GTK_NOTEBOOK (account_page), GSB_TRANSACTIONS_PAGE);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_ESTIMATE_PAGE);
			gtk_widget_hide (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_HISTORICAL_PAGE);
			gtk_widget_hide (page);
			page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (account_page), GSB_FINANCE_PAGE);
			gtk_widget_hide (page);
			break;
	}
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_set_div_ptr (gint type_div)
{
	if (type_div == 0)
	{
		ptr_div = &gsb_data_mix_get_category_number;
		ptr_sub_div = &gsb_data_mix_get_sub_category_number;
		ptr_type = &gsb_data_category_get_type;
		ptr_div_name = &gsb_data_category_get_name;
	}
	else
	{
		ptr_div = &gsb_data_mix_get_budgetary_number;
		ptr_sub_div = &gsb_data_mix_get_sub_budgetary_number;
		ptr_type = &gsb_data_budget_get_type;
		ptr_div_name = &gsb_data_budget_get_name;
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
void bet_data_struct_transaction_current_fyear_free (TransactionCurrentFyear *self)
{
	if (self->date)
		g_date_free (self->date);

	g_free (self);
}

/*
 * Met à jour les données à afficher dans les différentes vues du module
 *
 * \param
 * \param
 *
 * \return
 **/
void bet_data_update_bet_module (gint account_number,
								 gint page)
{
	gint type_maj;

	devel_debug_int (account_number);

	/* test account number */
	if (account_number == -1)
		return;

	type_maj = gsb_data_account_get_bet_maj (account_number);
	if (type_maj == BET_MAJ_FALSE)
		return;

	if (page == -1 && gsb_gui_navigation_get_current_page () == GSB_ACCOUNT_PAGE)
		page = gtk_notebook_get_current_page (GTK_NOTEBOOK (grisbi_win_get_account_page ()));

	switch (page)
	{
		case GSB_ESTIMATE_PAGE:
			bet_array_update_estimate_tab (account_number, type_maj);
			gsb_data_account_set_bet_maj (account_number, BET_MAJ_FALSE);
			break;
		case GSB_HISTORICAL_PAGE:
			if (type_maj == BET_MAJ_ALL)
			{
				bet_hist_populate_data (account_number);
				gsb_data_account_set_bet_maj (account_number, BET_MAJ_ESTIMATE);
			}
			else if (type_maj ==  BET_MAJ_HISTORICAL)
			{
				bet_hist_populate_data (account_number);
				gsb_data_account_set_bet_maj (account_number, BET_MAJ_FALSE);
			}
			bet_hist_set_page_title (account_number);
			break;
		default:
			break;
	}
}

/**
 * used to free the global variables
 *
 * \param
 *
 * \return
 **/
void bet_data_variables_free (void)
{
	if (bet_hist_list)
		g_hash_table_destroy (bet_hist_list);
	if (bet_futur_list)
		g_hash_table_destroy (bet_futur_list);
	if (bet_transfert_list)
		g_hash_table_destroy (bet_transfert_list);
}

/**
 * used when we init all the global variables
 *
 * \param
 *
 * \return FALSE
 **/
gboolean bet_data_variables_init (void)
{
	if (bet_hist_list)
		g_hash_table_destroy (bet_hist_list);
	bet_hist_list = g_hash_table_new_full (g_str_hash,
										   g_str_equal,
										   (GDestroyNotify) g_free,
										   (GDestroyNotify) bet_data_hist_struct_free);

	if (bet_futur_list)
		g_hash_table_destroy (bet_futur_list);
	bet_futur_list = g_hash_table_new_full (g_str_hash,
											 g_str_equal,
											 (GDestroyNotify) g_free,
											 (GDestroyNotify) bet_data_future_struct_free);
	futur_number = 0;

	if (bet_transfert_list)
		g_hash_table_destroy (bet_transfert_list);
	bet_transfert_list = g_hash_table_new_full (g_str_hash,
												g_str_equal,
												(GDestroyNotify) g_free,
												(GDestroyNotify) struct_free_bet_transfert);
	transfert_number = 0;

	return FALSE;
}

/* ARRAY_DATA */
/**
 * retourne la date max d'interrogation pour les prévisions
 *
 * \param
 *
 * \return
 **/
GDate *bet_data_array_get_date_max (gint account_number)
{
	GDate *date_min;
	GDate *date_max;
	GDate *date_tmp;

	date_min = gsb_data_account_get_bet_start_date (account_number);

	date_max = gsb_date_copy (date_min);
	date_tmp = date_max;

	if (g_date_get_day (date_min) == 1)
	{
		g_date_add_months (date_max, gsb_data_account_get_bet_months (account_number) - 1);
		date_max = gsb_date_get_last_day_of_month (date_tmp);
		g_date_free (date_tmp);
	}
	else
	{
		g_date_add_months (date_max, gsb_data_account_get_bet_months (account_number));
		g_date_subtract_days (date_max, 1);
	}

	g_date_free (date_min);

	return date_max;
}

/* FUTURE_DATA */
/**
 * add lines creates in the bet_futur_list
 *
  * \param
 *
 * \return
 **/
gboolean bet_data_future_add_lines (FuturData *sfd)
{
	gchar *key;

	futur_number ++;

	if (sfd->frequency == 0)
	{
		key = bet_data_get_key (sfd->account_number, futur_number);

		sfd->number = futur_number;
		g_hash_table_insert (bet_futur_list, key, sfd);
	}
	else
	{
		GDate *date;
		GDate *date_max;
		gint mother_row;
		FuturData *new_sfd = NULL;

		mother_row = futur_number;

		date_max = bet_data_array_get_date_max (sfd->account_number);

		/* we don't change the initial date */
		date = gsb_date_copy (sfd->date);
		while (TRUE)
		{
			key = bet_data_get_key (sfd->account_number, futur_number);

			if (mother_row == futur_number)
			{
				if (new_sfd)
					bet_data_future_struct_free (new_sfd);
				new_sfd = sfd;
			}
			else
			{
				if (new_sfd)
					new_sfd->mother_row = mother_row;
				else
				{
					g_date_free (date_max);
					return FALSE;
				}
			}

			new_sfd->number = futur_number;
			g_hash_table_insert (bet_futur_list, key, new_sfd);

			date = bet_data_futur_get_next_date (new_sfd, date, date_max);
			if (date == NULL || !g_date_valid (date))
				break;

			futur_number ++;
			new_sfd = bet_data_future_copy_struct (sfd);
			new_sfd->date = date;
		}
		g_date_free (date_max);
	}

	gsb_file_set_modified (TRUE);

	return TRUE;
}

/**
 * add lines from file
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_future_set_lines_from_file (FuturData *sfd)
{
	gchar *key;

	key = bet_data_get_key (sfd->account_number, sfd->number);

	bet_data_future_set_max_number (sfd->number);

	g_hash_table_insert (bet_futur_list, key, sfd);

	return TRUE;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
GHashTable *bet_data_future_get_list (void)
{
	return bet_futur_list;
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
FuturData *bet_data_future_get_struct (gint account_number,
									   gint number)
{
	gchar *key;
	FuturData *sfd;

	key = bet_data_get_key (account_number, number);

	if ((sfd = g_hash_table_lookup (bet_futur_list, key)))
		return sfd;
	else
		return NULL;
}


/**
 * supprime l'occurence sélectionnée.
 *
 * \param
 * \param
 * \param maj TRUE force la mise à jour
 *
 * \return
 **/
gboolean bet_data_future_remove_line (gint account_number,
									  gint number,
									  gboolean maj)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, bet_futur_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		FuturData *sfd = (FuturData *) value;

		if (number != sfd->number)
			continue;

		if (account_number != sfd->account_number)
		{
			if (sfd->is_transfert == 0
			 || (sfd->is_transfert && account_number != sfd->account_transfert))
				continue;
		}

		g_hash_table_iter_remove (&iter);

		break;
	}

	gsb_file_set_modified (TRUE);

	if (maj)
	{
		gsb_data_account_set_bet_maj (account_number, BET_MAJ_ESTIMATE);
		bet_data_update_bet_module (account_number, GSB_ESTIMATE_PAGE);
	}

	return FALSE;
}

/**
 * supprime touts les occurrences de la ligne sélectionnée
 *
 * \param
 * \param
 * \param
 *
 * \return
 **/
gboolean bet_data_future_remove_lines (gint account_number,
									   gint number,
									   gint mother_row)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, bet_futur_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		FuturData *sfd = (FuturData *) value;

		if (account_number != sfd->account_number)
		{
			if (sfd->is_transfert == 0
			 || (sfd->is_transfert && account_number != sfd->account_transfert))
				continue;
		}

		if (number == sfd->number)
			g_hash_table_iter_remove (&iter);
		else if (number == sfd->mother_row)
			g_hash_table_iter_remove (&iter);
		else if (mother_row > 0 && mother_row == sfd->number)
			g_hash_table_iter_remove (&iter);
		else if (mother_row > 0 && mother_row == sfd->mother_row)
			g_hash_table_iter_remove (&iter);
	}

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
FuturData *bet_data_future_struct_init (void)
{
	FuturData *sfd;

	sfd = g_malloc0 (sizeof (FuturData));

	sfd->date = NULL;
	sfd->amount = null_real;
	sfd->notes = NULL;
	sfd->limit_date = NULL;

	return sfd;
}

/* HISTORIQUE_DATA */
/**
 *
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_hist_div_add (gint account_number,
								gint div_number,
								gint sub_div_nb)
{
	gchar *key = NULL;
	gchar *sub_key = NULL;
	HistData *shd;

	key = bet_data_get_key (account_number, div_number);

	if ((shd = g_hash_table_lookup (bet_hist_list, key)))
	{
		g_free (key);
		shd->div_edited = FALSE;
		shd->amount = null_real;

		if (sub_div_nb > 0)
		{
			sub_key = utils_str_itoa (sub_div_nb);
			if (!g_hash_table_lookup (shd->sub_div_list, sub_key))
			{
				HistData *sub_shd;

				sub_shd = bet_data_hist_struct_init ();
				if (!sub_shd)
				{
					dialogue_error_memory ();
					g_free (sub_key);
					return FALSE;
				}
				sub_shd->div_number = sub_div_nb;
				g_hash_table_insert (shd->sub_div_list, sub_key, sub_shd);
			}
			else
			{
				shd->div_edited = FALSE;
				shd->amount = null_real;
				g_free (sub_key);
			}
		}
	}
	else
	{
		shd = bet_data_hist_struct_init ();
		if (!shd)
		{
			dialogue_error_memory ();
			return FALSE;
		}
		shd->account_nb = account_number;
		shd->origin = gsb_data_account_get_bet_hist_data (account_number);
		shd->div_number = div_number;
		if (sub_div_nb > 0)
		{
			HistData *sub_shd;

			sub_shd = bet_data_hist_struct_init ();
			if (!sub_shd)
			{
				dialogue_error_memory ();
				bet_data_hist_struct_free  (shd);
				return FALSE;
			}
			sub_key = utils_str_itoa (sub_div_nb);
			sub_shd->div_number = sub_div_nb;
			g_hash_table_insert (shd->sub_div_list, sub_key, sub_shd);
		}
		g_hash_table_insert (bet_hist_list, key, shd);
	}

	return TRUE;
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
GsbReal bet_data_hist_get_div_amount (gint account_nb,
									  gint div_number,
									  gint sub_div_nb)
{
	gchar *key;
	HistData *shd;
	GsbReal amount;

	key = bet_data_get_key (account_nb, div_number);

	if ((shd = g_hash_table_lookup (bet_hist_list, key)))
	{
		if (sub_div_nb == 0)
			amount = shd->amount;
		else
		{
			gchar *sub_key;
			HistData *sub_shd;

			sub_key = utils_str_itoa (sub_div_nb);
			if ((sub_shd = g_hash_table_lookup (shd->sub_div_list, sub_key)))
				amount = sub_shd->amount;
			else
				amount = null_real;
			g_free (sub_key);
		}
	}
	else
		amount = null_real;
	g_free (key);

	return amount;
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
gboolean bet_data_set_div_amount (gint account_nb,
								  gint div_number,
								  gint sub_div_nb,
								  GsbReal amount)
{
	gchar *key;
	HistData *shd;

	key = bet_data_get_key (account_nb, div_number);

	if ((shd = g_hash_table_lookup (bet_hist_list, key)))
	{
		if (sub_div_nb == 0)
			shd->amount = amount;
		else
		{
			gchar *sub_key;
			HistData *sub_shd;

			sub_key = utils_str_itoa (sub_div_nb);
			if ((sub_shd = g_hash_table_lookup (shd->sub_div_list, sub_key)))
				sub_shd->amount = amount;
			g_free (sub_key);
		}
	}

	g_free (key);

	return FALSE;
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
gboolean bet_data_hist_get_div_edited (gint account_number,
									   gint div_number,
									   gint sub_div_nb)
{
	gchar *key;
	gint origin;
	HistData *shd;
	gboolean edited;

	key = bet_data_get_key (account_number, div_number);

	origin = gsb_data_account_get_bet_hist_data (account_number);

	if ((shd = g_hash_table_lookup (bet_hist_list, key))
	 && shd->origin == origin)
	{
		if (sub_div_nb == 0)
			edited = shd->div_edited;
		else
		{
			gchar *sub_key;
			HistData *sub_shd;

			sub_key = utils_str_itoa (sub_div_nb);
			if ((sub_shd = g_hash_table_lookup (shd->sub_div_list, sub_key)))
				edited = sub_shd->div_edited;
			else
				edited = FALSE;
			g_free (sub_key);
		}
	}
	else
		edited = FALSE;
	g_free (key);

	return edited;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_set_div_edited (gint account_nb,
								  gint div_number,
								  gint sub_div_nb,
								  gboolean edited)
{
	gchar *key;
	HistData *shd;

	key = bet_data_get_key (account_nb, div_number);

	if ((shd = g_hash_table_lookup (bet_hist_list, key)))
	{
		if (sub_div_nb == 0)
			shd->div_edited = edited;
		else
		{
			gchar *sub_key;
			HistData *sub_shd;

			sub_key = utils_str_itoa (sub_div_nb);
			if ((sub_shd = g_hash_table_lookup (shd->sub_div_list, sub_key)))
				sub_shd->div_edited = edited;
			g_free (sub_key);
		}
	}

	g_free (key);

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
void bet_data_hist_div_insert (HistData *shd,
							   HistData *sub_shd)
{
	gchar *key;
	gchar *sub_key;
	HistData *tmp_shd;

	key = bet_data_get_key (shd->account_nb, shd->div_number);

	if ((tmp_shd = g_hash_table_lookup (bet_hist_list, key)))
	{
		g_free (key);
		tmp_shd->div_edited = shd->div_edited;
		tmp_shd->amount = shd->amount;

		if (sub_shd)
		{
			sub_key = utils_str_itoa (sub_shd->div_number);
			g_hash_table_insert (tmp_shd->sub_div_list, sub_key, sub_shd);
		}
		bet_data_hist_struct_free (shd);
	}
	else
	{
		if (sub_shd)
		{
			sub_key = utils_str_itoa (sub_shd->div_number);
			g_hash_table_insert (shd->sub_div_list, sub_key, sub_shd);
		}
		g_hash_table_insert (bet_hist_list, key, shd);
	}
}

/**
 * Ajoute les données de la transaction à la division et la sous division
 * création des nouvelles divisions et si existantes ajout des données
 * par appel à bet_data_hist_update_data ()
 *
 * \param
 * \param
 * \param
 * \param
 * \param
 *
 * \return
**/
gboolean bet_data_hist_div_populate (gint transaction_number,
									 gboolean is_transaction,
									 GHashTable  *list_div,
									 gint type_de_transaction,
									 TransactionCurrentFyear *tcf)
{
	gint div = 0;
	gint sub_div = 0;
	gchar *key;
	HistData *shd = NULL;
	GsbReal amount;

	div = ptr_div (transaction_number, is_transaction);
	if (div > 0)
		sub_div = ptr_sub_div (transaction_number, is_transaction);
	else
		return FALSE;

	amount = gsb_data_transaction_get_amount (transaction_number);

	/* on complète la structure tcf pour les graphiques */
	if (tcf)
	{
		tcf->div_nb = div;
		tcf->sub_div_nb = sub_div;
		tcf->amount = amount;
	}

	key = utils_str_itoa (div);
	if ((shd = g_hash_table_lookup (list_div, key)))
	{
		bet_data_hist_update_data (shd, transaction_number, sub_div, type_de_transaction, amount);
		g_free (key);
	}
	else
	{
		shd = bet_data_hist_struct_init ();
		shd->div_number = div;
		shd->account_nb = gsb_data_transaction_get_account_number (transaction_number);
		bet_data_hist_update_data (shd, transaction_number, sub_div, type_de_transaction, amount);
		g_hash_table_insert (list_div, key, shd);
	}

	/* return value */
	return FALSE;
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
gboolean bet_data_hist_div_remove (gint account_number,
								   gint div_number,
								   gint sub_div_nb)
{
	gchar *key;
	char *sub_key;
	HistData *shd;
	gboolean return_val = FALSE;

	key = bet_data_get_key (account_number, div_number);

	if ((shd = g_hash_table_lookup (bet_hist_list, key)))
	{
		return_val = TRUE;
		if (sub_div_nb > 0)
		{
			sub_key = utils_str_itoa (sub_div_nb);
			g_hash_table_remove (shd->sub_div_list, sub_key);
			g_free (sub_key);
		}
		if (g_hash_table_size (shd->sub_div_list) == 0)
			g_hash_table_remove (bet_hist_list, key);
	}
	g_free (key);

	return return_val;
}

/**
 * return TRUE si la division et sous division existe.
 *
 * \param
 * \param
 * \param
 *
 * \return
 **/
gboolean bet_data_hist_div_search (gint account_number,
								   gint div_number,
								   gint sub_div_nb)
{
	gchar *key;
	gchar *sub_key;
	gint origin;
	HistData *shd;
	gboolean return_val = FALSE;

	key = bet_data_get_key (account_number, div_number);

	origin = gsb_data_account_get_bet_hist_data (account_number);

	if ((shd = g_hash_table_lookup (bet_hist_list, key))
	 && shd->origin == origin)
	{
		if (sub_div_nb == 0)
			return_val = TRUE;
		else if (sub_div_nb > 0)
		{
			sub_key = utils_str_itoa (sub_div_nb);
			if (g_hash_table_lookup (shd->sub_div_list, sub_key))
				return_val = TRUE;
			g_free (sub_key);
		}
	}
	g_free (key);

	return return_val;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
void bet_data_hist_struct_free (HistData *shd)
{
	if (shd->sbr)
		bet_data_bet_range_struct_free (shd->sbr);

	if (shd->sub_div_list)
		g_hash_table_remove_all (shd->sub_div_list);
	g_hash_table_unref (shd->sub_div_list);

	g_free (shd);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
HistData *bet_data_hist_struct_init (void)
{
	HistData *shd;

	shd = g_malloc0 (sizeof (HistData));
	shd->account_nb = 0;
	shd->div_number = 0;
	shd->div_edited = FALSE;
	shd->amount = null_real;
	shd->sub_div_list = g_hash_table_new_full (g_str_hash,
						g_str_equal,
						(GDestroyNotify) g_free,
						(GDestroyNotify) bet_data_hist_struct_free);

	shd->sbr = bet_data_bet_range_struct_init ();

	return shd;
}

/**
 * modify futures data lines
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_future_modify_lines (FuturData *sfd)
{
	gchar *key;

	key = bet_data_get_key (sfd->account_number, sfd->number);

	g_hash_table_replace (bet_futur_list, key, sfd);

	gsb_file_set_modified (TRUE);

	return TRUE;
}

/* TRANSFERT_DATA */
/**
 *
 *
 * \param
 *
 * \return
 **/
TransfertData *struct_initialise_bet_transfert (void)
{
	TransfertData *std;

	std =  g_malloc0 (sizeof (TransfertData));

	std->date_debit = NULL;
	std->date_bascule = NULL;

	return std;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
void struct_free_bet_transfert (TransfertData *std)
{
	if (std->date_debit)
		g_date_free (std->date_debit);
	if (std->date_bascule)
		g_date_free (std->date_bascule);

	g_free (std);
}

/**
 *
 *
 * \param
 *
 * \return
 **/
GHashTable *bet_data_transfert_get_list (void)
{
	return bet_transfert_list;
}

/**
 * add line in the bet_transfer_list
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_transfert_add_line (TransfertData *std)
{
	gchar *key;

	transfert_number ++;

	key = bet_data_get_key (std->main_account_number, transfert_number);

	std->number = transfert_number;
	g_hash_table_insert (bet_transfert_list, key, std);

	gsb_file_set_modified (TRUE);

	return TRUE;
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
gboolean bet_data_transfert_remove_line (gint account_number,
										 gint number)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, bet_transfert_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		TransfertData *std = (TransfertData *) value;

		if (account_number != std->main_account_number
		 ||
		 number != std->number)
			continue;

		g_hash_table_iter_remove (&iter);

		break;
	}

	gsb_file_set_modified (TRUE);

	gsb_data_account_set_bet_maj (account_number, BET_MAJ_ESTIMATE);
	bet_data_update_bet_module (account_number, GSB_ESTIMATE_PAGE);

	return FALSE;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_transfert_set_line_from_file (TransfertData *std)
{
	gchar *key;

	key = bet_data_get_key (std->main_account_number, std->number);

	if (std->number >  transfert_number)
		transfert_number = std->number;

	g_hash_table_insert (bet_transfert_list, key, std);

	return TRUE;
}

/**
 * modify transfert line
 *
 * \param
 *
 * \return
 **/
gboolean bet_data_transfert_modify_line (TransfertData *std)
{
	gchar *key;

	key = bet_data_get_key (std->main_account_number, std->number);

	g_hash_table_replace (bet_transfert_list, key, std);

	gsb_file_set_modified (TRUE);

	return TRUE;
}

/**
 * crée la ou les transactions de remise à zéro des comptes cartes concernés
 *
 * \param struct transfert
 *
 * \return
 **/
static void bet_data_transfert_create_reset_credit_card (TransfertData *std)
{
	gint transaction_number;
	GDate *date;
	GsbReal amount;

	/* fixe bug :des operations peuvent exister le jour de bascule */
	date = gsb_date_copy (std->date_bascule);

	/* replace_account is an account */
	if (std->type == 0)
	{
		amount = gsb_data_account_get_balance_at_date (std->card_account_number, date);
		transaction_number = gsb_data_transaction_new_transaction (std->card_account_number);
		gsb_data_transaction_set_date (transaction_number, date);
		gsb_data_transaction_set_amount (transaction_number, gsb_real_opposite (amount));

		/* set the currency */
		gsb_data_transaction_set_currency_number (transaction_number,
												  gsb_data_account_get_currency (std->card_account_number));

		/* set the payement mode */
		if (amount.mantissa < 0)
			gsb_data_transaction_set_method_of_payment_number (transaction_number,
															   gsb_data_account_get_default_debit (std->card_account_number));
		else
			gsb_data_transaction_set_method_of_payment_number (transaction_number,
															   gsb_data_account_get_default_credit (std->card_account_number));

		/* set the payee */
		gsb_data_transaction_set_payee_number (transaction_number, std->main_payee_number);

		/* set the category sub_category */
		if (std->card_category_number)
		{
			gsb_data_transaction_set_category_number (transaction_number, std->card_category_number);
			if (std->card_sub_category_number)
				gsb_data_transaction_set_sub_category_number (transaction_number,
															  std->card_sub_category_number);
		}

		/* set the IB sub_IB */
		if (std->card_budgetary_number)
		{
			gsb_data_transaction_set_budgetary_number (transaction_number, std->card_budgetary_number);
			if (std->card_sub_category_number)
				gsb_data_transaction_set_sub_budgetary_number (transaction_number,
															   std->card_sub_budgetary_number);
		}

		/* append the transaction in list */
		gsb_transactions_list_append_new_transaction (transaction_number, TRUE);
	}
	else
	{
		GPtrArray *balances;
		gchar **tab;
		gint i;

		tab = g_strsplit (gsb_data_partial_balance_get_liste_cptes (std->card_account_number), ";", 0);

		/* on calcule la balance de tous les comptes du pseudo compte */
		balances = gsb_data_partial_balance_calculate_balances_at_date (std->card_account_number, date);

		for (i = 0; tab[i]; i++)
		{
			gint account_number;
			GsbReal *balance;

			account_number = utils_str_atoi (tab[i]);
			balance = (GsbReal *) g_ptr_array_index (balances, i);
			amount.mantissa = balance->mantissa;
			amount.exponent = balance->exponent;

			transaction_number = gsb_data_transaction_new_transaction (account_number);
			gsb_data_transaction_set_date (transaction_number, date);
			gsb_data_transaction_set_amount (transaction_number, gsb_real_opposite (amount));

			/* set the currency */
			gsb_data_transaction_set_currency_number (transaction_number,
													  gsb_data_account_get_currency (account_number));

			/* set the payement mode */
			if (amount.mantissa < 0)
				gsb_data_transaction_set_method_of_payment_number (transaction_number,
																   gsb_data_account_get_default_debit (account_number));
			else
				gsb_data_transaction_set_method_of_payment_number (transaction_number,
																   gsb_data_account_get_default_credit (account_number));

			/* set the payee */
			gsb_data_transaction_set_payee_number (transaction_number, std->main_payee_number);

			/* set the category sub_category */
			if (std->card_category_number)
			{
				gsb_data_transaction_set_category_number (transaction_number, std->card_category_number);
				if (std->card_sub_category_number)
					gsb_data_transaction_set_sub_category_number (transaction_number,
																  std->card_sub_category_number);
			}

			/* set the IB sub_IB */
			if (std->card_budgetary_number)
			{
				gsb_data_transaction_set_budgetary_number (transaction_number, std->card_budgetary_number);
				if (std->card_sub_category_number)
					gsb_data_transaction_set_sub_budgetary_number (transaction_number,
																   std->card_sub_budgetary_number);
			}

			/* append the transaction in list */
			gsb_transactions_list_append_new_transaction (transaction_number, TRUE);
		}
		g_strfreev (tab);
		g_ptr_array_free (balances, TRUE);
	}
	g_date_free (date);
}

/**
 * update la date de la ligne transfert lorsque la date de bascule est dépassée
 *
 * \param struct transfert contenant les données
 * \param
 * \param
 *
 * \return
 **/
void bet_data_transfert_update_date_if_necessary (TransfertData *std,
												  GDate *date_bascule,
												  gboolean force)
{
	GDate *date_jour;
	GDate *tmp_date;

	if (std->date_bascule == NULL)
		return;

	date_jour = gdate_today ();

	if (g_date_compare (date_jour, date_bascule) > 0 || force)
	{
		gchar *msg;
		const gchar *tmp_str;

		if (std->type == 0)
			tmp_str = gsb_data_account_get_name (std->card_account_number);
		else
			tmp_str = gsb_data_partial_balance_get_name (std->card_account_number);

		msg = g_strdup_printf (
						_("Warning: the start date of the period of deferred debit card account (%s) "
						  "is reached or exceeded. All operations of this card account must be entered "
						  "or they will not be taken into account in the period.\n\n"
						  "If you have entered all transactions, confirm with \"Yes\".\n"
						  "If you have yet to enter into transactions on behalf deferred debit card finish "
						  "with \"No\" and update the account before you start."),
						tmp_str);
		if (!dialogue_yes_no (msg, _("Confirmation of the change of period"), GTK_RESPONSE_CANCEL))
		{
			g_free (msg);
			return;
		}
			g_free (msg);

		/* on crée la transaction dans le compte principal */
		if (std->direct_debit)
			bet_array_create_transaction_from_transfert (std);

		/* on remet à zéro les comptes cartes */
		bet_data_transfert_create_reset_credit_card (std);

		/* on incrémente la date de prélèvement */
		tmp_date = gsb_date_copy (std->date_debit);

		g_date_free (std->date_debit);
		g_date_add_months (tmp_date, 1);

		if (std->main_choice_debit_day)
		{
			std->date_debit = gsb_date_get_last_banking_day_of_month (tmp_date);
			g_date_free (tmp_date);
		}
		else
		{
			std->date_debit = tmp_date;
		}

		/* on incrémente la date de bascule */
		tmp_date = gsb_date_copy (std->date_bascule);

		g_date_free (std->date_bascule);
		g_date_add_months (tmp_date, 1);
		std->date_bascule = tmp_date;

		gsb_data_account_set_bet_maj (std->main_account_number, BET_MAJ_ESTIMATE);
		gsb_file_set_modified (TRUE);
	}

	g_date_free (date_jour);
}

/**
 * Création effective de la transaction dans le compte qui comporte le suivi de la carte
 * à débit différé
 *
 * \param structure transfert
 *
 * \return
 **/
void bet_data_transfert_create_new_transaction (TransfertData *std)
{
	gint transaction_number;
	GDate *date;
	GsbReal amount;

	date = gsb_date_copy (std->date_bascule);

	/* on enlève 1 jour pour la date de l'opération de remise à 0 du compte */
	g_date_subtract_days (date, 1);

	transaction_number = gsb_data_transaction_new_transaction (std->main_account_number);

	/* set the date */
	gsb_data_transaction_set_date (transaction_number, std->date_debit);

	/* set the amount soit celui du compte carte soit celui du solde partiel */
	if (std->type == 0)
	{
		amount = gsb_data_account_get_balance_at_date (std->card_account_number, date);
	}
	else
	{
		amount = gsb_data_partial_balance_get_balance_at_date (std->card_account_number, date);
	}
	gsb_data_transaction_set_amount (transaction_number, amount);

	/* set the currency */
	gsb_data_transaction_set_currency_number (transaction_number,
											  gsb_data_account_get_currency (std->main_account_number));

	/* set the payement mode */
	gsb_data_transaction_set_method_of_payment_number (transaction_number, std->main_payment_number);

	/* set the payee */
	gsb_data_transaction_set_payee_number (transaction_number, std->main_payee_number);

	/* set the category sub_category */
	if (std->main_category_number)
	{
		gsb_data_transaction_set_category_number (transaction_number, std->main_category_number);
		if (std->main_sub_category_number)
			gsb_data_transaction_set_sub_category_number (transaction_number, std->main_sub_category_number);
	}

	/* set the IB sub_IB */
	if (std->main_budgetary_number)
	{
		gsb_data_transaction_set_budgetary_number (transaction_number, std->main_budgetary_number);
		if (std->main_sub_budgetary_number)
			gsb_data_transaction_set_sub_budgetary_number (transaction_number, std->main_sub_budgetary_number);
	}

	/* append the transaction in list */
	gsb_transactions_list_append_new_transaction (transaction_number, TRUE);

	g_date_free (date);
}

/**
 * initialise la structure
 *
 * \param
 *
 * \return
 **/
TransactionCurrentFyear *bet_data_struct_transaction_current_fyear_init (void)
{
	TransactionCurrentFyear *self;

	self = g_malloc0 (sizeof (TransactionCurrentFyear));

	self->date = NULL;
	self->amount = null_real;

	return self;
}

/**
 * Sets to 0 all the amounts selected for historical data (ou "archival")
 *
 * \param account_number
 *
 * \return
 **/
void bet_data_hist_reset_all_amounts (gint account_number)
{
	GHashTableIter iter;
	gpointer key, value;

	if (g_hash_table_size (bet_hist_list) == 0)
		return;

	g_hash_table_iter_init (&iter, bet_hist_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		HistData *shd = (HistData*) value;

		if (g_hash_table_size (shd->sub_div_list) == 0)
		{
			if (shd->div_edited)
				bet_data_set_div_amount (account_number, shd->div_number, 0, null_real);
		}
		else
		{
			GHashTableIter new_iter;

			g_hash_table_iter_init (&new_iter, shd->sub_div_list);
			while (g_hash_table_iter_next (&new_iter, &key, &value))
			{
				HistData *sub_shd = (HistData*) value;

				if (sub_shd->div_edited)
					bet_data_set_div_amount (account_number, shd->div_number, sub_shd->div_number, null_real);
			}
		}
	}
}

/**
 *
 *
 * \param
 *
 * \return
 **/
TransfertData *bet_data_transfert_get_struct_from_number (gint number)
{
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	TransfertData *std = NULL;

	g_hash_table_iter_init (&iter, bet_transfert_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		std = (TransfertData *) value;

		if (number != std->number)
			continue;
		break;
	}

	return std;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
gint bet_data_transfert_get_main_account_from_card (gint card_account_number)
{
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	TransfertData *std = NULL;

	g_hash_table_iter_init (&iter, bet_transfert_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		std = (TransfertData *) value;

		if (card_account_number == std->card_account_number)
		{
			return std->main_account_number;
		}
	}

	return 0;
}

/**
 *
 *
 * \param
 *
 * \return
 **/
void bet_data_transfert_remove_line_from_card (gint card_account_number)
{
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	gint main_account_number = 0;

	g_hash_table_iter_init (&iter, bet_transfert_list);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		gint number = 0;
		TransfertData *std = NULL;

		std = (TransfertData *) value;
		if (card_account_number != std->card_account_number)
			continue;

		main_account_number = std->main_account_number;
		number = gsb_data_account_get_bet_hist_use_data_in_account (main_account_number);
		if (number > 0)
			number--;
		gsb_data_account_set_bet_hist_use_data_in_account (main_account_number, number);
		g_hash_table_iter_remove (&iter);

		break;
	}
    if (gsb_gui_navigation_get_current_account () == main_account_number
		&& bet_data_get_current_bet_page () == GSB_ESTIMATE_PAGE)
	{
		gsb_data_account_set_bet_maj (main_account_number, BET_MAJ_ESTIMATE);
		bet_data_update_bet_module (main_account_number, GSB_ESTIMATE_PAGE);
	}
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
