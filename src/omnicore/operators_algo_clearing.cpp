#include "omnicore/operators_algo_clearing.h"
#include "omnicore/tradelayer_matrices.h"
#include "omnicore/externfns.h"
#include "omnicore/log.h"
#include "omnicore/mdex.h"
#include "omnicore/omnicore.h"
#include "amount.h"
#include <unordered_set>
#include <limits>
#include <iostream>
#include "omnicore/uint256_extensions.h"
#include "omnicore/parse_string.h"

typedef boost::multiprecision::cpp_dec_float_100 dec_float;

extern VectorTLS *pt_netted_npartly_anypos;
extern VectorTLS *pt_open_incr_anypos;
extern VectorTLS *pt_open_incr_long;
extern MatrixTLS *pt_ndatabase; MatrixTLS &ndatabase = *pt_ndatabase;

/**************************************************************/
/** Functions for Settlement Algorithm */
struct status_amounts *get_status_amounts_open_incr(VectorTLS &v, int q)
{
  struct status_amounts *pt_status = new status_amounts;
  VectorTLS status_z(2);
  VectorTLS status_q = status_open_incr(status_z, q);

  if ( finding(v[1], status_q) )
    {
      pt_status->addrs_trk  = v[0];
      pt_status->status_trk = v[1];
      pt_status->lives_trk  = stol(v[2].c_str());
      pt_status->addrs_src  = v[3];
      pt_status->status_src = v[4];
      pt_status->lives_src  = stol(v[5].c_str());
      pt_status->nlives_trk = stol(v[8].c_str());
      pt_status->nlives_src = stol(v[9].c_str());
    }
  else
    {
      pt_status->addrs_src  = v[0];
      pt_status->status_src = v[1];
      pt_status->lives_src  = stol(v[2].c_str());
      pt_status->addrs_trk  = v[3];
      pt_status->status_trk = v[4];
      pt_status->lives_trk  = stol(v[5].c_str());
      pt_status->nlives_src = stol(v[8].c_str());
      pt_status->nlives_trk = stol(v[9].c_str());
    }
  pt_status->amount_trd    = stol(v[6].c_str());
  pt_status->matched_price = stod(v[7].c_str());
  return pt_status;
}

struct status_amounts *get_status_amounts_byaddrs(VectorTLS &v, std::string addrs)
{
  struct status_amounts *pt_status = new status_amounts;
  
  if ( addrs == v[0] )
    {
      pt_status->addrs_trk  = v[0];
      pt_status->status_trk = v[1];
      pt_status->lives_trk  = stol(v[2].c_str());
      pt_status->addrs_src  = v[3];
      pt_status->status_src = v[4];
      pt_status->lives_src  = stol(v[5].c_str());
      pt_status->nlives_trk = stol(v[8].c_str());
      pt_status->nlives_src = stol(v[9].c_str());
    }
  else
    {
      pt_status->addrs_src  = v[0];
      pt_status->status_src = v[1];
      pt_status->lives_src  = stol(v[2].c_str());
      pt_status->addrs_trk  = v[3];
      pt_status->status_trk = v[4];
      pt_status->lives_trk  = stol(v[5].c_str());
      pt_status->nlives_src = stol(v[8].c_str());
      pt_status->nlives_trk = stol(v[9].c_str());
    }
  pt_status->amount_trd    = stol(v[6].c_str());
  pt_status->matched_price = stod(v[7].c_str());
  
  return pt_status;
}

struct status_amounts_edge *get_status_byedge(std::map<std::string, std::string> &edge)
{
  struct status_amounts_edge *pt_status = new status_amounts_edge;

  pt_status->addrs_src   = edge["addrs_src"];
  pt_status->addrs_trk   = edge["addrs_trk"];
  pt_status->status_src  = edge["status_src"];
  pt_status->status_trk  = edge["status_trk"];
  pt_status->entry_price = stod(edge["entry_price"]);
  pt_status->exit_price  = stod(edge["exit_price"]);
  pt_status->lives_src   = stol(edge["lives_src"]);
  pt_status->lives_trk   = stol(edge["lives_trk"]);
  pt_status->amount_trd  = stol(edge["amount_trd"]);
  pt_status->edge_row    = stol(edge["edge_row"]);
  pt_status->path_number = stol(edge["path_number"]);
  pt_status->ghost_edge  = stol(edge["ghost_edge"]);
  
  return pt_status;
}

struct status_lives_edge *get_status_bylivesedge(std::map<std::string, std::string> &edge)
{
  struct status_lives_edge *pt_status = new status_lives_edge;
  
  pt_status->addrs       = edge["addrs"];
  pt_status->status      = edge["status"];
  pt_status->lives       = stol(edge["lives"]);
  pt_status->entry_price = stod(edge["entry_price"]);
  pt_status->edge_row    = stol(edge["edge_row"]);
  pt_status->path_number = stol(edge["path_number"]);
  
  return pt_status;
}

VectorTLS status_open_incr(VectorTLS &status_q, int q)
{
  extern VectorTLS *pt_open_incr_long; VectorTLS &open_incr_long = *pt_open_incr_long;
  extern VectorTLS *pt_open_incr_short; VectorTLS &open_incr_short = *pt_open_incr_short;
  
  if ( q == 0 )
    {
      status_q[0] = open_incr_long[0];
      status_q[1] = open_incr_long[1];
    }
  else
    {
      status_q[0] = open_incr_short[0];
      status_q[1] = open_incr_short[1];
    }
  return status_q;
}

VectorTLS status_netted_npartly(VectorTLS &status_q, int q)
{
  extern VectorTLS *pt_netted_npartly_long;  VectorTLS &netted_npartly_long  = *pt_netted_npartly_long;
  extern VectorTLS *pt_netted_npartly_short; VectorTLS &netted_npartly_short = *pt_netted_npartly_short;

  if ( q == 0 )
    {
      status_q[0] = netted_npartly_long[0];
      status_q[1] = netted_npartly_long[1];
    }
  else
    {
      status_q[0] = netted_npartly_short[0];
      status_q[1] = netted_npartly_short[1];
    }
  return status_q;
}

void adding_newtwocols_trdamount(MatrixTLS &M_file, MatrixTLS &database)
{
  int N =  size(database, 1);
  for (int i = 0; i < size(database, 0); ++i)
    {
      for (int j = 0; j < N; ++j) M_file[i][j] = database[i][j];
      M_file[i][N] = database[i][6].c_str();
      M_file[i][N+1] = database[i][6].c_str();
    }
}

void settlement_algorithm_fifo(MatrixTLS &M_file, int64_t interest, int64_t twap_price)
{
  extern int n_cols;
  extern int n_rows;
  extern VectorTLS *pt_open_incr_long;  VectorTLS &open_incr_long  = *pt_open_incr_long;
  extern VectorTLS *pt_open_incr_short; VectorTLS &open_incr_short = *pt_open_incr_short;
  extern std::vector<std::map<std::string, std::string>> lives_longs_vg;
  extern std::vector<std::map<std::string, std::string>> lives_shorts_vg;
  std::vector<std::vector<std::map<std::string, std::string>>> path_main;
  std::vector<std::vector<std::map<std::string, std::string>>>::iterator it_path_main;
  std::vector<std::map<std::string, std::string>>::iterator it_path_maini;
  std::map<std::string, std::string> edge_source;
  
  int path_number = 0;
  VectorTLS vdata(n_cols);
  
  for (int i = 0; i < n_rows; ++i)
    {
      for (int j = 0; j < n_cols; ++j) vdata[j] = M_file[i][j];
      
      struct status_amounts *pt_vdata_long  = get_status_amounts_open_incr(vdata, 0);
      struct status_amounts *pt_vdata_short = get_status_amounts_open_incr(vdata, 1);
      
      std::vector<std::map<std::string, std::string>> path_maini;

      //if ( finding(pt_vdata_long->status_trk, open_incr_long) && finding(pt_vdata_short->status_trk, open_incr_short) )
      if (finding(pt_vdata_long->status_trk, open_incr_long) || finding(pt_vdata_short->status_trk, open_incr_short))
        {
	  path_number += 1;
	  
	  //PrintToLog("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	  //PrintToLog("New Edge Source: Row #%d\n\n", i);
	  building_edge(edge_source, pt_vdata_long->addrs_src, pt_vdata_long->addrs_trk, pt_vdata_long->status_src,
			pt_vdata_long->status_trk, pt_vdata_long->matched_price, pt_vdata_long->matched_price, 0, i, path_number,
			pt_vdata_long->amount_trd, 0);
	  path_maini.push_back(edge_source);
	  //printing_edges(edge_source);
	  
	  if (finding(pt_vdata_long->status_trk, open_incr_long))
	    {
	      int counting_netted_long = 0;
	      long int amount_trd_sum_long = 0;
	      
	      // PrintToLog("\n*************************************************");
	      // PrintToLog("\nTracking Long Position:");
	      clearing_operator_fifo(vdata, M_file, i, pt_vdata_long, 0, counting_netted_long, amount_trd_sum_long, path_maini,
				     path_number, pt_vdata_long->nlives_trk);
	    }
	  
	  if(finding(pt_vdata_short->status_trk, open_incr_short))
	    {
	      int counting_netted_short = 0;
	      long int amount_trd_sum_short = 0;
	      // PrintToLog("\n*************************************************");
	      // PrintToLog("\nTracking Short Position:");
	      clearing_operator_fifo(vdata, M_file, i, pt_vdata_short, 1, counting_netted_short, amount_trd_sum_short, path_maini,
				     path_number, pt_vdata_short->nlives_trk);
	    }
	  //PrintToLog("\n\nPath #%d:\n\n", path_number);
	  // for (std::vector<std::map<std::string, std::string>>::iterator it = path_maini.begin(); it != path_maini.end(); ++it)
	  //   printing_edges(*it);
        }
      if ( path_maini.size() != 0 ) path_main.push_back(path_maini);
    }
  // PrintToLog("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  // PrintToLog("\nChecking Lives by Path:\n\n");
  
  std::vector<std::map<std::string, std::string>> lives_longs;
  std::vector<std::map<std::string, std::string>> lives_shorts;
  std::vector<std::map<std::string, std::string>> ghost_edges_array;

  double vwap_exit_price = 0;
  double PNL_total = 0;
  int counting_paths = 0;
  long int sum_oflives = 0;
  double exit_price_desired = 0;
  double gamma_p = 0;
  double gamma_q = 0;
  double sum_gamma_p = 0;
  double sum_gamma_q = 0;
  
  for (it_path_main = path_main.begin(); it_path_main != path_main.end(); ++it_path_main)
    {
      //PrintToLog("*******************************************");
      counting_paths += 1;
      //PrintToLog("\n\nPath for Exit Price #%d:\n\n", counting_paths);
      //printing_path_maini(*it_path_main);
      checking_zeronetted_bypath(*it_path_main);
      computing_livesvectors_forlongshort(*it_path_main, lives_longs, lives_shorts);
      computing_settlement_exitprice(*it_path_main, sum_oflives, PNL_total, gamma_p, gamma_q, interest, twap_price);
      //PrintToLog("\ngamma_p : %d, gamma_q : %d, PNL_total : %d\n", gamma_p, gamma_q, PNL_total);
      sum_gamma_p += gamma_p;
      sum_gamma_q += gamma_q;
    }
  exit_price_desired = sum_gamma_p/sum_gamma_q;
  PrintToLog("\nexit_price_desired = %d\n", exit_price_desired);
  
  /**********************************************************************/
  /** Checking VWAP Price for Settlement**/
  
  int64_t VWAPContractPrice = mastercore::getVWAPPriceContracts("ALL F18");
  //PrintToLog("\nVWAPContractPrice = %s\n", FormatDivisibleMP(VWAPContractPrice));
  vwap_exit_price = static_cast<long double>(VWAPContractPrice)/COIN;
  
  int64_t VWAPMetaDExPrice = mastercore::getVWAPPriceByPair("ALL", "dUSD");
  PrintToLog("\nVWAPMetaDExPrice = %s\n", FormatDivisibleMP(VWAPMetaDExPrice));
  
  /**********************************************************************/

  // PrintToLog("\n----------------------------------------------------------\n");
  // PrintToLog("\n\nLocal Vector of Longs Lives\n\n");
  // printing_lives_vector(lives_longs);
  
  // PrintToLog("\n\nLocal Vector of Shorts Lives\n\n");
  // printing_lives_vector(lives_shorts);

  /**********************************************************************/

  counting_lives_longshorts(lives_longs, lives_shorts);

  /**********************************************************************/

  // PrintToLog("\n----------------------------------------------------------\n");
  
  // PrintToLog("\nGhost Edges Vector:\n");
  calculating_ghost_edges(lives_longs, lives_shorts, vwap_exit_price, ghost_edges_array);
  
  std::vector<std::map<std::string, std::string>>::iterator it_ghost;
  //for (it_ghost = ghost_edges_array.begin(); it_ghost != ghost_edges_array.end(); ++it_ghost) printing_edges(*it_ghost);
  
  //PrintToLog("\n__________________________________________________________\n");
  
  joining_pathmain_ghostedges(path_main, ghost_edges_array);
  int k = 0;
  long int nonzero_lives;
  double PNL_totalit;
  
  std::unordered_set<std::string> addrs_set;
  std::vector<std::string> addrsv;
  
  for (it_path_main = path_main.begin(); it_path_main != path_main.end(); ++it_path_main)
    {
      k += 1;
      //PrintToLog("\nPath #%d: with Ghost Nodes\n", k);
      //printing_path_maini(*it_path_main);
      nonzero_lives = checkpath_livesnonzero(*it_path_main);
      checkzeronetted_bypath_ghostedges(*it_path_main, nonzero_lives);
      listof_addresses_bypath(*it_path_main, addrsv);
      //PrintToLog("\nComputing PNL in this Path\n");
      calculate_pnltrk_bypath(*it_path_main, PNL_totalit, addrs_set, addrsv, interest, twap_price);
      PNL_total += PNL_totalit;
      //PrintToLog("\nPNL_total_main sum: %f\n", PNL_total);
      addrs_set.clear();
    }
  
  // PrintToLog("\n____________________________________________________\n");
  // PrintToLog("\nChecking PNL global (Total Sum PNL by Path):\n");
  // PrintToLog("\nPNL_total_main = %f", PNL_total);
}

void clearing_operator_fifo(VectorTLS &vdata, MatrixTLS &M_file, int index_init, struct status_amounts *pt_pos, int idx_long_short, int &counting_netted, long int amount_trd_sum, std::vector<std::map<std::string, std::string>> &path_main, int path_number, long int opened_contracts)
{
  extern int n_rows;
  extern int n_cols;
  
  VectorTLS status_z(2);
  std::string addrs_opening = pt_pos->addrs_trk;
  long int d_amounts = 0;
  std::map<std::string, std::string> path_first;

  for (int i = index_init+1; i < n_rows; ++i)
    {
      VectorTLS jrow_database(n_cols);
      sub_row(jrow_database, M_file, i);
      
      if ( finding(addrs_opening, jrow_database) )
        {
	  struct status_amounts *pt_status_addrs_trk = get_status_amounts_byaddrs(jrow_database, addrs_opening);
	  VectorTLS status_netted = status_netted_npartly(status_z, idx_long_short);
	  
	  if ( finding(pt_status_addrs_trk->status_trk, status_netted) && pt_status_addrs_trk->nlives_trk != 0 )
	    {
	      counting_netted +=1;
	      amount_trd_sum += pt_status_addrs_trk->nlives_trk;
	      //PrintToLog("\n\nNetted Event in the Row #%d\t Address Tracked: %s\n", i, addrs_opening.c_str());
	      // printing_vector(jrow_database);
	      // PrintToLog("\n\nopened_contracts = %d, nlives_trk = %d, amount_trd_sum = %d\n", opened_contracts, pt_status_addrs_trk->nlives_trk, amount_trd_sum);
	      d_amounts = opened_contracts - amount_trd_sum;
	      //PrintToLog("\n\nReview of d_amounts before cases:\t%ld", d_amounts);
	      
	      if ( d_amounts > 0 )
		{
		  //PrintToLog("\n\nd_amounts = %ld > 0", d_amounts);
		  updating_lasttwocols_fromdatabase(addrs_opening, M_file, i, 0);
		  
		  // PrintToLog("\n\nCheck last two columns Updated: Row #%d \n %s \t %s", i, M_file[i][8], M_file[i][9]);
		  // PrintToLog("\nOpened contrats: %ld > Sum amounts traded: %ld\n", opened_contracts, amount_trd_sum);
		  
		  building_edge(path_first, pt_status_addrs_trk->addrs_src, pt_status_addrs_trk->addrs_trk,
				pt_status_addrs_trk->status_src, pt_status_addrs_trk->status_trk, pt_pos->matched_price,
				pt_status_addrs_trk->matched_price, 0, i, path_number, pt_status_addrs_trk->nlives_trk, 0);
		  path_main.push_back(path_first);
		  // PrintToLog("\nEdge:\n");
		  // printing_edges(path_first);
		  
		  //continue;
		}
	      if ( d_amounts < 0)
		{
		  //PrintToLog("\n\nd_amounts = %ld < 0", d_amounts);
		  updating_lasttwocols_fromdatabase(addrs_opening, M_file, i, labs(d_amounts));
		  // PrintToLog("\n\nCheck last two columns Updated: Row #%d \n %s \t %s", i, M_file[i][8], M_file[i][9]);
		  // PrintToLog("\nOpened contrats: %ld < Sum amounts traded: %ld\n", opened_contracts, amount_trd_sum);
		  
		  building_edge(path_first, pt_status_addrs_trk->addrs_src, pt_status_addrs_trk->addrs_trk, pt_status_addrs_trk->status_src, pt_status_addrs_trk->status_trk, pt_pos->matched_price, pt_status_addrs_trk->matched_price, 0, i, path_number, pt_status_addrs_trk->nlives_trk-labs(d_amounts), 0);
		  path_main.push_back(path_first);
		  // PrintToLog("\nEdge:\n");
		  // printing_edges(path_first);
		  
		  break;
		}
	      if ( d_amounts == 0)
		{
		  //PrintToLog("\n\nd_amounts = %ld = 0", d_amounts);
		  updating_lasttwocols_fromdatabase(addrs_opening, M_file, i, 0);
		  //PrintToLog("\n\nCheck last two columns Updated: Row #%d \n %s \t %s", i, M_file[i][8], M_file[i][9]);
		  //PrintToLog("\nOpened contrats: %ld = Sum amounts traded: %ld\n", opened_contracts, amount_trd_sum);

		  building_edge(path_first, pt_status_addrs_trk->addrs_src, pt_status_addrs_trk->addrs_trk,
				pt_status_addrs_trk->status_src, pt_status_addrs_trk->status_trk, pt_pos->matched_price,
				pt_status_addrs_trk->matched_price, 0, i, path_number, pt_status_addrs_trk->nlives_trk, 0);
		  path_main.push_back(path_first);
		  //PrintToLog("\nEdge:\n");
		  //printing_edges(path_first);
		  
		  break;
		}
	    }
        }
    }
  /*****************************************************************/
  /** Computing Lives by Address **/
  if (counting_netted == 0)
    {
      if (addrs_opening == path_main[0]["addrs_src"])
	path_main[0]["lives_src"]=std::to_string(opened_contracts);
      else
	path_main[0]["lives_trk"]=std::to_string(opened_contracts);
    }
  else
    {
      if (d_amounts > 0)
	{
	  if (addrs_opening == path_main[0]["addrs_src"])
	    path_main[0]["lives_src"]=std::to_string(d_amounts);
	  else
	    path_main[0]["lives_trk"]=std::to_string(d_amounts);
	}
      else if (d_amounts < 0 || d_amounts == 0) {}
    }
  /*****************************************************************/
}

void updating_lasttwocols_fromdatabase(std::string addrs, MatrixTLS &M_file, int i, long int live_updated)
{
  if ( addrs == M_file[i][0] )
    M_file[i][8] = std::to_string(live_updated);
  else
    M_file[i][9] = std::to_string(live_updated);
}

void building_edge(std::map<std::string, std::string> &path_first, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, double entry_price, double exit_price, long int lives, int index_row, int path_number, long int amount_path, int ghost_edge)
{
  path_first["addrs_src"]   = addrs_src;
  path_first["addrs_trk"]   = addrs_trk;
  path_first["status_src"]  = status_src;
  path_first["status_trk"]  = status_trk;
  path_first["entry_price"] = std::to_string(entry_price);
  path_first["exit_price"]  = std::to_string(exit_price);
  path_first["lives_src"]   = std::to_string(lives);
  path_first["lives_trk"]   = std::to_string(lives);
  path_first["amount_trd"]  = std::to_string(amount_path);
  path_first["edge_row"]    = std::to_string(index_row);
  path_first["path_number"] = std::to_string(path_number);
  path_first["ghost_edge"]  = std::to_string(ghost_edge);
}

void building_lives_edges(std::map<std::string, std::string> &path_first, std::string addrs, std::string status, long int lives, double entry_price, struct status_amounts_edge *pt_status_byedge)
{
  path_first["addrs"] = addrs;
  path_first["status"] = status;
  path_first["lives"] =  std::to_string(lives);
  path_first["entry_price"] = std::to_string(entry_price);
  path_first["edge_row"] = std::to_string(pt_status_byedge->edge_row);
  path_first["path_number"] = std::to_string(pt_status_byedge->path_number);
}

void printing_edges(std::map<std::string, std::string> &path_first)
{
  PrintToLog("{ addrs_src : %s , status_src : %s, lives_src : %d, addrs_trk : %s , status_trk : %s, lives_trk : %d, entry_price : %d, exit_price : %d, amount_trd : %d, edge_row : %d, path_number : %d, ghost_edge : %d }\n", path_first["addrs_src"], path_first["status_src"], path_first["lives_src"], path_first["addrs_trk"], path_first["status_trk"], path_first["lives_trk"], path_first["entry_price"], path_first["exit_price"], path_first["amount_trd"], path_first["edge_row"], path_first["path_number"], path_first["ghost_edge"]);
}

void printing_edges_lives(std::map<std::string, std::string> &path_first)
{
  PrintToLog("{ addrs : %s , status : %s, lives : %d, entry_price : %d, edge_row : %d, path_number : %d }\n", path_first["addrs"], path_first["status"], path_first["lives"], path_first["entry_price"], path_first["edge_row"], path_first["path_number"]);
}

bool find_open_incr_anypos(std::string &s, VectorTLS *v)
{
  VectorTLS open_incr_anypos(4);
  VectorTLS &u = *v;
  for (int i = 0; i < length(open_incr_anypos); i++) open_incr_anypos[i] = u[i];
  return finding(s, open_incr_anypos);
}

bool find_netted_npartly_anypos(std::string &s, VectorTLS *v)
{
  VectorTLS netted_npartly_anypos(4);
  VectorTLS &u = *v;
  for (int i = 0; i < length(netted_npartly_anypos); i++) netted_npartly_anypos[i] = u[i];
  return finding(s, netted_npartly_anypos);
}

bool find_open_incr_long(std::string &s, VectorTLS *v)
{
  VectorTLS open_incr_long(2);
  VectorTLS &u = *v;
  for (int i = 0; i < length(open_incr_long); i++) open_incr_long[i] = u[i];
  return finding(s, open_incr_long);
}

void looking_netted_events(std::string &addrs_obj, std::vector<std::map<std::string, std::string>> &it_path_main, int q, long int amount_opened, int index_src_trk, std::string status_opening)
{
  long int sum_amount_trd = 0;
  int netted_counter = 0;
  unsigned index_netted = 0;
  
  for (unsigned i = q; i < it_path_main.size(); ++i)
    {
      struct status_amounts_edge *pt_status_byedge = get_status_byedge(it_path_main[i]);
      
      if ( sum_amount_trd > amount_opened ) break;
      
      if ( pt_status_byedge->addrs_trk == addrs_obj )
      	{
	  if ( finding_string("Long", status_opening) && finding_string("Long", pt_status_byedge->status_trk) )
	    {
	      index_netted = i;
	      netted_counter += 1;
	      sum_amount_trd += pt_status_byedge->amount_trd;
	    }
	  if ( finding_string("Short", status_opening) && finding_string("Short", pt_status_byedge->status_trk) )
	    {
	      index_netted = i;
	      netted_counter += 1;
	      sum_amount_trd += pt_status_byedge->amount_trd;
	    }
	}
    }
  
  bool check = sum_amount_trd <= amount_opened;
  if ( netted_counter != 0 )
    it_path_main[index_netted]["lives_trk"] = check ? std::to_string(amount_opened - sum_amount_trd) : std::to_string(0);
  else
    {
      //PrintToLog("\nindex q = %d\n", q);
      if ( index_src_trk == 0 )
	it_path_main[q-1]["lives_src"] = std::to_string(amount_opened);
      else if ( index_src_trk == 1 )
	it_path_main[q-1]["lives_trk"] = std::to_string(amount_opened);
    }
}

void printing_path_maini(std::vector<std::map<std::string, std::string>> &it_path_maini)
{
  for (std::vector<std::map<std::string, std::string>>::iterator it = it_path_maini.begin(); it != it_path_maini.end(); ++it)
    printing_edges(*it);
}

void checking_zeronetted_bypath(std::vector<std::map<std::string, std::string>> path_maini)
{
  long int contracts_closed = 0;
  //long int contracts_opened = 0;
  //extern VectorTLS *pt_open_incr_anypos; VectorTLS &open_incr_anypos = *pt_open_incr_anypos;
  
  // if (finding(path_maini[0]["status_src"], open_incr_anypos) && finding(path_maini[0]["status_trk"], open_incr_anypos))
  //   contracts_opened = 2*std::stol(path_maini[0]["amount_trd"]);
  // else
  //   contracts_opened = std::stol(path_maini[0]["amount_trd"]);
  
  //long int contracts_lives = std::stol(path_maini[0]["lives_src"])+std::stol(path_maini[0]["lives_trk"]);
  
  int idx_q = 0;
  /** Counting closed contracts. Netted eventes are followed by addrs_trk!! **/
  for (std::vector<std::map<std::string, std::string>>::iterator it = path_maini.begin()+1; it != path_maini.end(); ++it)
    {
      idx_q += 1;
      struct status_amounts_edge *pt_status_byedge = get_status_byedge(*it);
      if (path_maini[0]["addrs_trk"] == pt_status_byedge->addrs_trk)
	{
	  if (find_netted_npartly_anypos(pt_status_byedge->status_trk, pt_netted_npartly_anypos))
	    {
	      path_maini[idx_q]["addrs_src"]   = std::string();
	      path_maini[idx_q]["addrs_trk"]   = std::string();
	      path_maini[idx_q]["status_src"]  = std::string();
	      path_maini[idx_q]["status_trk"]  = std::string();
	      path_maini[idx_q]["entry_price"] = std::to_string(0);
	      path_maini[idx_q]["exit_price"]  = std::to_string(0);
	      path_maini[idx_q]["lives_src"]   = std::to_string(0);
	      path_maini[idx_q]["lives_trk"]   = std::to_string(0);
	      path_maini[idx_q]["amount_trd"]  = std::to_string(0);
	      path_maini[idx_q]["edge_row"]    = std::to_string(0);
	      path_maini[idx_q]["path_number"] = std::to_string(0);
	      path_maini[idx_q]["ghost_edge"]  = std::to_string(0);
	      contracts_closed += pt_status_byedge->amount_trd;
	    }
	}      
    }
  
  for (std::vector<std::map<std::string, std::string>>::iterator it = path_maini.begin()+1; it != path_maini.end(); ++it)
    {
      struct status_amounts_edge *pt_status_byedge = get_status_byedge(*it);
      if (path_maini[0]["addrs_src"] == pt_status_byedge->addrs_trk)
      	{
      	  if (find_netted_npartly_anypos(pt_status_byedge->status_trk, pt_netted_npartly_anypos))
	    contracts_closed += pt_status_byedge->amount_trd;
      	}
    }
  
  // PrintToLog("\ncontracts_opened = %d, contracts_closed = %d, contracts_lives = %d\n",
  // 	     contracts_opened, contracts_closed, contracts_lives);
  // if ( (contracts_opened - contracts_closed)-contracts_lives == 0 )
  //   PrintToLog("\nChecking Zero Netted by Path:\n(contracts_opened - contracts_closed)-contracts_lives = %d\n",
  // 	       (contracts_opened - contracts_closed)-contracts_lives);
  // else
  //   PrintToLog("¡¡Warning!! There is no zero netted event in the path");
}

void computing_livesvectors_forlongshort(std::vector<std::map<std::string, std::string>> &it_path_main, std::vector<std::map<std::string, std::string>> &lives_longs, std::vector<std::map<std::string, std::string>> &lives_shorts)
{
  std::map<std::string, std::string> path_ele;
  
  for (std::vector<std::map<std::string, std::string>>::iterator it = it_path_main.begin(); it != it_path_main.end(); ++it)
    {
      struct status_amounts_edge *pt_status_byedge = get_status_byedge(*it);
      if ( pt_status_byedge->lives_src != 0 )
	{
	  building_lives_edges(path_ele, pt_status_byedge->addrs_src, pt_status_byedge->status_src, pt_status_byedge->lives_src, pt_status_byedge->exit_price, pt_status_byedge);
	  
	  if ( finding_string("Long", pt_status_byedge->status_src) )
	    lives_longs.push_back(path_ele);
	  else if ( finding_string("Short", pt_status_byedge->status_src) )
	    lives_shorts.push_back(path_ele);
	}
      if ( pt_status_byedge->lives_trk != 0 )
	{
	  building_lives_edges(path_ele, pt_status_byedge->addrs_trk, pt_status_byedge->status_trk, pt_status_byedge->lives_trk, pt_status_byedge->entry_price, pt_status_byedge);
	  
	  if ( finding_string("Long", pt_status_byedge->status_trk) )
	    lives_longs.push_back(path_ele);
	  else if ( finding_string("Short", pt_status_byedge->status_trk) )
	    lives_shorts.push_back(path_ele);
	}
    }
  /** Be sure if M_file contain addrs in G_lives. Are those addrs still containing lives contracts from some past settlement? **/
}

void counting_lives_longshorts(std::vector<std::map<std::string, std::string>> &lives_longs, std::vector<std::map<std::string, std::string>> &lives_shorts)
{
  long int nlives_longs = 0;
  long int nlives_shorts = 0;
  
  //PrintToLog("\nList of Long Lives:\n");
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives_longs.begin(); it != lives_longs.end(); ++it)
    {
      //printing_edges_lives(*it);
      nlives_longs += stol((*it)["lives"]);
    }
  //PrintToLog("\nList of Short Lives:\n");
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives_shorts.begin(); it != lives_shorts.end(); ++it)
    {
      //printing_edges_lives(*it);
      nlives_shorts += stol((*it)["lives"]);
    }
  PrintToLog("\n|nlives_longs| : %d, \n|nlives_shorts| : %d\n", nlives_longs, nlives_shorts);
  if (nlives_longs != nlives_shorts) PrintToLog("\n\nWarning!! Lives Longs sould be equal to Lives Shorts\n\n");
}

void computing_livesvector_global(std::vector<std::map<std::string, std::string>> lives_longs, std::vector<std::map<std::string, std::string>> lives_shorts, std::vector<std::map<std::string, std::string>> &lives_longs_vg, std::vector<std::map<std::string, std::string>> &lives_shorts_vg)
{
  getting_globallives_long_short(lives_longs, lives_longs_vg);  
  getting_globallives_long_short(lives_shorts, lives_shorts_vg);
}

void printing_lives_vector(std::vector<std::map<std::string, std::string>> lives)
{
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives.begin(); it != lives.end(); ++it)
    {
      printing_edges_lives(*it);
    }
}

void getting_globallives_long_short(std::vector<std::map<std::string, std::string>> lives, std::vector<std::map<std::string, std::string>> &lives_vg)
{
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives.begin(); it != lives.end(); ++it)
    lives_vg.push_back(*it);
}

bool find_address_lives_vector(std::vector<std::map<std::string, std::string>> lives_v, std::string address)
{
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives_v.begin(); it != lives_v.end(); ++it)
    {
      struct status_lives_edge *pt_status_bylivesedge = get_status_bylivesedge(*it);
      if (pt_status_bylivesedge->addrs == address)
	{
	  return true;
	}
    }
  return false;
}

int find_posaddress_lives_vector(std::vector<std::map<std::string, std::string>> lives_v, std::string address)
{
  int idx_q = 0;
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives_v.begin(); it != lives_v.end(); ++it)
    {
      idx_q += 1;
      struct status_lives_edge *pt_status_bylivesedge = get_status_bylivesedge(*it);
      if (pt_status_bylivesedge->addrs == address)
	{
	  return idx_q-1;
	}
    }
  return idx_q-1;
}

void computing_settlement_exitprice(std::vector<std::map<std::string, std::string>> &it_path_main, long int &sum_oflives, double &PNL_total, double &gamma_p, double &gamma_q, int64_t interest, int64_t twap_price)
{
  long int sum_oflivesh = 0;
  std::unordered_set<std::string> addrs_set;
  std::vector<std::string> addrsv;
  
  for (std::vector<std::map<std::string, std::string>>::iterator it = it_path_main.begin(); it != it_path_main.end(); ++it)
    {
      std::map<std::string, std::string> &it_ele = *it;
      sum_oflivesh += stol(it_ele["lives_src"]) + stol(it_ele["lives_trk"]);
    }
  sum_oflives = sum_oflivesh;
  if ( sum_oflives == 0 )
    {
      //PrintToLog("\nThis path does not have lives contracts!!\n");
    }
  else
    {
      listof_addresses_bypath(it_path_main, addrsv);
      calculate_pnltrk_bypath(it_path_main, PNL_total, addrs_set, addrsv, interest, twap_price);
      getting_gammapq_bypath(it_path_main, PNL_total, gamma_p, gamma_q, addrs_set);
      addrs_set.clear();
    }
}

void calculate_pnltrk_bypath(std::vector<std::map<std::string, std::string>> &path_main, double &PNL_total, std::unordered_set<std::string> &addrs_set, std::vector<std::string> addrsv, int64_t interest, int64_t twap_price)
{
  std::vector<std::map<std::string, std::string>>::iterator it_path;
  std::string addrsit;
  double PNL_trk;
  double sumPNL_trk = 0;
  extern int n_cols;
  extern MatrixTLS *pt_ndatabase; MatrixTLS &ndatabase = *pt_ndatabase;
  VectorTLS jrow_database(n_cols);
  
  for (std::vector<std::string>::iterator it_addrs = addrsv.begin(); it_addrs != addrsv.end(); ++it_addrs)
    {
      addrsit = *it_addrs;
      for (it_path = path_main.begin()+1; it_path != path_main.end(); ++it_path)
	{
	  std::map<std::string, std::string> &edge_path = *it_path;
	  if ( addrsit == edge_path["addrs_trk"] )
	    {
	      sub_row(jrow_database, ndatabase, stol(edge_path["edge_row"]));
	      struct status_amounts *pt_jrow_database = get_status_amounts_byaddrs(jrow_database, addrsit);
	      addrs_set.insert(addrsit);
	      
	      PNL_trk = PNL_function(stod(edge_path["entry_price"]), stod(edge_path["exit_price"]), stol(edge_path["amount_trd"]),
				     pt_jrow_database);
	      sumPNL_trk += PNL_trk;
	      
	      std::string addrssr = edge_path["addrs_src"];
	      int64_t PNL_trkInt64 = mastercore::DoubleToInt64(PNL_trk);
	      
	      struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, "ALL F18");
	      uint32_t NotionalSize = pfuture->fco_notional_size;
	      
	      arith_uint256 volumeALL256_t = mastercore::ConvertTo256(NotionalSize)*mastercore::ConvertTo256(PNL_trkInt64)/COIN;  
	      int64_t volumeALL64_t = mastercore::ConvertTo64(volumeALL256_t);
	      
	      struct TokenDataByName *pfuture_ALL = getTokenDataByName("ALL");
	      uint32_t ALLId = pfuture_ALL->data_propertyId;
	      
	      // int64_t entry_priceh =  mastercore::StrToInt64(edge_path["entry_price"], true);
	      // PrintToLog("\nInterest Payment: Entry Price = %s, Twap Price = %s\n",
	      // 		 FormatDivisibleMP(entry_priceh), FormatDivisibleMP(twap_price));
	      
	      if (volumeALL64_t != 0 && volumeALL64_t < getMPbalance(addrssr, ALLId, BALANCE))
		{
		  PrintToLog("\nRebalancing Settlement\n");
		  assert(mastercore::update_tally_map(addrssr, ALLId, -volumeALL64_t, BALANCE)); /** Change in testnet **/
		  assert(mastercore::update_tally_map(addrsit, ALLId,  volumeALL64_t, BALANCE)); /** Change in testnet**/
	       	}
	    }
	}
    }  
  PNL_total = sumPNL_trk;
}

void calculate_pnltrk_bypath(std::vector<std::map<std::string, std::string>> path_main, double &PNL_total)
{
  std::vector<std::map<std::string, std::string>>::iterator it_path;
  double sumPNL_trk = 0;
  double PNL_trk;
  extern int n_cols;
  extern MatrixTLS *pt_ndatabase; MatrixTLS &ndatabase = *pt_ndatabase;
  VectorTLS jrow_database(n_cols);
  
  //PrintToLog("\nChecking PNL in this Path:\n");
  for (it_path = path_main.begin(); it_path != path_main.end(); ++it_path)
    {
      sub_row(jrow_database, ndatabase, stol((*it_path)["edge_row"]));
      struct status_amounts *pt_jrow_database = get_status_amounts_byaddrs(jrow_database, (*it_path)["addrs_trk"]);
      PNL_trk = PNL_function(stod((*it_path)["entry_price"]), stod((*it_path)["exit_price"]), stol((*it_path)["amount_trd"]), pt_jrow_database);
      //PrintToLog("\nPNL_trk = %f\n", PNL_trk);
      sumPNL_trk += PNL_trk;
    }
  PNL_total = sumPNL_trk;
  //PrintToLog("\nPNL_total_thispath = %f\n", PNL_total);
}

void listof_addresses_lives(std::vector<std::map<std::string, std::string>> lives, std::vector<std::string> &addrsv)
{
  std::vector<std::string> addrsvh;
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives.begin(); it != lives.end(); ++it)
    {
      std::map<std::string, std::string> &it_ele = *it;
      if ( !find_string_strv(it_ele["addrs"], addrsvh) )
	addrsvh.push_back(it_ele["addrs"]);
      else
	continue;
    }
  addrsv = addrsvh;
}

void listof_addresses_bypath(std::vector<std::map<std::string, std::string>> &it_path_main, std::vector<std::string> &addrsv)
{
  std::vector<std::string> addrsvh;
  for (std::vector<std::map<std::string, std::string>>::iterator it = it_path_main.begin(); it != it_path_main.end(); ++it)
    {
      std::map<std::string, std::string> &it_ele = *it;
      if ( !find_string_strv(it_ele["addrs_src"], addrsvh) )
	addrsvh.push_back(it_ele["addrs_src"]);
      if ( !find_string_strv(it_ele["addrs_trk"], addrsvh) )
	addrsvh.push_back(it_ele["addrs_trk"]);
    }
  addrsv = addrsvh;
}

double PNL_function(double entry_price, double exit_price, long int amount_trd, struct status_amounts *pt_jrow_database)
{
  double PNL = 0;
  
  if ( finding_string("Long", pt_jrow_database->status_trk) )
    PNL = (double)amount_trd*(1/entry_price-1/exit_price);
  else if ( finding_string("Short", pt_jrow_database->status_trk) )
    PNL = (double)amount_trd*(1/exit_price-1/entry_price);

  return PNL;
}

void getting_gammapq_bypath(std::vector<std::map<std::string, std::string>> &path_main, double PNL_total, double &gamma_p, double &gamma_q, std::unordered_set<std::string> addrs_set)
{
  extern int n_cols;
  extern MatrixTLS *pt_ndatabase; MatrixTLS &ndatabase = *pt_ndatabase;
  VectorTLS jrow_database(n_cols);
  long int sum_alpha_beta_i = 0;
  long int sum_alpha_i = 0;
  long int sum_alpha_beta_j = 0;
  long int sum_alpha_j = 0;

  std::vector<std::map<std::string, std::string>>::iterator it_path_main;
  for (it_path_main = path_main.begin(); it_path_main != path_main.end(); ++it_path_main)
    {
      std::map<std::string, std::string> &edge_ele = *it_path_main;
      if ( stol(edge_ele["lives_src"]) != 0 )
	{
	  sub_row(jrow_database, ndatabase, stol(edge_ele["edge_row"]));
	  struct status_amounts *pt_jrow_database = get_status_amounts_byaddrs(jrow_database, edge_ele["addrs_src"]);
	  if ( finding_string("Long", pt_jrow_database->status_trk) )
	    {
	      sum_alpha_beta_i += stol(edge_ele["lives_src"])*stod(edge_ele["exit_price"]);
	      sum_alpha_i += stol(edge_ele["lives_src"]);
	    }
	  else
	    {
	      sum_alpha_beta_j += stol(edge_ele["lives_src"])*stod(edge_ele["exit_price"]);
	      sum_alpha_j += stol(edge_ele["lives_src"]);
	    }
	}
      if ( stol(edge_ele["lives_trk"]) != 0 )
	{
	  sub_row(jrow_database, ndatabase, stol(edge_ele["edge_row"]));
	  struct status_amounts *pt_jrow_database = get_status_amounts_byaddrs(jrow_database, edge_ele["addrs_trk"]);
	  if ( finding_string("Long", pt_jrow_database->status_trk) )
	    {
	      sum_alpha_beta_i += stol(edge_ele["lives_trk"])*stod(edge_ele["entry_price"]);
	      sum_alpha_i += stol(edge_ele["lives_trk"]);
	    }
	  else
	    {
	      sum_alpha_beta_j += stol(edge_ele["lives_trk"])*stod(edge_ele["entry_price"]);
	      sum_alpha_j += stol(edge_ele["lives_trk"]);
	    }
	}
    }
  gamma_p = PNL_total-sum_alpha_beta_i+sum_alpha_beta_j;
  gamma_q = sum_alpha_j-sum_alpha_i;
}

void calculating_ghost_edges(std::vector<std::map<std::string, std::string>> lives_longs, std::vector<std::map<std::string, std::string>> lives_shorts, double exit_price_desired, std::vector<std::map<std::string, std::string>> &ghost_edges_array)
{
  long int amount_itlongs  = 0;
  long int amount_itshorts = 0;
  unsigned index_start = 0;

  std::vector<std::map<std::string, std::string>>::iterator it_longs;
  std::vector<std::map<std::string, std::string>>::iterator it_shorts;
  std::map<std::string, std::string> short_ele;
  std::map<std::string, std::string> long_ele;

  std::map<std::string, std::string> edge_ele;

  for (unsigned i = 0; i < lives_longs.size(); i++)
    {
      long_ele = lives_longs[i];
      amount_itlongs = stol(long_ele["lives"]);

      for (unsigned j = index_start; j < lives_shorts.size(); j++)
	{
	  short_ele = lives_shorts[j];
	  amount_itshorts = stol(short_ele["lives"]);

	  if ( amount_itlongs > amount_itshorts )
	    {
	      amount_itlongs = amount_itlongs - amount_itshorts;

	      building_edge(edge_ele, short_ele["addrs"], long_ele["addrs"], short_ele["status"], long_ele["status"], stod(long_ele["entry_price"]), exit_price_desired, 0, stol(long_ele["edge_row"]), stol(long_ele["path_number"]), amount_itshorts, 1);

	      ghost_edges_array.push_back(edge_ele);
	      building_edge(edge_ele, long_ele["addrs"], short_ele["addrs"], long_ele["status"], short_ele["status"], stod(short_ele["entry_price"]), exit_price_desired, 0, stol(short_ele["edge_row"]), stol(short_ele["path_number"]), amount_itshorts, 1);
	      ghost_edges_array.push_back(edge_ele);

	      continue;
	    }
	  if ( amount_itlongs < amount_itshorts )
	    {
	      index_start = j;
	      lives_shorts[j]["lives"] = std::to_string(amount_itshorts - amount_itlongs);

	      building_edge(edge_ele, short_ele["addrs"], long_ele["addrs"], short_ele["status"], long_ele["status"], stod(long_ele["entry_price"]), exit_price_desired, 0, stol(long_ele["edge_row"]), stol(long_ele["path_number"]), amount_itlongs, 1);
	      ghost_edges_array.push_back(edge_ele);

	      building_edge(edge_ele, long_ele["addrs"], short_ele["addrs"], long_ele["status"], short_ele["status"], stod(short_ele["entry_price"]), exit_price_desired, 0, stol(short_ele["edge_row"]), stol(short_ele["path_number"]), amount_itlongs, 1);
	      ghost_edges_array.push_back(edge_ele);

	      break;
	    }
	  if ( amount_itlongs == amount_itshorts )
	    {
	      index_start = j+1;

	      building_edge(edge_ele, short_ele["addrs"], long_ele["addrs"], short_ele["status"], long_ele["status"], stod(long_ele["entry_price"]), exit_price_desired, 0, stol(long_ele["edge_row"]), stol(long_ele["path_number"]), amount_itlongs, 1);
	      ghost_edges_array.push_back(edge_ele);

	      building_edge(edge_ele, long_ele["addrs"], short_ele["addrs"], long_ele["status"], short_ele["status"], stod(short_ele["entry_price"]), exit_price_desired, 0, stol(short_ele["edge_row"]), stol(short_ele["path_number"]), amount_itlongs, 1);
	      ghost_edges_array.push_back(edge_ele);

	      break;
	    }
	}
    }
}

void updating_lives_tozero(std::vector<std::vector<std::map<std::string, std::string>>> &path_main)
{
  std::vector<std::vector<std::map<std::string, std::string>>>::iterator it_path_main;
  std::vector<std::map<std::string, std::string>>::iterator it_path_maini;

  for (it_path_main = path_main.begin(); it_path_main != path_main.end(); ++it_path_main)
    {
      for (it_path_maini = (*it_path_main).begin(); it_path_maini != (*it_path_main).end(); ++it_path_maini)
	{
	  std::map<std::string, std::string> &edge_ele = *it_path_maini;
	  edge_ele["lives_src"] = std::to_string(0);
	  edge_ele["lives_trk"] = std::to_string(0);
	  printing_edges(*it_path_maini);
	}
    }
}

void joining_pathmain_ghostedges(std::vector<std::vector<std::map<std::string, std::string>>> &path_main, std::vector<std::map<std::string, std::string>> ghost_edges_array)
{
  std::vector<std::vector<std::map<std::string, std::string>>>::iterator it_path_main;
  std::vector<std::map<std::string, std::string>>::iterator it_ghosts;
  long int index_path = 0;

  for (it_path_main = path_main.begin(); it_path_main != path_main.end(); ++it_path_main)
    {
      index_path = stol((*it_path_main).front()["path_number"]);
      for (it_ghosts = ghost_edges_array.begin(); it_ghosts != ghost_edges_array.end(); ++it_ghosts)
	{
	  if ( stol((*it_ghosts)["path_number"]) == index_path )
	    {
	      (*it_path_main).push_back(*it_ghosts);
	    }
	}
    }
}

void checkzeronetted_bypath_ghostedges(std::vector<std::map<std::string, std::string>> path_maini, long int nonzero_lives)
{
  std::vector<std::map<std::string, std::string>>::iterator it_path_maini;
  long int lives_settled = 0;

  if ( nonzero_lives != 0 )
    {
      for (it_path_maini = path_maini.begin(); it_path_maini != path_maini.end(); ++it_path_maini)
	{
	  if ( stol((*it_path_maini)["ghost_edge"]) == 1 )
	    {
	      lives_settled += stol((*it_path_maini)["amount_trd"]);
	    }
	}
    }
  //PrintToLog("\nLives in the Path - Lives settled : (%ld - %ld) = %ld\n", nonzero_lives, lives_settled, nonzero_lives - lives_settled);
}

long int checkpath_livesnonzero(std::vector<std::map<std::string, std::string>> path_maini)
{
  std::vector<std::map<std::string, std::string>>::iterator it_path_maini;
  long int lives = 0;

  for (it_path_maini = path_maini.begin(); it_path_maini != path_maini.end(); ++it_path_maini)
    {
      lives += stol((*it_path_maini)["lives_src"])+stol((*it_path_maini)["lives_trk"]);
    }
  return lives;
}
