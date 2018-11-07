#ifndef OPERATORS_ALGO_CLEARING_H
#define OPERATORS_ALGO_CLEARING_H

#include <map>
#include <unordered_set>
#include <vector>

#include "tradelayer_matrices.h"

/**************************************************************/
/** Structures for clearing algo */
struct status_amounts
{
  std::string addrs_src, status_src, addrs_trk, status_trk;
  long int lives_src, lives_trk, amount_trd, nlives_src, nlives_trk;
  double matched_price;
};

struct status_amounts_edge
{
  std::string addrs_src, status_src, addrs_trk, status_trk;
  long int lives_src, lives_trk, amount_trd, edge_row, path_number, ghost_edge;
  double entry_price, exit_price;
};

/**************************************************************/
/** Functions for clearing algo */
struct status_amounts *get_status_amounts_open_incr(VectorTLS &v, int q);

struct status_amounts *get_status_amounts_byaddrs(VectorTLS &v, std::string addrs);

VectorTLS status_open_incr(VectorTLS &status_q, int q);

VectorTLS status_netted_npartly(VectorTLS &status_q, int q);

void clearing_operator_fifo(VectorTLS &vdata, MatrixTLS &M_file, int index_init, struct status_amounts *pt_pos, int idx_long_short, int &counting_netted, long int amount_trd_sum, std::vector<std::map<std::string, std::string>> &path_main, int path_number, long int opened_contracts);

void adding_newtwocols_trdamount(MatrixTLS &M_file, MatrixTLS &database);

void settlement_algorithm_fifo(MatrixTLS &M_file);

void updating_lasttwocols_fromdatabase(std::string addrs, MatrixTLS &M_file, int i, long int live_updated);

void building_edge(std::map<std::string, std::string> &path_first, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, double entry_price, double exit_price, long int lives, int index_row, int path_number, long int amount_path, int ghost_edge);

void printing_edges(std::map<std::string, std::string> &path_first);

bool find_open_incr_anypos(std::string &s, VectorTLS *v);

bool find_open_incr_long(std::string &s, VectorTLS *v);

void computing_lives_bypath(std::vector<std::map<std::string, std::string>> &it_path_main);

struct status_amounts_edge *get_status_byedge(std::map<std::string, std::string> &edge);

void looking_netted_events(std::string &addrs_obj, std::vector<std::map<std::string, std::string>> &it_path_main, int q, long int amount_opened, int index_src_trk, std::string status_opening);

void printing_path_maini(std::vector<std::map<std::string, std::string>> &it_path_maini);

void checking_zeronetted_bypath(std::vector<std::map<std::string, std::string>> &path_maini);

bool find_netted_npartly_anypos(std::string &s, VectorTLS *v);

void computing_livesvectors_forlongshort(std::vector<std::map<std::string, std::string>> &it_path_main, std::vector<std::map<std::string, std::string>> &lives_longs, std::vector<std::map<std::string, std::string>> &lives_shorts);

void building_lives_edges(std::map<std::string, std::string> &path_first, std::string addrs, std::string status, long int lives, double entry_price, struct status_amounts_edge *pt_status_byedge);

void printing_edges_lives(std::map<std::string, std::string> &path_first);

void counting_lives_longshorts(std::vector<std::map<std::string, std::string>> &lives_longs, std::vector<std::map<std::string, std::string>> &lives_shorts);

void computing_settlement_exitprice(std::vector<std::map<std::string, std::string>> &it_path_main, long int &sum_oflives, double &PNL_total, double &gamma_p, double &gamma_q);

void calculate_pnltrk_bypath(std::vector<std::map<std::string, std::string>> &path_main, double &PNL_total, std::unordered_set<std::string> &addrs_set, std::vector<std::string> addrsv);

void listof_addresses_bypath(std::vector<std::map<std::string, std::string>> &it_path_main, std::vector<std::string> &addrsv);

double PNL_function(double entry_price, double exit_price, long int amount_trd, struct status_amounts *pt_jrow_database);

void getting_gammapq_bypath(std::vector<std::map<std::string, std::string>> &path_main, double PNL_total, double &gamma_p, double &gamma_q, std::unordered_set<std::string> addrs_set);

void calculating_ghost_edges(std::vector<std::map<std::string, std::string>> lives_longs, std::vector<std::map<std::string, std::string>> lives_shorts, double exit_price_desired, std::vector<std::map<std::string, std::string>> &ghost_edges_array);

void updating_lives_tozero(std::vector<std::vector<std::map<std::string, std::string>>> &path_main);

void joining_pathmain_ghostedges(std::vector<std::vector<std::map<std::string, std::string>>> &path_main, std::vector<std::map<std::string, std::string>> ghost_edges_array);

void checkzeronetted_bypath_ghostedges(std::vector<std::map<std::string, std::string>> path_maini, long int nonzero_lives);

long int checkpath_livesnonzero(std::vector<std::map<std::string, std::string>> path_maini);

void calculate_pnltrk_bypath(std::vector<std::map<std::string, std::string>> path_main, double &PNL_total);

#endif
