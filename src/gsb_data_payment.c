/* ************************************************************************** */
/*                                                                            */
/*     Copyright (C)    2000-2008 Cédric Auger (cedric@grisbi.org)            */
/*          2003-2008 Benjamin Drieu (bdrieu@april.org)                       */
/*          https://www.grisbi.org                                            */
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
 * \file gsb_data_payment.c
 * work with the payment structure, no GUI here
 */

#include "config.h"

#include "include.h"
#include <glib/gi18n.h>

/*START_INCLUDE*/
#include "gsb_data_payment.h"
#include "gsb_data_account.h"
#include "utils_str.h"
#include "erreur.h"
/*END_INCLUDE*/

/**
 * \struct
 * Describe a method of payment
 */
typedef struct _PaymentStruct		PaymentStruct;

struct _PaymentStruct
{
	gint		payment_number;
	gint		account_number;
	gchar *		payment_name;
	gint		payment_sign;			/*< GSB_PAYMENT_NEUTRAL, GSB_PAYMENT_DEBIT, GSB_PAYMENT_CREDIT */

	gint		show_entry;				/*< when select it in form, need to show an entry (for cheque number for example) or not */
	gint		automatic_numbering;	/*< for cheque number for example */
	gchar *		last_number;			/*< the last number of cheque used */
};

/*START_STATIC*/
/* contains the g_slist of PaymentStruct */
static GSList *payment_list = NULL;

/* a pointer to the last payment used (to increase the speed) */
static PaymentStruct *payment_buffer;
/*END_STATIC*/

/*START_EXTERN*/
/*END_EXTERN*/

/******************************************************************************/
/* Private functions                                                          */
/******************************************************************************/
/*
 * This internal function is called to free the memory used by a PaymentStruct structure
 *
 * \param
 *
 * \return
 **/
static void _gsb_data_payment_free (PaymentStruct *payment)
{
	if (!payment)
		return;
	if (payment->payment_name)
		g_free (payment->payment_name);
	if (payment->last_number)
		g_free (payment->last_number);

	if (payment_buffer == payment)
		payment_buffer = NULL;

	g_free (payment);
}

/**
 * find and return the structure of the payment asked
 *
 * \param payment_number number of payment
 *
 * \return the PaymentStruct corresponding to the payment_number or NULL
 **/
static PaymentStruct *gsb_data_payment_get_structure (gint payment_number)
{
	GSList *tmp;

	if (!payment_number)
		return NULL;

	/* before checking all the payments, we check the buffer */
	if (payment_buffer && payment_buffer->payment_number == payment_number)
		return payment_buffer;

	tmp = payment_list;
	while (tmp)
	{
		PaymentStruct *payment;

		payment = tmp->data;

		if (payment->payment_number == payment_number)
		{
			payment_buffer = payment;

			return payment;
		}

		tmp = tmp->next;
	}

	return NULL;
}

/**
 * find and return the last number of payment
 * carrefull, it's an internal function to create a new payment number,
 * nothing to see to the number of cheque we want to increase
 *
 * \param none
 *
 * \return last number of payment
 **/
static gint gsb_data_payment_max_number (void)
{
	GSList *tmp;
	gint number_tmp = 0;

	tmp = payment_list;

	while (tmp)
	{
		PaymentStruct *payment;

		payment = tmp->data;

		if (payment->payment_number > number_tmp)
			number_tmp = payment->payment_number;

		tmp = tmp->next;
	}

	return number_tmp;
}

/******************************************************************************/
/* Public functions                                                           */
/******************************************************************************/
/**
 * set the payments global variables to NULL,
 * usually when we init all the global variables
 *
 * \param
 *
 * \return FALSE
 **/
gboolean gsb_data_payment_init_variables (void)
{
	if (payment_list)
	{
		GSList* tmp_list;

		tmp_list = payment_list;
		while (tmp_list)
		{
			PaymentStruct *payment;

			payment = tmp_list->data;
			tmp_list = tmp_list->next;
			_gsb_data_payment_free (payment);
		}

		g_slist_free (payment_list);
	}
	payment_list = NULL;
	payment_buffer = NULL;

	return FALSE;
}

/**
 * return the list of payment structure
 * usefull when want to check all payments
 *
 * \param none
 *
 * \return the g_slist of payments structure
 **/
GSList *gsb_data_payment_get_payments_list (void)
{
	return payment_list;
}

/**
 * return a newly allocated list with the payments numbers for
 * the asked account
 * usefull to know for example if there is some method of payments for that account
 *
 * \param account_number
 *
 * \return a newly allocated GSList of payments number (to be freed), or NULL if none
 **/
GSList *gsb_data_payment_get_list_for_account (gint account_number)
{
	GSList *tmp_list;
	GSList *returned_list = NULL;

	tmp_list = payment_list;
	while (tmp_list)
	{
		PaymentStruct *payment;

		payment = tmp_list->data;

		if (payment->account_number == account_number)
			returned_list = g_slist_append (returned_list, GINT_TO_POINTER (payment->payment_number));

		tmp_list = tmp_list->next;
	}

	return returned_list;
}

/**
 * return the number of the payment given in param
 * and initialise the buffer
 *
 * \param payment_ptr a pointer to the struct of the payment
 *
 * \return the number of the payment, 0 if problem
 **/
gint gsb_data_payment_get_number (gpointer payment_ptr)
{
	PaymentStruct *payment;

	if (!payment_ptr)
		return 0;

	payment = payment_ptr;
	payment_buffer = payment;

	return payment->payment_number;
}

/**
 * return the number of the payment given in param
 *
 * \param the name of the payment
 * \param the account_number
 *
 * \return the number of the payment, 0 if problem
 **/
gint gsb_data_payment_get_number_by_name (const gchar *name,
										  gint account_number)
{
	GSList *tmp_list;
	gint payment_number = 0 ;

	tmp_list = payment_list;
	while (tmp_list)
	{
		PaymentStruct *payment;

		payment = tmp_list->data;
		if (payment->account_number == account_number)
		{
			if (my_strcasecmp ((gchar *) name, payment->payment_name) == 0)
				return payment->payment_number;
		}
		tmp_list = tmp_list->next;
	}

	return payment_number;
}

/**
 * create a new payment, give him a number, append it to the list
 * and return the number
 *
 * \param name the name of the payment (can be freed after, it's a copy) or NULL
 *
 * \return the number of the new payment
 **/
gint gsb_data_payment_new (const gchar *name)
{
	PaymentStruct *payment;

	payment = g_malloc0 (sizeof (PaymentStruct));
	payment->payment_number = gsb_data_payment_max_number () + 1;

	if (name)
		payment->payment_name = my_strdup (name);
	else
		payment->payment_name = NULL;

	payment->last_number = NULL;

	payment_list = g_slist_append (payment_list, payment);
	payment_buffer = payment;

	return payment->payment_number;
}

/**
 * remove a payment
 * set all the payments of transaction which are this one to 0
 *
 * \param payment_number the payment we want to remove
 *
 * \return TRUE ok
 **/
gboolean gsb_data_payment_remove (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	payment_list = g_slist_remove (payment_list, payment);

	_gsb_data_payment_free (payment);

	return TRUE;
}

/**
 * set a new number for the payment
 * normally used only while loading the file because
 * the number are given automatically
 *
 * \param payment_number the number of the payment
 * \param new_no_payment the new number of the payment
 *
 * \return the new number or 0 if the payment doen't exist
 **/
gint gsb_data_payment_set_new_number (gint payment_number,
									  gint new_no_payment)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return 0;

	payment->payment_number = new_no_payment;

	return new_no_payment;
}

/**
 * return the account_number of the method of payment
 *
 * \param payment_number the number of the method of payment
 *
 * \return the name of the payment or 0 if fail
 **/
gint gsb_data_payment_get_account_number (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return 0;

	return payment->account_number;
}

/**
 * set the account_number of the method of payment
 *
 * \param payment_number the number of the method of payment
 * \param account_number the account_number of the method of payment
 *
 * \return TRUE if ok or FALSE if problem
 **/
gboolean gsb_data_payment_set_account_number (gint payment_number,
											  gint account_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	payment->account_number = account_number;

	return TRUE;
}

/**
 * return the name of the payment
 *
 * \param payment_number the number of the payment
 *
 * \return the name of the payment or NULL if fail
 **/
const gchar *gsb_data_payment_get_name (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return NULL;

	return payment->payment_name;
}

/**
 * set the name of the payment
 * the value is dupplicate in memory
 *
 * \param payment_number the number of the payment
 * \param name the name of the payment
 *
 * \return TRUE if ok or FALSE if problem
 **/
gboolean gsb_data_payment_set_name (gint payment_number,
									const gchar *name)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	/* we free the last name */
	if (payment->payment_name)
		g_free (payment->payment_name);

	/* and copy the new one */
	payment->payment_name = my_strdup (name);

	return TRUE;
}

/**
 * return the sign of the method of payment
 * 	GSB_PAYMENT_NEUTRAL : neutral sign (can be negative or positive)
 * 	GSB_PAYMENT_DEBIT : debit
 * 	GSB_PAYMENT_CREDIT : credit
 *
 * \param payment_number the number of the method of payment
 *
 * \return the sign of the payment or -1 if fail
 **/
gint gsb_data_payment_get_sign (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return -1;

	return payment->payment_sign;
}

/**
 * set the sign of the method of payment
 *
 * \param payment_number the number of the method of payment
 * \param sign the sign of the method of payment
 *
 * \return TRUE if ok or FALSE if problem
 **/
gboolean gsb_data_payment_set_sign (gint payment_number,
									gint sign)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	payment->payment_sign = sign;

	return TRUE;
}

/**
 * return the show_entry flag of the method of payment
 * if TRUE, choosing that method of payment will show an entry in the form
 * 	to set a number (cheque, transfer...)
 *
 * \param payment_number the number of the method of payment
 *
 * \return the show_entry flag of the payment or -1 if fail
 **/
gint gsb_data_payment_get_show_entry (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return -1;

	return payment->show_entry;
}

/**
 * set the show_entry of the method of payment
 *
 * \param payment_number the number of the method of payment
 * \param show_entry the show_entry of the method of payment
 *
 * \return TRUE if ok or FALSE if problem
 **/
gboolean gsb_data_payment_set_show_entry (gint payment_number,
										  gint show_entry)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	payment->show_entry = show_entry;

	return TRUE;
}

/**
 * return the automatic_numbering of the method of payment
 * if automatic_numbering is set and if show_entry is set,
 * 	when choosing the method of payment in the form, an entry
 * 	is showed and the content contains automatically the
 * 	last_number for that method of payment + 1
 *
 * \param payment_number the number of the method of payment
 *
 * \return the automatic_numbering of the payment or -1 if fail
 **/
gint gsb_data_payment_get_automatic_numbering (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return -1;

	return payment->automatic_numbering;
}

/**
 * set the automatic_numbering of the method of payment
 *
 * \param payment_number the number of the method of payment
 * \param automatic_numbering the automatic_numbering of the method of payment
 *
 * \return TRUE if ok or FALSE if problem
 **/
gboolean gsb_data_payment_set_automatic_numbering (gint payment_number,
												   gint automatic_numbering)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	payment->automatic_numbering = automatic_numbering;

	return TRUE;
}

/**
 * used to find easily the transfer payment number for an account
 * while importing transactions and if grisbi found a link between 2 transactions,
 * it tries to set transfer as method of payment and use that function
 * if necessary, we can modify that function to find any method of payment,
 * but it's not usefull for now...
 *
 * \param account_number	the account we want the transfer payment number
 *
 * \return the number of payment for transfer or 0 if not found
 **/
gint gsb_data_payment_get_transfer_payment_number (gint account_number)
{
	GSList *tmp_list;

	if (account_number < 0)
		return 0;

	tmp_list = payment_list;
	while (tmp_list)
	{
		PaymentStruct *payment;

		payment = tmp_list->data;
		if (payment->account_number == account_number)
			if (!my_strcasecmp (payment->payment_name, _("Transfer")))
			return payment->payment_number;

		tmp_list = tmp_list->next;
	}

	return 0;
}
/**
 * return the last_number of the method of payment
 * this is the last number to set automatically that number + 1
 * in the cheque entry ; nothing to deal with gsb_data_payment_max_number
 * which give the last payment_number, internal number to count the method of payment
 *
 * \param payment_number the number of the method of payment
 *
 * \return the last_number of the payment or -1 if fail
 **/
const gchar *gsb_data_payment_get_last_number (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return NULL;

	return payment->last_number;
}

/**
 * set the last_number of the method of payment
 *
 * \param payment_number the number of the method of payment
 * \param last_number the last_number of the method of payment
 *
 * \return TRUE if ok or FALSE if problem
 **/
gboolean gsb_data_payment_set_last_number (gint payment_number,
										   const gchar *last_number)
{
	gint old_number;
	gint new_number;
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	if (payment->last_number == NULL)
		old_number = 0;
	else
		old_number = atoi (payment->last_number);

	new_number = atoi (last_number);

	if (new_number > old_number)
		payment->last_number = my_strdup (last_number);

	return TRUE;
}

/**
 * create a set of preformated method of payment and associate
 * them to the account given in param
 * moreover, create a preformated list to sort the transaction list according to
 * 	the method of payment, and set it in the account
 *
 * \param account_number
 *
 * \return TRUE ok, FALSE problem
 **/
gint gsb_data_payment_create_default  (gint account_number)
{
	gint payment_number;
	GSList *sort_list = NULL;

	switch (gsb_data_account_get_kind (account_number))
	{
		case GSB_TYPE_BALANCE:
			break;

		case GSB_TYPE_CASH:
			break;

		case GSB_TYPE_ASSET:
			break;

		case GSB_TYPE_BANK:
			/* create the method of payment for a bank account */
			payment_number = gsb_data_payment_new (_("Transfer"));
			gsb_data_payment_set_account_number (payment_number, account_number);
			gsb_data_payment_set_show_entry (payment_number, 1);
			sort_list = g_slist_append (sort_list, GINT_TO_POINTER (payment_number));

			payment_number = gsb_data_payment_new (_("Deposit"));
			gsb_data_payment_set_account_number (payment_number, account_number);
			gsb_data_payment_set_sign (payment_number, 2);
			sort_list = g_slist_append (sort_list, GINT_TO_POINTER (payment_number));

			/* the deposit is the default credit for that account */
			gsb_data_account_set_default_credit (account_number, payment_number);

			payment_number = gsb_data_payment_new (_("Credit card"));
			gsb_data_payment_set_account_number (payment_number, account_number);
			gsb_data_payment_set_sign (payment_number, 1);
			sort_list = g_slist_append (sort_list, GINT_TO_POINTER (payment_number));

			/* the credit card is the default debit for that account */
			gsb_data_account_set_default_debit (account_number, payment_number);

			payment_number = gsb_data_payment_new (_("Direct debit"));
			gsb_data_payment_set_account_number (payment_number, account_number);
			gsb_data_payment_set_sign (payment_number, 1);
			sort_list = g_slist_append (sort_list, GINT_TO_POINTER (payment_number));

			payment_number = gsb_data_payment_new (_("Cheque"));
			gsb_data_payment_set_account_number (payment_number, account_number);
			gsb_data_payment_set_sign (payment_number, 1);
			gsb_data_payment_set_show_entry (payment_number, 1);
			gsb_data_payment_set_automatic_numbering (payment_number, 1);
			sort_list = g_slist_append (sort_list, GINT_TO_POINTER (payment_number));
			break;

		case GSB_TYPE_LIABILITIES:
			/* create the method of payment for a liabilities account */
			payment_number = gsb_data_payment_new (_("Transfer"));
			gsb_data_payment_set_account_number (payment_number, account_number);
			gsb_data_payment_set_show_entry (payment_number, 1);
			sort_list = g_slist_append (sort_list, GINT_TO_POINTER (payment_number));

			gsb_data_account_set_default_debit (account_number, payment_number);
			gsb_data_account_set_default_credit (account_number, payment_number);
	}

	if (sort_list)
		gsb_data_account_set_sort_list (account_number, sort_list);

	return TRUE;
}

/**
 * try to find a method of payment similar to the origin method of payment
 *
 * \param origin_payment	the method of payment we try to find a similar other
 * \param target_account_number	the account we want to search into for the new method of payment
 *
 * \return the similar method of payment or default method of payment if nothing found
 **/
gint gsb_data_payment_get_similar (gint origin_payment,
								   gint target_account_number)
{
	PaymentStruct *payment;
	GSList *tmp_list;

	payment = gsb_data_payment_get_structure (origin_payment);
	if (!payment)
		return 0;

	tmp_list = payment_list;

	while (tmp_list)
	{
		PaymentStruct *tmp_payment;

		tmp_payment = tmp_list->data;

		if (tmp_payment->account_number == target_account_number
			&& payment->payment_name
			&& tmp_payment->payment_name
			&& !strcmp (payment->payment_name, tmp_payment->payment_name)
			&& payment->payment_sign == tmp_payment->payment_sign)
			return tmp_payment->payment_number;

		tmp_list = tmp_list->next;
	}

	if (payment->payment_sign == 1)
		return gsb_data_account_get_default_debit (target_account_number);
	else
		return gsb_data_account_get_default_credit (target_account_number);
}

gchar *gsb_data_payment_incremente_last_number (gint payment_number,
					gint increment)
{
	const gchar *last_number;
	gchar *new_number;

	last_number = gsb_data_payment_get_last_number (payment_number);
	new_number = utils_str_incremente_number_from_str (last_number, increment);

	return new_number;
}

/**
 * return the last_number of the method of payment to int
 * this is the last number to set automatically that number + 1
 * 	in the cheque entry ; nothing to deal with gsb_data_payment_max_number
 * 	which give the last payment_number, internal number to count the method of payment
 *
 * \param payment_number the number of the method of payment
 *
 * \return the int_last_number of the payment or -1 if fail
 **/
gint gsb_data_payment_get_last_number_to_int (gint payment_number)
{
	PaymentStruct *payment;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return -1;

	return utils_str_atoi (payment->last_number);
}

/**
 * set the last_number of the method of payment from an int
 *
 * \param payment_number the number of the method of payment
 * \param last_number the last_number of the method of payment
 *
 * \return TRUE if ok or FALSE if problem
 **/
gboolean gsb_data_payment_set_last_number_from_int (gint payment_number,
													gint last_number)
{
	PaymentStruct *payment;
	const gchar *old_number = NULL;
	gchar *new_number;
	gchar *prefix = NULL;
	gint i = 0;

	payment = gsb_data_payment_get_structure (payment_number);
	if (!payment)
		return FALSE;

	old_number = gsb_data_payment_get_last_number (payment_number);
	if (old_number)
	{
		while (old_number[i] == '0')
		{
			i++;
		}
		if (i > 0)
			prefix = g_strndup (old_number, i);
	}

	new_number = utils_str_itoa (last_number);
	if (prefix && strlen (prefix))
	{
		gchar *tmp_str;
		tmp_str = g_strconcat (prefix, new_number, NULL);
		g_free(new_number);
		new_number = tmp_str;
	}
	g_free(prefix);

	payment->last_number = new_number;

	return TRUE;
}

/**
 * permet de trouver pour un autre compte le moyen de paiement
 * ayant le même nom.
 *
 * \param number initial
 * \ numéro du nouveau compte
 *
 * \ return the numéro de paiement si trouvé ou celui par défaut pour
 * le compte concerné.
 **/
gint gsb_data_payment_search_number_other_account_by_name (gint payment_number,
														   gint account_number)
{

	const gchar *name;
	GSList *tmp_list;
	gint new_payment_number;
	gint sign;

	name = gsb_data_payment_get_name (payment_number);

	tmp_list = payment_list;
	while (tmp_list)
	{
		PaymentStruct *payment;

		payment = tmp_list->data;
		if (payment->account_number == account_number)
		{
			if (my_strcasecmp ( (gchar *)name, payment->payment_name) == 0)
				return payment->payment_number;
		}
		tmp_list = tmp_list->next;
	}

	sign = gsb_data_payment_get_sign (payment_number);

	if (sign == GSB_PAYMENT_CREDIT)
		new_payment_number = gsb_data_account_get_default_credit (account_number);
	else
		new_payment_number = gsb_data_account_get_default_debit (account_number);

	return new_payment_number;
}

/**
 * This function looks for a method of payment which matches signe_type' and returns its number.
 *
 * \param account_number	The account to process.
 * \param signe_type		A payment method sign to match against payment
 *							methods of specified account.
 *							(GSB_PAYMENT_NEUTRAL, GSB_PAYMENT_DEBIT, GSB_PAYMENT_CREDIT)
 * \param exclude			A payment method that should not be checked,
 *							as it may be the one which is to be removed.
 *
 * \return					The matching payment method number on success,
 *							0 otherwise (for transfer).
 **/
gint gsb_data_payment_get_other_number_from_sign (gint account_number,
												  gint signe_type,
												  gint exclude_number)
{
	GSList *tmp_list;

	tmp_list = gsb_data_payment_get_payments_list ();
	while (tmp_list)
	{
		gint payment_number;

		payment_number = gsb_data_payment_get_number (tmp_list->data);
		if (gsb_data_payment_get_account_number (payment_number) == account_number
			&& payment_number != exclude_number
			&& gsb_data_payment_get_sign (payment_number) == signe_type)
		{
			return payment_number;
		}

		tmp_list = tmp_list->next;
	};

	/* Defaults to first method_ptr, whatever it may be */
	return 0;
}

/**
 *
 *
 * \param
 * \param
 *
 * \return
 **/
void gsb_data_payment_change_default_payment_method (gint payment_number,
													 gint account_number)
{
	switch (gsb_data_payment_get_sign (payment_number))
	{
		case GSB_PAYMENT_DEBIT:
			if (gsb_data_account_get_default_debit (account_number) == payment_number)
			{
				gsb_data_account_set_default_debit (account_number,
													gsb_data_payment_get_other_number_from_sign
													(account_number, GSB_PAYMENT_DEBIT, payment_number));
			}
			break;

		case GSB_PAYMENT_CREDIT:
			if (gsb_data_account_get_default_credit (account_number) == payment_number)
			{
				gsb_data_account_set_default_credit (account_number,
													 gsb_data_payment_get_other_number_from_sign
													 (account_number, GSB_PAYMENT_CREDIT, payment_number));
			}
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
/* Local Variables: */
/* c-basic-offset: 4 */
/* End: */
