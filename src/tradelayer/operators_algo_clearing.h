#ifndef OPERATORS_ALGO_CLEARING_H
#define OPERATORS_ALGO_CLEARING_H

#include <tradelayer/tradelayer_matrices.h>
#include <map>
#include <unordered_set>
#include <vector>

extern int n_cols;

extern VectorTLS *pt_open_incr_long;
extern VectorTLS *pt_open_incr_short;
extern VectorTLS *pt_netted_npartly_long;
extern VectorTLS *pt_netted_npartly_short;
extern VectorTLS *pt_open_incr_anypos;
extern VectorTLS *pt_netted_npartly_anypos;
extern VectorTLS *pt_changepos_status;
extern MatrixTLS *pt_ndatabase;

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

struct EdgeInfo
{
  std::string addrs_src, status_src, addrs_trk, status_trk;
  long int lives_src, lives_trk, amount_trd_src, amount_trd_trk, edge_row, path_number, ghost_edge;
  double entry_price, exit_price;
};

struct status_lives_edge
{
  std::string addrs, status;
  long int lives, edge_row, path_number;
  double entry_price;
};

/**************************************************************/
/** Functions for clearing algo */
struct status_amounts *get_status_amounts_open_incr(VectorTLS &v, int q);

struct status_amounts *get_status_amounts_byaddrs(VectorTLS &v, std::string addrs);

struct EdgeInfo *GetEdgeInfo(std::map<std::string, std::string> &edge);

VectorTLS status_open_incr(VectorTLS &status_q, int q);

VectorTLS status_netted_npartly(VectorTLS &status_q, int q);

void clearing_operator_fifo(VectorTLS &vdata, MatrixTLS &M_file, int index_init, struct status_amounts *pt_pos, int idx_long_short, int &counting_netted, long int amount_trd_sum, std::vector<std::map<std::string, std::string>> &path_main, int path_number, long int opened_contracts, int idx_b);

void adding_newtwocols_trdamount(MatrixTLS &M_file, MatrixTLS &database);

void settlement_algorithm_fifo(MatrixTLS &M_file, int64_t interest, int64_t twap_price);

void updating_lasttwocols_fromdatabase(std::string addrs, MatrixTLS &M_file, int i, long int live_updated);

void building_edge(std::map<std::string, std::string> &path_first, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, double entry_price, double exit_price, long int lives, int index_row, int path_number, long int amount_path, int ghost_edge);

void building_edge(std::map<std::string, std::string> &path_first, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, double entry_price, double exit_price, long int lives_src, long int lives_trk, int index_row, int path_number, long int amount_path, int ghost_edge);

void printing_edges(std::map<std::string, std::string> &path_first);

bool find_open_incr_anypos(std::string &s, VectorTLS *v);

bool find_open_incr_long(std::string &s, VectorTLS *v);

void computing_lives_bypath(std::vector<std::map<std::string, std::string>> &it_path_main);

struct status_amounts_edge *get_status_byedge(std::map<std::string, std::string> &edge);

struct status_lives_edge *get_status_bylivesedge(std::map<std::string, std::string> &edge);

void looking_netted_events(std::string &addrs_obj, std::vector<std::map<std::string, std::string>> &it_path_main, int q, long int amount_opened, int index_src_trk, std::string status_opening);

void printing_path_maini(std::vector<std::map<std::string, std::string>> &it_path_maini);

void checking_zeronetted_bypath(std::vector<std::map<std::string, std::string>> path_maini);

bool find_netted_npartly_anypos(std::string &s, VectorTLS *v);

void computing_livesvectors_forlongshort(std::vector<std::map<std::string, std::string>> &it_path_main, std::vector<std::map<std::string, std::string>> &lives_longs, std::vector<std::map<std::string, std::string>> &lives_shorts);

void building_lives_edges(std::map<std::string, std::string> &path_first, std::string addrs, std::string status, long int lives, double entry_price, struct status_amounts_edge *pt_status_byedge);

void printing_edges_lives(std::map<std::string, std::string> &path_first);

void counting_lives_longshorts(std::vector<std::map<std::string, std::string>> &lives_longs, std::vector<std::map<std::string, std::string>> &lives_shorts);

void computing_livesvector_global(std::vector<std::map<std::string, std::string>> lives_longs, std::vector<std::map<std::string, std::string>> lives_shorts, std::vector<std::map<std::string, std::string>> &lives_longs_vg, std::vector<std::map<std::string, std::string>> &lives_shorts_vg);

void computing_settlement_exitprice(std::vector<std::map<std::string, std::string>> &it_path_main, long int &sum_oflives, double &PNL_total, double &gamma_p, double &gamma_q, int64_t interest, int64_t twap_price);

void calculate_pnltrk_bypath(std::vector<std::map<std::string, std::string>> &path_main, double &PNL_total, std::unordered_set<std::string> &addrs_set, std::vector<std::string> addrsv, int64_t interest, int64_t twap_price);

void listof_addresses_bypath(std::vector<std::map<std::string, std::string>> &it_path_main, std::vector<std::string> &addrsv);

void listof_addresses_lives(std::vector<std::map<std::string, std::string>> lives, std::vector<std::string> &addrsv);

double PNL_function(double entry_price, double exit_price, long int amount_trd, struct status_amounts *pt_jrow_database);

void getting_gammapq_bypath(std::vector<std::map<std::string, std::string>> &path_main, double PNL_total, double &gamma_p, double &gamma_q, std::unordered_set<std::string> addrs_set);

void GhostEdgesComputing(std::vector<std::map<std::string, std::string>> lives_longs, std::vector<std::map<std::string, std::string>> lives_shorts, double exit_price_desired, std::vector<std::map<std::string, std::string>> &ghost_edges_array);

void updating_lives_tozero(std::vector<std::vector<std::map<std::string, std::string>>> &path_main);

void joining_pathmain_ghostedges(std::vector<std::vector<std::map<std::string, std::string>>> &path_main, std::vector<std::map<std::string, std::string>> ghost_edges_array);

void checkzeronetted_bypath_ghostedges(std::vector<std::map<std::string, std::string>> path_maini, long int nonzero_lives);

long int checkpath_livesnonzero(std::vector<std::map<std::string, std::string>> path_maini);

void calculate_pnltrk_bypath(std::vector<std::map<std::string, std::string>> path_main, double &PNL_total);

bool find_address_lives_vector(std::vector<std::map<std::string, std::string>> lives_v, std::string address);

int find_posaddress_lives_vector(std::vector<std::map<std::string, std::string>> lives_v, std::string address);

void getting_globallives_long_short(std::vector<std::map<std::string, std::string>> lives, std::vector<std::map<std::string, std::string>> &lives_vg);

void printing_lives_vector(std::vector<std::map<std::string, std::string>> lives);

std::vector<std::string> AddressesList(std::vector<std::vector<std::map<std::string, std::string>>> &path_main);

void building_lives_edges(std::map<std::string, std::string> &path_first, std::string addrs, std::string status, long int lives, double entry_price, long int edge_row, long int path_number);

void PrintingEdge(std::map<std::string, std::string> &path_first);

void PrintingGraph(std::vector<std::map<std::string, std::string>> &it_path_maini);

std::vector<std::string> LivesNonZero(std::vector<std::vector<std::map<std::string, std::string>>> &path_main, std::vector<std::string> AddrsV);

void LivesNotZeroHelper(std::string AddrsTrk, std::string AddrsI, std::string LivesTrk, std::vector<std::string> &Result, bool &Loop);

void PushBackLives(int IdPosition, std::string AddrsLives, std::string Status, long int NLives, double EntryPrice, long int EdgeRow, long int PathNumber, std::map<std::string, std::string> LivesLongsEle, std::vector<std::map<std::string, std::string>> &LivesLongs, std::map<std::string, std::string> LivesShortsEle, std::vector<std::map<std::string, std::string>> &LivesShorts);

void IncreasedLastPos(std::vector<std::vector<std::map<std::string, std::string>>> path_main, long int LastEdgeRow, std::string AddrsLives, std::map<std::string, std::string> LivesLongsEle, std::vector<std::map<std::string, std::string>> &LivesLongs, std::map<std::string, std::string> LivesShortsEle, std::vector<std::map<std::string, std::string>> &LivesShorts);

void BuildingGhostEdges(std::map<std::string, std::string> &path_first, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, string entry_price_src, string entry_price_trk, double exit_price, long int amount_path);

void PrintingGhostEdge(std::map<std::string, std::string> &path_first);

void calculate_pnl_forghost(std::vector<std::map<std::string, std::string>> path_main, double &PNL_total);

double PNL_ghosts(double entry_price, double exit_price, long int amount_trd, std::string status);

#endif
