#include "ctrigger.h"

FUNCTION(tg_mv5_insert_on_countries) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// countries table's vars
	char * str_country_iso_code_countries_table;
	char * str_country_name_countries_table;
	char * str_country_subregion_countries_table;
	char * str_country_subregion_id_countries_table;
	char * str_country_region_countries_table;
	char * str_country_region_id_countries_table;
	char * str_country_total_countries_table;
	char * str_country_total_id_countries_table;
	char * str_country_name_hist_countries_table;
	int64 bigint_country_id_countries_table;
	char str_bigint_country_id_countries_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_STR_ON_TRIGGERED_ROW(str_country_iso_code_countries_table, 1);
	GET_STR_ON_TRIGGERED_ROW(str_country_name_countries_table, 2);
	GET_STR_ON_TRIGGERED_ROW(str_country_subregion_countries_table, 3);
	GET_STR_ON_TRIGGERED_ROW(str_country_subregion_id_countries_table, 4);
	GET_STR_ON_TRIGGERED_ROW(str_country_region_countries_table, 5);
	GET_STR_ON_TRIGGERED_ROW(str_country_region_id_countries_table, 6);
	GET_STR_ON_TRIGGERED_ROW(str_country_total_countries_table, 7);
	GET_STR_ON_TRIGGERED_ROW(str_country_total_id_countries_table, 8);
	GET_STR_ON_TRIGGERED_ROW(str_country_name_hist_countries_table, 9);
	GET_INT64_ON_TRIGGERED_ROW(bigint_country_id_countries_table, 10);
	INT64_TO_STR(bigint_country_id_countries_table, str_bigint_country_id_countries_table);

	// FCC
	if (1 ) {

		// Re-query
		DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
		ADDQ(" from ");
		ADDQ("countries, customers, sales, costs");
		ADDQ(" where true ");
		ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
		ADDQ(" and countries.country_id = ");
		ADDQ(str_bigint_country_id_countries_table);
		ADDQ(" ");
		ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
		EXEC;

		// Allocate SAVED_RESULT set
		DEFR(10);

		// Save dQ result
		FOR_EACH_RESULT_ROW(i) {
			GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
			ADDR(bigint_country_id_countries);
			GET_STR_ON_RESULT(str_country_name_countries, i, 2);
			ADDR(str_country_name_countries);
			GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
			ADDR(str_country_region_id_countries);
			GET_STR_ON_RESULT(str_country_region_countries, i, 4);
			ADDR(str_country_region_countries);
			GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
			ADDR(bigint_cust_id_customers);
			GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
			ADDR(str_cust_first_name_customers);
			GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
			ADDR(str_cust_last_name_customers);
			GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
			ADDR(num_DCXmvLdEWH);
			GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
			ADDR(bigint_uFpIEtUxTL);
			GET_STR_ON_RESULT(mvcount, i, 10);
			ADDR(mvcount);
		}

		// For each dQi:
		FOR_EACH_SAVED_RESULT_ROW(i, 10) {
			bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
			str_country_name_countries = GET_SAVED_RESULT(i, 1);
			str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
			str_country_region_countries = GET_SAVED_RESULT(i, 3);
			bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
			str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
			str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
			num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
			bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
			mvcount = GET_SAVED_RESULT(i, 9);

			// Check if there is a similar row in MV
			DEFQ("select mvcount from mv5 where true ");
			ADDQ(" and country_id = ");
			ADDQ(bigint_country_id_countries);
			ADDQ(" ");
			ADDQ(" and country_name = '");
			ADDQ(str_country_name_countries);
			ADDQ("' ");
			ADDQ(" and country_region_id = '");
			ADDQ(str_country_region_id_countries);
			ADDQ("' ");
			ADDQ(" and country_region = '");
			ADDQ(str_country_region_countries);
			ADDQ("' ");
			ADDQ(" and cust_id = ");
			ADDQ(bigint_cust_id_customers);
			ADDQ(" ");
			ADDQ(" and cust_first_name = '");
			ADDQ(str_cust_first_name_customers);
			ADDQ("' ");
			ADDQ(" and cust_last_name = '");
			ADDQ(str_cust_last_name_customers);
			ADDQ("' ");
			EXEC;

			if (NO_ROW) {
				// insert new row to mv
				DEFQ("insert into mv5 values (");
				ADDQ("");
				ADDQ(bigint_country_id_countries);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_name_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_id_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_cust_id_customers);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_first_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_last_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(num_DCXmvLdEWH);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(mvcount);
				ADDQ("");
				ADDQ(")");
				EXEC;
			} else {
				// update old row
				DEFQ("update mv5 set ");
				ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
				ADDQ(num_DCXmvLdEWH);
				ADDQ(", ");
				ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ(", ");
				ADDQ(" mvcount = mvcount + ");
				ADDQ(mvcount);
				ADDQ(" where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_delete_on_countries) {
	/*
		Variables declaration
	*/
	// countries table's vars
	int64 bigint_country_id_countries_table;
	char str_bigint_country_id_countries_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_INT64_ON_TRIGGERED_ROW(bigint_country_id_countries_table, 10);
	INT64_TO_STR(bigint_country_id_countries_table, str_bigint_country_id_countries_table);
	{
		DEFQ("delete from mv5 where true ");
		ADDQ(" and country_id = ");
		ADDQ(str_bigint_country_id_countries_table);
		ADDQ(" ");
		EXEC;

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_update_on_countries) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// countries table's vars
	char * str_country_iso_code_countries_table;
	char * str_country_name_countries_table;
	char * str_country_subregion_countries_table;
	char * str_country_subregion_id_countries_table;
	char * str_country_region_countries_table;
	char * str_country_region_id_countries_table;
	char * str_country_total_countries_table;
	char * str_country_total_id_countries_table;
	char * str_country_name_hist_countries_table;
	int64 bigint_country_id_countries_table;
	char str_bigint_country_id_countries_table[20];
	int64 new_bigint_country_id_countries_table;
	char new_str_bigint_country_id_countries_table[20];

	char * count;

	int flag;

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

		GET_INT64_ON_TRIGGERED_ROW(bigint_country_id_countries_table, 10);
		INT64_TO_STR(bigint_country_id_countries_table, str_bigint_country_id_countries_table);
		GET_INT64_ON_NEW_ROW(new_bigint_country_id_countries_table, 10);
		INT64_TO_STR(new_bigint_country_id_countries_table, new_str_bigint_country_id_countries_table);
	if (1  && STR_EQUAL(str_bigint_country_id_countries_table, new_str_bigint_country_id_countries_table)  ) {
		flag = 1;
	}
	else {
		flag = 0;
	}
	if (UTRIGGER_FIRED_BEFORE && !flag) {
		/* DELETE OLD*/
		// Get values of table's fields
		GET_STR_ON_TRIGGERED_ROW(str_country_iso_code_countries_table, 1);
		GET_STR_ON_TRIGGERED_ROW(str_country_name_countries_table, 2);
		GET_STR_ON_TRIGGERED_ROW(str_country_subregion_countries_table, 3);
		GET_STR_ON_TRIGGERED_ROW(str_country_subregion_id_countries_table, 4);
		GET_STR_ON_TRIGGERED_ROW(str_country_region_countries_table, 5);
		GET_STR_ON_TRIGGERED_ROW(str_country_region_id_countries_table, 6);
		GET_STR_ON_TRIGGERED_ROW(str_country_total_countries_table, 7);
		GET_STR_ON_TRIGGERED_ROW(str_country_total_id_countries_table, 8);
		GET_STR_ON_TRIGGERED_ROW(str_country_name_hist_countries_table, 9);
		GET_INT64_ON_TRIGGERED_ROW(bigint_country_id_countries_table, 10);
		INT64_TO_STR(bigint_country_id_countries_table, str_bigint_country_id_countries_table);

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and countries.country_id = ");
			ADDQ(str_bigint_country_id_countries_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				GET_STR_ON_RESULT(count, 0, 1);

				if (STR_EQUAL(count, mvcount)) {
					// Delete the row
					DEFQ("delete from mv5 where true");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				} else {
					// ow, decrease the count column 
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH - ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL - ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount - ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}

	} else if (!UTRIGGER_FIRED_BEFORE) {

		/* INSERT */
		// Get values of table's fields
		GET_STR_ON_NEW_ROW(str_country_iso_code_countries_table, 1);
		GET_STR_ON_NEW_ROW(str_country_name_countries_table, 2);
		GET_STR_ON_NEW_ROW(str_country_subregion_countries_table, 3);
		GET_STR_ON_NEW_ROW(str_country_subregion_id_countries_table, 4);
		GET_STR_ON_NEW_ROW(str_country_region_countries_table, 5);
		GET_STR_ON_NEW_ROW(str_country_region_id_countries_table, 6);
		GET_STR_ON_NEW_ROW(str_country_total_countries_table, 7);
		GET_STR_ON_NEW_ROW(str_country_total_id_countries_table, 8);
		GET_STR_ON_NEW_ROW(str_country_name_hist_countries_table, 9);
		GET_INT64_ON_NEW_ROW(bigint_country_id_countries_table, 10);
		INT64_TO_STR(bigint_country_id_countries_table, str_bigint_country_id_countries_table);
		if (flag) {
			DEFQ("update mv5 set mvcount = mvcount ");
			ADDQ(", country_name = '");
			ADDQ(str_country_name_countries_table);
				ADDQ("' ");
			ADDQ(", country_region_id = '");
			ADDQ(str_country_region_id_countries_table);
				ADDQ("' ");
			ADDQ(", country_region = '");
			ADDQ(str_country_region_countries_table);
				ADDQ("' ");
			ADDQ(" where true ");
			ADDQ(" and country_id = ");
			ADDQ(str_bigint_country_id_countries_table);
			ADDQ(" ");
			EXEC;
		}
		else {

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and countries.country_id = ");
			ADDQ(str_bigint_country_id_countries_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				if (NO_ROW) {
					// insert new row to mv
					DEFQ("insert into mv5 values (");
					ADDQ("");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(mvcount);
					ADDQ(" ");
					ADDQ(")");
					EXEC;
				} else {
					// update old row
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount + ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_insert_on_customers) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// customers table's vars
	char * str_cust_first_name_customers_table;
	char * str_cust_last_name_customers_table;
	char * str_cust_gender_customers_table;
	Numeric num_cust_year_of_birth_customers_table;
	char str_num_cust_year_of_birth_customers_table[20];
	char * str_cust_marital_status_customers_table;
	char * str_cust_street_address_customers_table;
	char * str_cust_postal_code_customers_table;
	char * str_cust_city_customers_table;
	char * str_cust_city_id_customers_table;
	char * str_cust_state_province_customers_table;
	char * str_cust_state_province_id_customers_table;
	char * str_cust_main_phone_number_customers_table;
	char * str_cust_income_level_customers_table;
	char * str_cust_credit_limit_customers_table;
	char * str_cust_email_customers_table;
	char * str_cust_total_customers_table;
	char * str_cust_total_id_customers_table;
	char * str_cust_src_id_customers_table;
	char * str_cust_eff_from_customers_table;
	char * str_cust_eff_to_customers_table;
	char * str_cust_valid_customers_table;
	int64 bigint_cust_id_customers_table;
	char str_bigint_cust_id_customers_table[20];
	int64 bigint_country_id_customers_table;
	char str_bigint_country_id_customers_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_STR_ON_TRIGGERED_ROW(str_cust_first_name_customers_table, 1);
	GET_STR_ON_TRIGGERED_ROW(str_cust_last_name_customers_table, 2);
	GET_STR_ON_TRIGGERED_ROW(str_cust_gender_customers_table, 3);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_cust_year_of_birth_customers_table, 4);
	NUMERIC_TO_STR(num_cust_year_of_birth_customers_table, str_num_cust_year_of_birth_customers_table);
	GET_STR_ON_TRIGGERED_ROW(str_cust_marital_status_customers_table, 5);
	GET_STR_ON_TRIGGERED_ROW(str_cust_street_address_customers_table, 6);
	GET_STR_ON_TRIGGERED_ROW(str_cust_postal_code_customers_table, 7);
	GET_STR_ON_TRIGGERED_ROW(str_cust_city_customers_table, 8);
	GET_STR_ON_TRIGGERED_ROW(str_cust_city_id_customers_table, 9);
	GET_STR_ON_TRIGGERED_ROW(str_cust_state_province_customers_table, 10);
	GET_STR_ON_TRIGGERED_ROW(str_cust_state_province_id_customers_table, 11);
	GET_STR_ON_TRIGGERED_ROW(str_cust_main_phone_number_customers_table, 12);
	GET_STR_ON_TRIGGERED_ROW(str_cust_income_level_customers_table, 13);
	GET_STR_ON_TRIGGERED_ROW(str_cust_credit_limit_customers_table, 14);
	GET_STR_ON_TRIGGERED_ROW(str_cust_email_customers_table, 15);
	GET_STR_ON_TRIGGERED_ROW(str_cust_total_customers_table, 16);
	GET_STR_ON_TRIGGERED_ROW(str_cust_total_id_customers_table, 17);
	GET_STR_ON_TRIGGERED_ROW(str_cust_src_id_customers_table, 18);
	GET_STR_ON_TRIGGERED_ROW(str_cust_eff_from_customers_table, 19);
	GET_STR_ON_TRIGGERED_ROW(str_cust_eff_to_customers_table, 20);
	GET_STR_ON_TRIGGERED_ROW(str_cust_valid_customers_table, 21);
	GET_INT64_ON_TRIGGERED_ROW(bigint_cust_id_customers_table, 22);
	INT64_TO_STR(bigint_cust_id_customers_table, str_bigint_cust_id_customers_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_country_id_customers_table, 23);
	INT64_TO_STR(bigint_country_id_customers_table, str_bigint_country_id_customers_table);

	// FCC
	if (1 ) {

		// Re-query
		DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
		ADDQ(" from ");
		ADDQ("countries, customers, sales, costs");
		ADDQ(" where true ");
		ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
		ADDQ(" and customers.cust_id = ");
		ADDQ(str_bigint_cust_id_customers_table);
		ADDQ(" ");
		ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
		EXEC;

		// Allocate SAVED_RESULT set
		DEFR(10);

		// Save dQ result
		FOR_EACH_RESULT_ROW(i) {
			GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
			ADDR(bigint_country_id_countries);
			GET_STR_ON_RESULT(str_country_name_countries, i, 2);
			ADDR(str_country_name_countries);
			GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
			ADDR(str_country_region_id_countries);
			GET_STR_ON_RESULT(str_country_region_countries, i, 4);
			ADDR(str_country_region_countries);
			GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
			ADDR(bigint_cust_id_customers);
			GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
			ADDR(str_cust_first_name_customers);
			GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
			ADDR(str_cust_last_name_customers);
			GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
			ADDR(num_DCXmvLdEWH);
			GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
			ADDR(bigint_uFpIEtUxTL);
			GET_STR_ON_RESULT(mvcount, i, 10);
			ADDR(mvcount);
		}

		// For each dQi:
		FOR_EACH_SAVED_RESULT_ROW(i, 10) {
			bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
			str_country_name_countries = GET_SAVED_RESULT(i, 1);
			str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
			str_country_region_countries = GET_SAVED_RESULT(i, 3);
			bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
			str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
			str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
			num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
			bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
			mvcount = GET_SAVED_RESULT(i, 9);

			// Check if there is a similar row in MV
			DEFQ("select mvcount from mv5 where true ");
			ADDQ(" and country_id = ");
			ADDQ(bigint_country_id_countries);
			ADDQ(" ");
			ADDQ(" and country_name = '");
			ADDQ(str_country_name_countries);
			ADDQ("' ");
			ADDQ(" and country_region_id = '");
			ADDQ(str_country_region_id_countries);
			ADDQ("' ");
			ADDQ(" and country_region = '");
			ADDQ(str_country_region_countries);
			ADDQ("' ");
			ADDQ(" and cust_id = ");
			ADDQ(bigint_cust_id_customers);
			ADDQ(" ");
			ADDQ(" and cust_first_name = '");
			ADDQ(str_cust_first_name_customers);
			ADDQ("' ");
			ADDQ(" and cust_last_name = '");
			ADDQ(str_cust_last_name_customers);
			ADDQ("' ");
			EXEC;

			if (NO_ROW) {
				// insert new row to mv
				DEFQ("insert into mv5 values (");
				ADDQ("");
				ADDQ(bigint_country_id_countries);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_name_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_id_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_cust_id_customers);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_first_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_last_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(num_DCXmvLdEWH);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(mvcount);
				ADDQ("");
				ADDQ(")");
				EXEC;
			} else {
				// update old row
				DEFQ("update mv5 set ");
				ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
				ADDQ(num_DCXmvLdEWH);
				ADDQ(", ");
				ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ(", ");
				ADDQ(" mvcount = mvcount + ");
				ADDQ(mvcount);
				ADDQ(" where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_delete_on_customers) {
	/*
		Variables declaration
	*/
	// customers table's vars
	int64 bigint_cust_id_customers_table;
	char str_bigint_cust_id_customers_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_INT64_ON_TRIGGERED_ROW(bigint_cust_id_customers_table, 22);
	INT64_TO_STR(bigint_cust_id_customers_table, str_bigint_cust_id_customers_table);
	{
		DEFQ("delete from mv5 where true ");
		ADDQ(" and cust_id = ");
		ADDQ(str_bigint_cust_id_customers_table);
		ADDQ(" ");
		EXEC;

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_update_on_customers) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// customers table's vars
	char * str_cust_first_name_customers_table;
	char * str_cust_last_name_customers_table;
	char * str_cust_gender_customers_table;
	Numeric num_cust_year_of_birth_customers_table;
	char str_num_cust_year_of_birth_customers_table[20];
	char * str_cust_marital_status_customers_table;
	char * str_cust_street_address_customers_table;
	char * str_cust_postal_code_customers_table;
	char * str_cust_city_customers_table;
	char * str_cust_city_id_customers_table;
	char * str_cust_state_province_customers_table;
	char * str_cust_state_province_id_customers_table;
	char * str_cust_main_phone_number_customers_table;
	char * str_cust_income_level_customers_table;
	char * str_cust_credit_limit_customers_table;
	char * str_cust_email_customers_table;
	char * str_cust_total_customers_table;
	char * str_cust_total_id_customers_table;
	char * str_cust_src_id_customers_table;
	char * str_cust_eff_from_customers_table;
	char * str_cust_eff_to_customers_table;
	char * str_cust_valid_customers_table;
	int64 bigint_cust_id_customers_table;
	char str_bigint_cust_id_customers_table[20];
	int64 new_bigint_cust_id_customers_table;
	char new_str_bigint_cust_id_customers_table[20];
	int64 bigint_country_id_customers_table;
	char str_bigint_country_id_customers_table[20];

	char * count;

	int flag;

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

		GET_INT64_ON_TRIGGERED_ROW(bigint_cust_id_customers_table, 22);
		INT64_TO_STR(bigint_cust_id_customers_table, str_bigint_cust_id_customers_table);
		GET_INT64_ON_NEW_ROW(new_bigint_cust_id_customers_table, 22);
		INT64_TO_STR(new_bigint_cust_id_customers_table, new_str_bigint_cust_id_customers_table);
	if (1  && STR_EQUAL(str_bigint_cust_id_customers_table, new_str_bigint_cust_id_customers_table)  ) {
		flag = 1;
	}
	else {
		flag = 0;
	}
	if (UTRIGGER_FIRED_BEFORE && !flag) {
		/* DELETE OLD*/
		// Get values of table's fields
		GET_STR_ON_TRIGGERED_ROW(str_cust_first_name_customers_table, 1);
		GET_STR_ON_TRIGGERED_ROW(str_cust_last_name_customers_table, 2);
		GET_STR_ON_TRIGGERED_ROW(str_cust_gender_customers_table, 3);
		GET_NUMERIC_ON_TRIGGERED_ROW(num_cust_year_of_birth_customers_table, 4);
		NUMERIC_TO_STR(num_cust_year_of_birth_customers_table, str_num_cust_year_of_birth_customers_table);
		GET_STR_ON_TRIGGERED_ROW(str_cust_marital_status_customers_table, 5);
		GET_STR_ON_TRIGGERED_ROW(str_cust_street_address_customers_table, 6);
		GET_STR_ON_TRIGGERED_ROW(str_cust_postal_code_customers_table, 7);
		GET_STR_ON_TRIGGERED_ROW(str_cust_city_customers_table, 8);
		GET_STR_ON_TRIGGERED_ROW(str_cust_city_id_customers_table, 9);
		GET_STR_ON_TRIGGERED_ROW(str_cust_state_province_customers_table, 10);
		GET_STR_ON_TRIGGERED_ROW(str_cust_state_province_id_customers_table, 11);
		GET_STR_ON_TRIGGERED_ROW(str_cust_main_phone_number_customers_table, 12);
		GET_STR_ON_TRIGGERED_ROW(str_cust_income_level_customers_table, 13);
		GET_STR_ON_TRIGGERED_ROW(str_cust_credit_limit_customers_table, 14);
		GET_STR_ON_TRIGGERED_ROW(str_cust_email_customers_table, 15);
		GET_STR_ON_TRIGGERED_ROW(str_cust_total_customers_table, 16);
		GET_STR_ON_TRIGGERED_ROW(str_cust_total_id_customers_table, 17);
		GET_STR_ON_TRIGGERED_ROW(str_cust_src_id_customers_table, 18);
		GET_STR_ON_TRIGGERED_ROW(str_cust_eff_from_customers_table, 19);
		GET_STR_ON_TRIGGERED_ROW(str_cust_eff_to_customers_table, 20);
		GET_STR_ON_TRIGGERED_ROW(str_cust_valid_customers_table, 21);
		GET_INT64_ON_TRIGGERED_ROW(bigint_cust_id_customers_table, 22);
		INT64_TO_STR(bigint_cust_id_customers_table, str_bigint_cust_id_customers_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_country_id_customers_table, 23);
		INT64_TO_STR(bigint_country_id_customers_table, str_bigint_country_id_customers_table);

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and customers.cust_id = ");
			ADDQ(str_bigint_cust_id_customers_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				GET_STR_ON_RESULT(count, 0, 1);

				if (STR_EQUAL(count, mvcount)) {
					// Delete the row
					DEFQ("delete from mv5 where true");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				} else {
					// ow, decrease the count column 
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH - ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL - ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount - ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}

	} else if (!UTRIGGER_FIRED_BEFORE) {

		/* INSERT */
		// Get values of table's fields
		GET_STR_ON_NEW_ROW(str_cust_first_name_customers_table, 1);
		GET_STR_ON_NEW_ROW(str_cust_last_name_customers_table, 2);
		GET_STR_ON_NEW_ROW(str_cust_gender_customers_table, 3);
		GET_NUMERIC_ON_NEW_ROW(num_cust_year_of_birth_customers_table, 4);
		NUMERIC_TO_STR(num_cust_year_of_birth_customers_table, str_num_cust_year_of_birth_customers_table);
		GET_STR_ON_NEW_ROW(str_cust_marital_status_customers_table, 5);
		GET_STR_ON_NEW_ROW(str_cust_street_address_customers_table, 6);
		GET_STR_ON_NEW_ROW(str_cust_postal_code_customers_table, 7);
		GET_STR_ON_NEW_ROW(str_cust_city_customers_table, 8);
		GET_STR_ON_NEW_ROW(str_cust_city_id_customers_table, 9);
		GET_STR_ON_NEW_ROW(str_cust_state_province_customers_table, 10);
		GET_STR_ON_NEW_ROW(str_cust_state_province_id_customers_table, 11);
		GET_STR_ON_NEW_ROW(str_cust_main_phone_number_customers_table, 12);
		GET_STR_ON_NEW_ROW(str_cust_income_level_customers_table, 13);
		GET_STR_ON_NEW_ROW(str_cust_credit_limit_customers_table, 14);
		GET_STR_ON_NEW_ROW(str_cust_email_customers_table, 15);
		GET_STR_ON_NEW_ROW(str_cust_total_customers_table, 16);
		GET_STR_ON_NEW_ROW(str_cust_total_id_customers_table, 17);
		GET_STR_ON_NEW_ROW(str_cust_src_id_customers_table, 18);
		GET_STR_ON_NEW_ROW(str_cust_eff_from_customers_table, 19);
		GET_STR_ON_NEW_ROW(str_cust_eff_to_customers_table, 20);
		GET_STR_ON_NEW_ROW(str_cust_valid_customers_table, 21);
		GET_INT64_ON_NEW_ROW(bigint_cust_id_customers_table, 22);
		INT64_TO_STR(bigint_cust_id_customers_table, str_bigint_cust_id_customers_table);
		GET_INT64_ON_NEW_ROW(bigint_country_id_customers_table, 23);
		INT64_TO_STR(bigint_country_id_customers_table, str_bigint_country_id_customers_table);
		if (flag) {
			DEFQ("update mv5 set mvcount = mvcount ");
			ADDQ(", cust_first_name = '");
			ADDQ(str_cust_first_name_customers_table);
				ADDQ("' ");
			ADDQ(", cust_last_name = '");
			ADDQ(str_cust_last_name_customers_table);
				ADDQ("' ");
			ADDQ(" where true ");
			ADDQ(" and cust_id = ");
			ADDQ(str_bigint_cust_id_customers_table);
			ADDQ(" ");
			EXEC;
		}
		else {

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and customers.cust_id = ");
			ADDQ(str_bigint_cust_id_customers_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				if (NO_ROW) {
					// insert new row to mv
					DEFQ("insert into mv5 values (");
					ADDQ("");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(mvcount);
					ADDQ(" ");
					ADDQ(")");
					EXEC;
				} else {
					// update old row
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount + ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_insert_on_sales) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// sales table's vars
	char * str_time_id_sales_table;
	Numeric num_quantity_sold_sales_table;
	char str_num_quantity_sold_sales_table[20];
	Numeric num_amount_sold_sales_table;
	char str_num_amount_sold_sales_table[20];
	int64 bigint_prod_id_sales_table;
	char str_bigint_prod_id_sales_table[20];
	int64 bigint_cust_id_sales_table;
	char str_bigint_cust_id_sales_table[20];
	int64 bigint_channel_id_sales_table;
	char str_bigint_channel_id_sales_table[20];
	int64 bigint_promo_id_sales_table;
	char str_bigint_promo_id_sales_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_STR_ON_TRIGGERED_ROW(str_time_id_sales_table, 1);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_quantity_sold_sales_table, 2);
	NUMERIC_TO_STR(num_quantity_sold_sales_table, str_num_quantity_sold_sales_table);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_amount_sold_sales_table, 3);
	NUMERIC_TO_STR(num_amount_sold_sales_table, str_num_amount_sold_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_prod_id_sales_table, 4);
	INT64_TO_STR(bigint_prod_id_sales_table, str_bigint_prod_id_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_cust_id_sales_table, 5);
	INT64_TO_STR(bigint_cust_id_sales_table, str_bigint_cust_id_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_channel_id_sales_table, 6);
	INT64_TO_STR(bigint_channel_id_sales_table, str_bigint_channel_id_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_promo_id_sales_table, 7);
	INT64_TO_STR(bigint_promo_id_sales_table, str_bigint_promo_id_sales_table);

	// FCC
	if (1 ) {

		// Re-query
		DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
		ADDQ(" from ");
		ADDQ("countries, customers, sales, costs");
		ADDQ(" where true ");
		ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
		ADDQ(" and sales.time_id = '");
		ADDQ(str_time_id_sales_table);
		ADDQ("' ");
		ADDQ(" and sales.prod_id = ");
		ADDQ(str_bigint_prod_id_sales_table);
		ADDQ(" ");
		ADDQ(" and sales.cust_id = ");
		ADDQ(str_bigint_cust_id_sales_table);
		ADDQ(" ");
		ADDQ(" and sales.channel_id = ");
		ADDQ(str_bigint_channel_id_sales_table);
		ADDQ(" ");
		ADDQ(" and sales.promo_id = ");
		ADDQ(str_bigint_promo_id_sales_table);
		ADDQ(" ");
		ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
		EXEC;

		// Allocate SAVED_RESULT set
		DEFR(10);

		// Save dQ result
		FOR_EACH_RESULT_ROW(i) {
			GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
			ADDR(bigint_country_id_countries);
			GET_STR_ON_RESULT(str_country_name_countries, i, 2);
			ADDR(str_country_name_countries);
			GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
			ADDR(str_country_region_id_countries);
			GET_STR_ON_RESULT(str_country_region_countries, i, 4);
			ADDR(str_country_region_countries);
			GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
			ADDR(bigint_cust_id_customers);
			GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
			ADDR(str_cust_first_name_customers);
			GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
			ADDR(str_cust_last_name_customers);
			GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
			ADDR(num_DCXmvLdEWH);
			GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
			ADDR(bigint_uFpIEtUxTL);
			GET_STR_ON_RESULT(mvcount, i, 10);
			ADDR(mvcount);
		}

		// For each dQi:
		FOR_EACH_SAVED_RESULT_ROW(i, 10) {
			bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
			str_country_name_countries = GET_SAVED_RESULT(i, 1);
			str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
			str_country_region_countries = GET_SAVED_RESULT(i, 3);
			bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
			str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
			str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
			num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
			bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
			mvcount = GET_SAVED_RESULT(i, 9);

			// Check if there is a similar row in MV
			DEFQ("select mvcount from mv5 where true ");
			ADDQ(" and country_id = ");
			ADDQ(bigint_country_id_countries);
			ADDQ(" ");
			ADDQ(" and country_name = '");
			ADDQ(str_country_name_countries);
			ADDQ("' ");
			ADDQ(" and country_region_id = '");
			ADDQ(str_country_region_id_countries);
			ADDQ("' ");
			ADDQ(" and country_region = '");
			ADDQ(str_country_region_countries);
			ADDQ("' ");
			ADDQ(" and cust_id = ");
			ADDQ(bigint_cust_id_customers);
			ADDQ(" ");
			ADDQ(" and cust_first_name = '");
			ADDQ(str_cust_first_name_customers);
			ADDQ("' ");
			ADDQ(" and cust_last_name = '");
			ADDQ(str_cust_last_name_customers);
			ADDQ("' ");
			EXEC;

			if (NO_ROW) {
				// insert new row to mv
				DEFQ("insert into mv5 values (");
				ADDQ("");
				ADDQ(bigint_country_id_countries);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_name_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_id_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_cust_id_customers);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_first_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_last_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(num_DCXmvLdEWH);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(mvcount);
				ADDQ("");
				ADDQ(")");
				EXEC;
			} else {
				// update old row
				DEFQ("update mv5 set ");
				ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
				ADDQ(num_DCXmvLdEWH);
				ADDQ(", ");
				ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ(", ");
				ADDQ(" mvcount = mvcount + ");
				ADDQ(mvcount);
				ADDQ(" where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_delete_on_sales) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	char * count;
	// sales table's vars
	char * str_time_id_sales_table;
	Numeric num_quantity_sold_sales_table;
	char str_num_quantity_sold_sales_table[20];
	Numeric num_amount_sold_sales_table;
	char str_num_amount_sold_sales_table[20];
	int64 bigint_prod_id_sales_table;
	char str_bigint_prod_id_sales_table[20];
	int64 bigint_cust_id_sales_table;
	char str_bigint_cust_id_sales_table[20];
	int64 bigint_channel_id_sales_table;
	char str_bigint_channel_id_sales_table[20];
	int64 bigint_promo_id_sales_table;
	char str_bigint_promo_id_sales_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_STR_ON_TRIGGERED_ROW(str_time_id_sales_table, 1);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_quantity_sold_sales_table, 2);
	NUMERIC_TO_STR(num_quantity_sold_sales_table, str_num_quantity_sold_sales_table);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_amount_sold_sales_table, 3);
	NUMERIC_TO_STR(num_amount_sold_sales_table, str_num_amount_sold_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_prod_id_sales_table, 4);
	INT64_TO_STR(bigint_prod_id_sales_table, str_bigint_prod_id_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_cust_id_sales_table, 5);
	INT64_TO_STR(bigint_cust_id_sales_table, str_bigint_cust_id_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_channel_id_sales_table, 6);
	INT64_TO_STR(bigint_channel_id_sales_table, str_bigint_channel_id_sales_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_promo_id_sales_table, 7);
	INT64_TO_STR(bigint_promo_id_sales_table, str_bigint_promo_id_sales_table);

	// FCC
	if (1 ) {

		// Re-query
		DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
		ADDQ(" from ");
		ADDQ("countries, customers, sales, costs");
		ADDQ(" where true ");
		ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
		ADDQ(" and sales.time_id = '");
		ADDQ(str_time_id_sales_table);
		ADDQ("' ");
		ADDQ(" and sales.prod_id = ");
		ADDQ(str_bigint_prod_id_sales_table);
		ADDQ(" ");
		ADDQ(" and sales.cust_id = ");
		ADDQ(str_bigint_cust_id_sales_table);
		ADDQ(" ");
		ADDQ(" and sales.channel_id = ");
		ADDQ(str_bigint_channel_id_sales_table);
		ADDQ(" ");
		ADDQ(" and sales.promo_id = ");
		ADDQ(str_bigint_promo_id_sales_table);
		ADDQ(" ");
		ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
		EXEC;

		// Allocate SAVED_RESULT set
		DEFR(10);

		// Save dQ result
		FOR_EACH_RESULT_ROW(i) {
			GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
			ADDR(bigint_country_id_countries);
			GET_STR_ON_RESULT(str_country_name_countries, i, 2);
			ADDR(str_country_name_countries);
			GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
			ADDR(str_country_region_id_countries);
			GET_STR_ON_RESULT(str_country_region_countries, i, 4);
			ADDR(str_country_region_countries);
			GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
			ADDR(bigint_cust_id_customers);
			GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
			ADDR(str_cust_first_name_customers);
			GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
			ADDR(str_cust_last_name_customers);
			GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
			ADDR(num_DCXmvLdEWH);
			GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
			ADDR(bigint_uFpIEtUxTL);
			GET_STR_ON_RESULT(mvcount, i, 10);
			ADDR(mvcount);
		}

		// For each dQi:
		FOR_EACH_SAVED_RESULT_ROW(i, 10) {
			bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
			str_country_name_countries = GET_SAVED_RESULT(i, 1);
			str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
			str_country_region_countries = GET_SAVED_RESULT(i, 3);
			bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
			str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
			str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
			num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
			bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
			mvcount = GET_SAVED_RESULT(i, 9);

			// Check if there is a similar row in MV
			DEFQ("select mvcount from mv5 where true ");
			ADDQ(" and country_id = ");
			ADDQ(bigint_country_id_countries);
			ADDQ(" ");
			ADDQ(" and country_name = '");
			ADDQ(str_country_name_countries);
			ADDQ("' ");
			ADDQ(" and country_region_id = '");
			ADDQ(str_country_region_id_countries);
			ADDQ("' ");
			ADDQ(" and country_region = '");
			ADDQ(str_country_region_countries);
			ADDQ("' ");
			ADDQ(" and cust_id = ");
			ADDQ(bigint_cust_id_customers);
			ADDQ(" ");
			ADDQ(" and cust_first_name = '");
			ADDQ(str_cust_first_name_customers);
			ADDQ("' ");
			ADDQ(" and cust_last_name = '");
			ADDQ(str_cust_last_name_customers);
			ADDQ("' ");
			EXEC;

			GET_STR_ON_RESULT(count, 0, 1);

			if (STR_EQUAL(count, mvcount)) {
				// Delete the row
				DEFQ("delete from mv5 where true");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			} else {
				// ow, decrease the count column 
				DEFQ("update mv5 set ");
				ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH - ");
				ADDQ(num_DCXmvLdEWH);
				ADDQ(", ");
				ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL - ");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ(", ");
				ADDQ(" mvcount = mvcount - ");
				ADDQ(mvcount);
				ADDQ(" where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_update_on_sales) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// sales table's vars
	char * str_time_id_sales_table;
	Numeric num_quantity_sold_sales_table;
	char str_num_quantity_sold_sales_table[20];
	Numeric num_amount_sold_sales_table;
	char str_num_amount_sold_sales_table[20];
	int64 bigint_prod_id_sales_table;
	char str_bigint_prod_id_sales_table[20];
	int64 bigint_cust_id_sales_table;
	char str_bigint_cust_id_sales_table[20];
	int64 bigint_channel_id_sales_table;
	char str_bigint_channel_id_sales_table[20];
	int64 bigint_promo_id_sales_table;
	char str_bigint_promo_id_sales_table[20];

	char * count;

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	if (UTRIGGER_FIRED_BEFORE && !0) {
		/* DELETE OLD*/
		// Get values of table's fields
		GET_STR_ON_TRIGGERED_ROW(str_time_id_sales_table, 1);
		GET_NUMERIC_ON_TRIGGERED_ROW(num_quantity_sold_sales_table, 2);
		NUMERIC_TO_STR(num_quantity_sold_sales_table, str_num_quantity_sold_sales_table);
		GET_NUMERIC_ON_TRIGGERED_ROW(num_amount_sold_sales_table, 3);
		NUMERIC_TO_STR(num_amount_sold_sales_table, str_num_amount_sold_sales_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_prod_id_sales_table, 4);
		INT64_TO_STR(bigint_prod_id_sales_table, str_bigint_prod_id_sales_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_cust_id_sales_table, 5);
		INT64_TO_STR(bigint_cust_id_sales_table, str_bigint_cust_id_sales_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_channel_id_sales_table, 6);
		INT64_TO_STR(bigint_channel_id_sales_table, str_bigint_channel_id_sales_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_promo_id_sales_table, 7);
		INT64_TO_STR(bigint_promo_id_sales_table, str_bigint_promo_id_sales_table);

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and sales.time_id = '");
			ADDQ(str_time_id_sales_table);
			ADDQ("' ");
			ADDQ(" and sales.prod_id = ");
			ADDQ(str_bigint_prod_id_sales_table);
			ADDQ(" ");
			ADDQ(" and sales.cust_id = ");
			ADDQ(str_bigint_cust_id_sales_table);
			ADDQ(" ");
			ADDQ(" and sales.channel_id = ");
			ADDQ(str_bigint_channel_id_sales_table);
			ADDQ(" ");
			ADDQ(" and sales.promo_id = ");
			ADDQ(str_bigint_promo_id_sales_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				GET_STR_ON_RESULT(count, 0, 1);

				if (STR_EQUAL(count, mvcount)) {
					// Delete the row
					DEFQ("delete from mv5 where true");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				} else {
					// ow, decrease the count column 
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH - ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL - ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount - ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}

	} else if (!UTRIGGER_FIRED_BEFORE) {

		/* INSERT */
		// Get values of table's fields
		GET_STR_ON_NEW_ROW(str_time_id_sales_table, 1);
		GET_NUMERIC_ON_NEW_ROW(num_quantity_sold_sales_table, 2);
		NUMERIC_TO_STR(num_quantity_sold_sales_table, str_num_quantity_sold_sales_table);
		GET_NUMERIC_ON_NEW_ROW(num_amount_sold_sales_table, 3);
		NUMERIC_TO_STR(num_amount_sold_sales_table, str_num_amount_sold_sales_table);
		GET_INT64_ON_NEW_ROW(bigint_prod_id_sales_table, 4);
		INT64_TO_STR(bigint_prod_id_sales_table, str_bigint_prod_id_sales_table);
		GET_INT64_ON_NEW_ROW(bigint_cust_id_sales_table, 5);
		INT64_TO_STR(bigint_cust_id_sales_table, str_bigint_cust_id_sales_table);
		GET_INT64_ON_NEW_ROW(bigint_channel_id_sales_table, 6);
		INT64_TO_STR(bigint_channel_id_sales_table, str_bigint_channel_id_sales_table);
		GET_INT64_ON_NEW_ROW(bigint_promo_id_sales_table, 7);
		INT64_TO_STR(bigint_promo_id_sales_table, str_bigint_promo_id_sales_table);

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and sales.time_id = '");
			ADDQ(str_time_id_sales_table);
			ADDQ("' ");
			ADDQ(" and sales.prod_id = ");
			ADDQ(str_bigint_prod_id_sales_table);
			ADDQ(" ");
			ADDQ(" and sales.cust_id = ");
			ADDQ(str_bigint_cust_id_sales_table);
			ADDQ(" ");
			ADDQ(" and sales.channel_id = ");
			ADDQ(str_bigint_channel_id_sales_table);
			ADDQ(" ");
			ADDQ(" and sales.promo_id = ");
			ADDQ(str_bigint_promo_id_sales_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				if (NO_ROW) {
					// insert new row to mv
					DEFQ("insert into mv5 values (");
					ADDQ("");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(mvcount);
					ADDQ(" ");
					ADDQ(")");
					EXEC;
				} else {
					// update old row
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount + ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_insert_on_costs) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// costs table's vars
	char * str_time_id_costs_table;
	Numeric num_unit_cost_costs_table;
	char str_num_unit_cost_costs_table[20];
	Numeric num_unit_price_costs_table;
	char str_num_unit_price_costs_table[20];
	int64 bigint_prod_id_costs_table;
	char str_bigint_prod_id_costs_table[20];
	int64 bigint_promo_id_costs_table;
	char str_bigint_promo_id_costs_table[20];
	int64 bigint_channel_id_costs_table;
	char str_bigint_channel_id_costs_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_STR_ON_TRIGGERED_ROW(str_time_id_costs_table, 1);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_unit_cost_costs_table, 2);
	NUMERIC_TO_STR(num_unit_cost_costs_table, str_num_unit_cost_costs_table);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_unit_price_costs_table, 3);
	NUMERIC_TO_STR(num_unit_price_costs_table, str_num_unit_price_costs_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_prod_id_costs_table, 4);
	INT64_TO_STR(bigint_prod_id_costs_table, str_bigint_prod_id_costs_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_promo_id_costs_table, 5);
	INT64_TO_STR(bigint_promo_id_costs_table, str_bigint_promo_id_costs_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_channel_id_costs_table, 6);
	INT64_TO_STR(bigint_channel_id_costs_table, str_bigint_channel_id_costs_table);

	// FCC
	if (1 ) {

		// Re-query
		DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
		ADDQ(" from ");
		ADDQ("countries, customers, sales, costs");
		ADDQ(" where true ");
		ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
		ADDQ(" and costs.time_id = '");
		ADDQ(str_time_id_costs_table);
		ADDQ("' ");
		ADDQ(" and costs.prod_id = ");
		ADDQ(str_bigint_prod_id_costs_table);
		ADDQ(" ");
		ADDQ(" and costs.promo_id = ");
		ADDQ(str_bigint_promo_id_costs_table);
		ADDQ(" ");
		ADDQ(" and costs.channel_id = ");
		ADDQ(str_bigint_channel_id_costs_table);
		ADDQ(" ");
		ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
		EXEC;

		// Allocate SAVED_RESULT set
		DEFR(10);

		// Save dQ result
		FOR_EACH_RESULT_ROW(i) {
			GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
			ADDR(bigint_country_id_countries);
			GET_STR_ON_RESULT(str_country_name_countries, i, 2);
			ADDR(str_country_name_countries);
			GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
			ADDR(str_country_region_id_countries);
			GET_STR_ON_RESULT(str_country_region_countries, i, 4);
			ADDR(str_country_region_countries);
			GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
			ADDR(bigint_cust_id_customers);
			GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
			ADDR(str_cust_first_name_customers);
			GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
			ADDR(str_cust_last_name_customers);
			GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
			ADDR(num_DCXmvLdEWH);
			GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
			ADDR(bigint_uFpIEtUxTL);
			GET_STR_ON_RESULT(mvcount, i, 10);
			ADDR(mvcount);
		}

		// For each dQi:
		FOR_EACH_SAVED_RESULT_ROW(i, 10) {
			bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
			str_country_name_countries = GET_SAVED_RESULT(i, 1);
			str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
			str_country_region_countries = GET_SAVED_RESULT(i, 3);
			bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
			str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
			str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
			num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
			bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
			mvcount = GET_SAVED_RESULT(i, 9);

			// Check if there is a similar row in MV
			DEFQ("select mvcount from mv5 where true ");
			ADDQ(" and country_id = ");
			ADDQ(bigint_country_id_countries);
			ADDQ(" ");
			ADDQ(" and country_name = '");
			ADDQ(str_country_name_countries);
			ADDQ("' ");
			ADDQ(" and country_region_id = '");
			ADDQ(str_country_region_id_countries);
			ADDQ("' ");
			ADDQ(" and country_region = '");
			ADDQ(str_country_region_countries);
			ADDQ("' ");
			ADDQ(" and cust_id = ");
			ADDQ(bigint_cust_id_customers);
			ADDQ(" ");
			ADDQ(" and cust_first_name = '");
			ADDQ(str_cust_first_name_customers);
			ADDQ("' ");
			ADDQ(" and cust_last_name = '");
			ADDQ(str_cust_last_name_customers);
			ADDQ("' ");
			EXEC;

			if (NO_ROW) {
				// insert new row to mv
				DEFQ("insert into mv5 values (");
				ADDQ("");
				ADDQ(bigint_country_id_countries);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_name_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_id_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_country_region_countries);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_cust_id_customers);
				ADDQ("");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_first_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("'");
				ADDQ(str_cust_last_name_customers);
				ADDQ("'");
				ADDQ(", ");
				ADDQ("");
				ADDQ(num_DCXmvLdEWH);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ("");
				ADDQ(", ");
				ADDQ("");
				ADDQ(mvcount);
				ADDQ("");
				ADDQ(")");
				EXEC;
			} else {
				// update old row
				DEFQ("update mv5 set ");
				ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
				ADDQ(num_DCXmvLdEWH);
				ADDQ(", ");
				ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ(", ");
				ADDQ(" mvcount = mvcount + ");
				ADDQ(mvcount);
				ADDQ(" where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_delete_on_costs) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	char * count;
	// costs table's vars
	char * str_time_id_costs_table;
	Numeric num_unit_cost_costs_table;
	char str_num_unit_cost_costs_table[20];
	Numeric num_unit_price_costs_table;
	char str_num_unit_price_costs_table[20];
	int64 bigint_prod_id_costs_table;
	char str_bigint_prod_id_costs_table[20];
	int64 bigint_promo_id_costs_table;
	char str_bigint_promo_id_costs_table[20];
	int64 bigint_channel_id_costs_table;
	char str_bigint_channel_id_costs_table[20];

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	// Get values of table's fields
	GET_STR_ON_TRIGGERED_ROW(str_time_id_costs_table, 1);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_unit_cost_costs_table, 2);
	NUMERIC_TO_STR(num_unit_cost_costs_table, str_num_unit_cost_costs_table);
	GET_NUMERIC_ON_TRIGGERED_ROW(num_unit_price_costs_table, 3);
	NUMERIC_TO_STR(num_unit_price_costs_table, str_num_unit_price_costs_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_prod_id_costs_table, 4);
	INT64_TO_STR(bigint_prod_id_costs_table, str_bigint_prod_id_costs_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_promo_id_costs_table, 5);
	INT64_TO_STR(bigint_promo_id_costs_table, str_bigint_promo_id_costs_table);
	GET_INT64_ON_TRIGGERED_ROW(bigint_channel_id_costs_table, 6);
	INT64_TO_STR(bigint_channel_id_costs_table, str_bigint_channel_id_costs_table);

	// FCC
	if (1 ) {

		// Re-query
		DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
		ADDQ(" from ");
		ADDQ("countries, customers, sales, costs");
		ADDQ(" where true ");
		ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
		ADDQ(" and costs.time_id = '");
		ADDQ(str_time_id_costs_table);
		ADDQ("' ");
		ADDQ(" and costs.prod_id = ");
		ADDQ(str_bigint_prod_id_costs_table);
		ADDQ(" ");
		ADDQ(" and costs.promo_id = ");
		ADDQ(str_bigint_promo_id_costs_table);
		ADDQ(" ");
		ADDQ(" and costs.channel_id = ");
		ADDQ(str_bigint_channel_id_costs_table);
		ADDQ(" ");
		ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
		EXEC;

		// Allocate SAVED_RESULT set
		DEFR(10);

		// Save dQ result
		FOR_EACH_RESULT_ROW(i) {
			GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
			ADDR(bigint_country_id_countries);
			GET_STR_ON_RESULT(str_country_name_countries, i, 2);
			ADDR(str_country_name_countries);
			GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
			ADDR(str_country_region_id_countries);
			GET_STR_ON_RESULT(str_country_region_countries, i, 4);
			ADDR(str_country_region_countries);
			GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
			ADDR(bigint_cust_id_customers);
			GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
			ADDR(str_cust_first_name_customers);
			GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
			ADDR(str_cust_last_name_customers);
			GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
			ADDR(num_DCXmvLdEWH);
			GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
			ADDR(bigint_uFpIEtUxTL);
			GET_STR_ON_RESULT(mvcount, i, 10);
			ADDR(mvcount);
		}

		// For each dQi:
		FOR_EACH_SAVED_RESULT_ROW(i, 10) {
			bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
			str_country_name_countries = GET_SAVED_RESULT(i, 1);
			str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
			str_country_region_countries = GET_SAVED_RESULT(i, 3);
			bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
			str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
			str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
			num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
			bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
			mvcount = GET_SAVED_RESULT(i, 9);

			// Check if there is a similar row in MV
			DEFQ("select mvcount from mv5 where true ");
			ADDQ(" and country_id = ");
			ADDQ(bigint_country_id_countries);
			ADDQ(" ");
			ADDQ(" and country_name = '");
			ADDQ(str_country_name_countries);
			ADDQ("' ");
			ADDQ(" and country_region_id = '");
			ADDQ(str_country_region_id_countries);
			ADDQ("' ");
			ADDQ(" and country_region = '");
			ADDQ(str_country_region_countries);
			ADDQ("' ");
			ADDQ(" and cust_id = ");
			ADDQ(bigint_cust_id_customers);
			ADDQ(" ");
			ADDQ(" and cust_first_name = '");
			ADDQ(str_cust_first_name_customers);
			ADDQ("' ");
			ADDQ(" and cust_last_name = '");
			ADDQ(str_cust_last_name_customers);
			ADDQ("' ");
			EXEC;

			GET_STR_ON_RESULT(count, 0, 1);

			if (STR_EQUAL(count, mvcount)) {
				// Delete the row
				DEFQ("delete from mv5 where true");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			} else {
				// ow, decrease the count column 
				DEFQ("update mv5 set ");
				ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH - ");
				ADDQ(num_DCXmvLdEWH);
				ADDQ(", ");
				ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL - ");
				ADDQ(bigint_uFpIEtUxTL);
				ADDQ(", ");
				ADDQ(" mvcount = mvcount - ");
				ADDQ(mvcount);
				ADDQ(" where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;
			}
		}

	}

	/*
		Finish
	*/
	END;
}

FUNCTION(tg_mv5_update_on_costs) {
	/*
		Variables declaration
	*/
	// MV's vars
	char *bigint_country_id_countries; // country_id (countries)
	char *str_country_name_countries; // country_name (countries)
	char *str_country_region_id_countries; // country_region_id (countries)
	char *str_country_region_countries; // country_region (countries)
	char *bigint_cust_id_customers; // cust_id (customers)
	char *str_cust_first_name_customers; // cust_first_name (customers)
	char *str_cust_last_name_customers; // cust_last_name (customers)
	char *num_DCXmvLdEWH; // sum(sales.quantity_sold*costs.unit_price) (no table)
	char *bigint_uFpIEtUxTL; // count(*) (no table)
	char *mvcount; // count(*) (no table)
	// costs table's vars
	char * str_time_id_costs_table;
	Numeric num_unit_cost_costs_table;
	char str_num_unit_cost_costs_table[20];
	Numeric num_unit_price_costs_table;
	char str_num_unit_price_costs_table[20];
	int64 bigint_prod_id_costs_table;
	char str_bigint_prod_id_costs_table[20];
	int64 bigint_promo_id_costs_table;
	char str_bigint_promo_id_costs_table[20];
	int64 bigint_channel_id_costs_table;
	char str_bigint_channel_id_costs_table[20];

	char * count;

	/*
		Standard trigger preparing procedures
	*/
	REQUIRED_PROCEDURES;

	if (UTRIGGER_FIRED_BEFORE && !0) {
		/* DELETE OLD*/
		// Get values of table's fields
		GET_STR_ON_TRIGGERED_ROW(str_time_id_costs_table, 1);
		GET_NUMERIC_ON_TRIGGERED_ROW(num_unit_cost_costs_table, 2);
		NUMERIC_TO_STR(num_unit_cost_costs_table, str_num_unit_cost_costs_table);
		GET_NUMERIC_ON_TRIGGERED_ROW(num_unit_price_costs_table, 3);
		NUMERIC_TO_STR(num_unit_price_costs_table, str_num_unit_price_costs_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_prod_id_costs_table, 4);
		INT64_TO_STR(bigint_prod_id_costs_table, str_bigint_prod_id_costs_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_promo_id_costs_table, 5);
		INT64_TO_STR(bigint_promo_id_costs_table, str_bigint_promo_id_costs_table);
		GET_INT64_ON_TRIGGERED_ROW(bigint_channel_id_costs_table, 6);
		INT64_TO_STR(bigint_channel_id_costs_table, str_bigint_channel_id_costs_table);

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and costs.time_id = '");
			ADDQ(str_time_id_costs_table);
			ADDQ("' ");
			ADDQ(" and costs.prod_id = ");
			ADDQ(str_bigint_prod_id_costs_table);
			ADDQ(" ");
			ADDQ(" and costs.promo_id = ");
			ADDQ(str_bigint_promo_id_costs_table);
			ADDQ(" ");
			ADDQ(" and costs.channel_id = ");
			ADDQ(str_bigint_channel_id_costs_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				GET_STR_ON_RESULT(count, 0, 1);

				if (STR_EQUAL(count, mvcount)) {
					// Delete the row
					DEFQ("delete from mv5 where true");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				} else {
					// ow, decrease the count column 
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH - ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL - ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount - ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}

	} else if (!UTRIGGER_FIRED_BEFORE) {

		/* INSERT */
		// Get values of table's fields
		GET_STR_ON_NEW_ROW(str_time_id_costs_table, 1);
		GET_NUMERIC_ON_NEW_ROW(num_unit_cost_costs_table, 2);
		NUMERIC_TO_STR(num_unit_cost_costs_table, str_num_unit_cost_costs_table);
		GET_NUMERIC_ON_NEW_ROW(num_unit_price_costs_table, 3);
		NUMERIC_TO_STR(num_unit_price_costs_table, str_num_unit_price_costs_table);
		GET_INT64_ON_NEW_ROW(bigint_prod_id_costs_table, 4);
		INT64_TO_STR(bigint_prod_id_costs_table, str_bigint_prod_id_costs_table);
		GET_INT64_ON_NEW_ROW(bigint_promo_id_costs_table, 5);
		INT64_TO_STR(bigint_promo_id_costs_table, str_bigint_promo_id_costs_table);
		GET_INT64_ON_NEW_ROW(bigint_channel_id_costs_table, 6);
		INT64_TO_STR(bigint_channel_id_costs_table, str_bigint_channel_id_costs_table);

		// FCC
		if (1 ) {

			// Re-query
			DEFQ("select countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name, sum(sales.quantity_sold*costs.unit_price), count(*), count(*)");
			ADDQ(" from ");
			ADDQ("countries, customers, sales, costs");
			ADDQ(" where true ");
			ADDQ(" and countries.country_id = customers.country_id AND customers.cust_id = sales.cust_id AND sales.time_id = costs.time_id AND sales.promo_id = costs.promo_id AND sales.channel_id = costs.channel_id AND sales.prod_id = costs.prod_id");
			ADDQ(" and costs.time_id = '");
			ADDQ(str_time_id_costs_table);
			ADDQ("' ");
			ADDQ(" and costs.prod_id = ");
			ADDQ(str_bigint_prod_id_costs_table);
			ADDQ(" ");
			ADDQ(" and costs.promo_id = ");
			ADDQ(str_bigint_promo_id_costs_table);
			ADDQ(" ");
			ADDQ(" and costs.channel_id = ");
			ADDQ(str_bigint_channel_id_costs_table);
			ADDQ(" ");
			ADDQ(" group by countries.country_id, countries.country_name, countries.country_region_id, countries.country_region, customers.cust_id, customers.cust_first_name, customers.cust_last_name");
			EXEC;

			// Allocate SAVED_RESULT set
			DEFR(10);

			// Save dQ result
			FOR_EACH_RESULT_ROW(i) {
				GET_STR_ON_RESULT(bigint_country_id_countries, i, 1);
				ADDR(bigint_country_id_countries);
				GET_STR_ON_RESULT(str_country_name_countries, i, 2);
				ADDR(str_country_name_countries);
				GET_STR_ON_RESULT(str_country_region_id_countries, i, 3);
				ADDR(str_country_region_id_countries);
				GET_STR_ON_RESULT(str_country_region_countries, i, 4);
				ADDR(str_country_region_countries);
				GET_STR_ON_RESULT(bigint_cust_id_customers, i, 5);
				ADDR(bigint_cust_id_customers);
				GET_STR_ON_RESULT(str_cust_first_name_customers, i, 6);
				ADDR(str_cust_first_name_customers);
				GET_STR_ON_RESULT(str_cust_last_name_customers, i, 7);
				ADDR(str_cust_last_name_customers);
				GET_STR_ON_RESULT(num_DCXmvLdEWH, i, 8);
				ADDR(num_DCXmvLdEWH);
				GET_STR_ON_RESULT(bigint_uFpIEtUxTL, i, 9);
				ADDR(bigint_uFpIEtUxTL);
				GET_STR_ON_RESULT(mvcount, i, 10);
				ADDR(mvcount);
			}

			// For each dQi:
			FOR_EACH_SAVED_RESULT_ROW(i, 10) {
				bigint_country_id_countries = GET_SAVED_RESULT(i, 0);
				str_country_name_countries = GET_SAVED_RESULT(i, 1);
				str_country_region_id_countries = GET_SAVED_RESULT(i, 2);
				str_country_region_countries = GET_SAVED_RESULT(i, 3);
				bigint_cust_id_customers = GET_SAVED_RESULT(i, 4);
				str_cust_first_name_customers = GET_SAVED_RESULT(i, 5);
				str_cust_last_name_customers = GET_SAVED_RESULT(i, 6);
				num_DCXmvLdEWH = GET_SAVED_RESULT(i, 7);
				bigint_uFpIEtUxTL = GET_SAVED_RESULT(i, 8);
				mvcount = GET_SAVED_RESULT(i, 9);

				// Check if there is a similar row in MV
				DEFQ("select mvcount from mv5 where true ");
				ADDQ(" and country_id = ");
				ADDQ(bigint_country_id_countries);
				ADDQ(" ");
				ADDQ(" and country_name = '");
				ADDQ(str_country_name_countries);
				ADDQ("' ");
				ADDQ(" and country_region_id = '");
				ADDQ(str_country_region_id_countries);
				ADDQ("' ");
				ADDQ(" and country_region = '");
				ADDQ(str_country_region_countries);
				ADDQ("' ");
				ADDQ(" and cust_id = ");
				ADDQ(bigint_cust_id_customers);
				ADDQ(" ");
				ADDQ(" and cust_first_name = '");
				ADDQ(str_cust_first_name_customers);
				ADDQ("' ");
				ADDQ(" and cust_last_name = '");
				ADDQ(str_cust_last_name_customers);
				ADDQ("' ");
				EXEC;

				if (NO_ROW) {
					// insert new row to mv
					DEFQ("insert into mv5 values (");
					ADDQ("");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("'");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(" ");
					ADDQ(", ");
					ADDQ("");
					ADDQ(mvcount);
					ADDQ(" ");
					ADDQ(")");
					EXEC;
				} else {
					// update old row
					DEFQ("update mv5 set ");
					ADDQ(" num_DCXmvLdEWH = num_DCXmvLdEWH + ");
					ADDQ(num_DCXmvLdEWH);
					ADDQ(", ");
					ADDQ(" bigint_uFpIEtUxTL = bigint_uFpIEtUxTL + ");
					ADDQ(bigint_uFpIEtUxTL);
					ADDQ(", ");
					ADDQ(" mvcount = mvcount + ");
					ADDQ(mvcount);
					ADDQ(" where true ");
					ADDQ(" and country_id = ");
					ADDQ(bigint_country_id_countries);
					ADDQ(" ");
					ADDQ(" and country_name = '");
					ADDQ(str_country_name_countries);
					ADDQ("' ");
					ADDQ(" and country_region_id = '");
					ADDQ(str_country_region_id_countries);
					ADDQ("' ");
					ADDQ(" and country_region = '");
					ADDQ(str_country_region_countries);
					ADDQ("' ");
					ADDQ(" and cust_id = ");
					ADDQ(bigint_cust_id_customers);
					ADDQ(" ");
					ADDQ(" and cust_first_name = '");
					ADDQ(str_cust_first_name_customers);
					ADDQ("' ");
					ADDQ(" and cust_last_name = '");
					ADDQ(str_cust_last_name_customers);
					ADDQ("' ");
					EXEC;
				}
			}
		}

	}

	/*
		Finish
	*/
	END;
}

