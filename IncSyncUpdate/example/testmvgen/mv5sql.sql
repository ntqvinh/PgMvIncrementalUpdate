drop table mv5;
create table mv5 (
	country_id bigint,
	country_name character varying(40),
	country_region_id character(50),
	country_region character varying(20),
	cust_id bigint,
	cust_first_name character varying(20),
	cust_last_name character varying(40),
	num_DCXmvLdEWH numeric,
	bigint_uFpIEtUxTL bigint,
	mvcount bigint
);
/*Populate the mv*/
/*insert into mv5 
SELECT   countries.country_id,   countries.country_name,   countries.country_region_id,   countries.country_region, customers.cust_id,   customers.cust_first_name,   
customers.cust_last_name,   SUM(sales.quantity_sold *  costs.unit_price) as total, COUNT(*) as cnt, COUNT(*) as cnt2 
FROM   public.countries,   public.customers,   public.sales,   public.costs 
WHERE   countries.country_id = customers.country_id 
AND  customers.cust_id = sales.cust_id AND  sales.time_id = costs.time_id 
AND  sales.promo_id = costs.promo_id 
AND  sales.channel_id = costs.channel_id AND  sales.prod_id = costs.prod_id 
GROUP BY countries.country_id,   countries.country_name,   countries.country_region_id,   
countries.country_region,  customers.cust_id, customers.cust_first_name,   customers.cust_last_name
*/
/*
Thay the gia tri: Giam 6% cost.
SELECT   countries.country_id,   countries.country_name,   countries.country_region_id,   countries.country_region, 

--customers.cust_id,   customers.cust_first_name,   customers.cust_last_name,   
104448 as cid, 'cust_first_name' as fn, 'cust_first_name' as ln,
SUM(sales.quantity_sold *  costs.unit_price) as total, COUNT(*) as cnt, COUNT(*) as cnt2 
FROM   public.countries,   
--public.customers,   
public.sales,   public.costs 
WHERE   countries.country_id = 52790 --customers.country_id 
AND  21718 --customers.cust_id 
= sales.cust_id AND  sales.time_id = costs.time_id 
AND  sales.promo_id = costs.promo_id 
AND  sales.channel_id = costs.channel_id AND  sales.prod_id = costs.prod_id 
GROUP BY countries.country_id,   countries.country_name,   countries.country_region_id,   
countries.country_region,  cid, fn, ln
--customers.cust_id, customers.cust_first_name,   customers.cust_last_name
*/

/* insert on countries*/
drop function tg_mv5_insert_on_countries() cascade;
create function tg_mv5_insert_on_countries() returns trigger as 'mv5.dll', 'tg_mv5_insert_on_countries' language c strict;
create trigger pgtg_mv5_insert_on_countries after insert on countries for each row execute procedure tg_mv5_insert_on_countries();

/* delete on countries*/
drop function tg_mv5_delete_on_countries() cascade;
create function tg_mv5_delete_on_countries() returns trigger as 'mv5.dll', 'tg_mv5_delete_on_countries' language c strict;
create trigger pgtg_mv5_before_delete_on_countries before delete on countries for each row execute procedure tg_mv5_delete_on_countries();

/* update on countries*/
drop function tg_mv5_update_on_countries() cascade;
create function tg_mv5_update_on_countries() returns trigger as 'mv5.dll', 'tg_mv5_update_on_countries' language c strict;
create trigger pgtg_mv5_before_update_on_countries before update on countries for each row execute procedure tg_mv5_update_on_countries();
create trigger pgtg_mv5_after_update_on_countries after update on countries for each row execute procedure tg_mv5_update_on_countries();

/* insert on customers*/
drop function tg_mv5_insert_on_customers() cascade;
create function tg_mv5_insert_on_customers() returns trigger as 'mv5.dll', 'tg_mv5_insert_on_customers' language c strict;
create trigger pgtg_mv5_insert_on_customers after insert on customers for each row execute procedure tg_mv5_insert_on_customers();

/* delete on customers*/
drop function tg_mv5_delete_on_customers() cascade;
create function tg_mv5_delete_on_customers() returns trigger as 'mv5.dll', 'tg_mv5_delete_on_customers' language c strict;
create trigger pgtg_mv5_before_delete_on_customers before delete on customers for each row execute procedure tg_mv5_delete_on_customers();

/* update on customers*/
drop function tg_mv5_update_on_customers() cascade;
create function tg_mv5_update_on_customers() returns trigger as 'mv5.dll', 'tg_mv5_update_on_customers' language c strict;
create trigger pgtg_mv5_before_update_on_customers before update on customers for each row execute procedure tg_mv5_update_on_customers();
create trigger pgtg_mv5_after_update_on_customers after update on customers for each row execute procedure tg_mv5_update_on_customers();

/* insert on sales*/
drop function tg_mv5_insert_on_sales() cascade;
create function tg_mv5_insert_on_sales() returns trigger as 'mv5.dll', 'tg_mv5_insert_on_sales' language c strict;
create trigger pgtg_mv5_insert_on_sales after insert on sales for each row execute procedure tg_mv5_insert_on_sales();

/* delete on sales*/
drop function tg_mv5_delete_on_sales() cascade;
create function tg_mv5_delete_on_sales() returns trigger as 'mv5.dll', 'tg_mv5_delete_on_sales' language c strict;
create trigger pgtg_mv5_before_delete_on_sales before delete on sales for each row execute procedure tg_mv5_delete_on_sales();

/* update on sales*/
drop function tg_mv5_update_on_sales() cascade;
create function tg_mv5_update_on_sales() returns trigger as 'mv5.dll', 'tg_mv5_update_on_sales' language c strict;
create trigger pgtg_mv5_before_update_on_sales before update on sales for each row execute procedure tg_mv5_update_on_sales();
create trigger pgtg_mv5_after_update_on_sales after update on sales for each row execute procedure tg_mv5_update_on_sales();

/* insert on costs*/
drop function tg_mv5_insert_on_costs() cascade;
create function tg_mv5_insert_on_costs() returns trigger as 'mv5.dll', 'tg_mv5_insert_on_costs' language c strict;
create trigger pgtg_mv5_insert_on_costs after insert on costs for each row execute procedure tg_mv5_insert_on_costs();

/* delete on costs*/
drop function tg_mv5_delete_on_costs() cascade;
create function tg_mv5_delete_on_costs() returns trigger as 'mv5.dll', 'tg_mv5_delete_on_costs' language c strict;
create trigger pgtg_mv5_before_delete_on_costs before delete on costs for each row execute procedure tg_mv5_delete_on_costs();

/* update on costs*/
drop function tg_mv5_update_on_costs() cascade;
create function tg_mv5_update_on_costs() returns trigger as 'mv5.dll', 'tg_mv5_update_on_costs' language c strict;
create trigger pgtg_mv5_before_update_on_costs before update on costs for each row execute procedure tg_mv5_update_on_costs();
create trigger pgtg_mv5_after_update_on_costs after update on costs for each row execute procedure tg_mv5_update_on_costs();