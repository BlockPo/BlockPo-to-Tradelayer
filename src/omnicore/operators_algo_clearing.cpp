#include "operators_algo_clearing.h"
#include <bits/stdc++.h>
#include "tradelayer_matrices.h"
#include "externfns.h"
#include <unordered_set>
#include <limits>
#include <iostream>

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
    } else
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

VectorTLS status_open_incr(VectorTLS &status_q, int q)
{
  extern VectorTLS *pt_open_incr_long; VectorTLS &open_incr_long = *pt_open_incr_long;
  extern VectorTLS *pt_open_incr_short; VectorTLS &open_incr_short = *pt_open_incr_short;

  if ( q == 0 )
    {
      status_q[0] = open_incr_long[0];
      status_q[1] = open_incr_long[1];
    } else
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

void settlement_algorithm_fifo(MatrixTLS &M_file)
{
  extern int n_cols;
  extern int n_rows;
  extern VectorTLS *pt_open_incr_long;  VectorTLS &open_incr_long  = *pt_open_incr_long;
  extern VectorTLS *pt_open_incr_short; VectorTLS &open_incr_short = *pt_open_incr_short;
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
      
      if ( finding(pt_vdata_long->status_trk, open_incr_long) && finding(pt_vdata_short->status_trk, open_incr_short) )
        {
	  path_number += 1;

	  printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	  printf("New Edge Source: Row #%d\n\n", i);
	  building_edge(edge_source, pt_vdata_long->addrs_src, pt_vdata_long->addrs_trk, pt_vdata_long->status_src, pt_vdata_long->status_trk, pt_vdata_long->matched_price, pt_vdata_long->matched_price, 0, i, path_number, pt_vdata_long->amount_trd, 0);
	  path_maini.push_back(edge_source);
	  printing_edges(edge_source);
	  
	  int counting_netted_long = 0;
	  long int amount_trd_sum_long = 0;
	  printf("\n*************************************************");
	  printf("\nTracking Long Position:");
	  clearing_operator_fifo(vdata, M_file, i, pt_vdata_long, 0, counting_netted_long, amount_trd_sum_long, path_maini, path_number, pt_vdata_long->nlives_trk);

	  int counting_netted_short = 0;
	  long int amount_trd_sum_short = 0;
	  printf("\n*************************************************");
	  printf("\nTracking Short Position:");
	  clearing_operator_fifo(vdata, M_file, i, pt_vdata_short, 1, counting_netted_short, amount_trd_sum_short, path_maini, path_number, pt_vdata_short->nlives_trk);

	  printf("\n\nPath #%d\n\n", path_number);
	  for (std::vector<std::map<std::string, std::string>>::iterator it = path_maini.begin(); it != path_maini.end(); ++it)
	    printing_edges(*it);
        }
      if ( path_maini.size() != 0 ) path_main.push_back(path_maini);
    }
  printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
  printf("\nChecking Lives by Path:\n\n");
  int counting_paths = 0;

  std::vector<std::map<std::string, std::string>> lives_longs;
  std::vector<std::map<std::string, std::string>> lives_shorts;
  std::vector<std::map<std::string, std::string>> ghost_edges_array;

  long int sum_oflives = 0;
  double exit_price_desired = 0;
  double PNL_total = 0;
  double gamma_p = 0;
  double gamma_q = 0;
  double sum_gamma_p = 0;
  double sum_gamma_q = 0;

  for (it_path_main = path_main.begin(); it_path_main != path_main.end(); ++it_path_main)
    {
      printf("*******************************************");
      counting_paths += 1;
      computing_lives_bypath(*it_path_main);
      printf("\n\nPath #%d:\n\n", counting_paths);
      printing_path_maini(*it_path_main);
      checking_zeronetted_bypath(*it_path_main);
      computing_livesvectors_forlongshort(*it_path_main, lives_longs, lives_shorts);
      computing_settlement_exitprice(*it_path_main, sum_oflives, PNL_total, gamma_p, gamma_q);
      cout << "\ngamma_p : " << gamma_p <<  ", gamma_q : " << gamma_q << ", PNL_total : " << PNL_total <<"\n";
      sum_gamma_p += gamma_p;
      sum_gamma_q += gamma_q;
    }
  
  exit_price_desired = sum_gamma_p/sum_gamma_q;
  cout <<"\nexit_price_desired: " << exit_price_desired << "\n";
  counting_lives_longshorts(lives_longs, lives_shorts);
  
  printf("____________________________________________________");
  printf("\nGhost Edges Vector:\n");
  calculating_ghost_edges(lives_longs, lives_shorts, exit_price_desired, ghost_edges_array);
  std::vector<std::map<std::string, std::string>>::iterator it_ghost;
  for (it_ghost = ghost_edges_array.begin(); it_ghost != ghost_edges_array.end(); ++it_ghost)
    {
      printing_edges(*it_ghost);
    }

  printf("____________________________________________________");
  joining_pathmain_ghostedges(path_main, ghost_edges_array);
  int k = 0;
  long int nonzero_lives;
  double PNL_totalit;
  
  for (it_path_main = path_main.begin(); it_path_main != path_main.end(); ++it_path_main)
    {
      k += 1;
      printf("\nPath #%d with Ghost Nodes\n", k);
      printing_path_maini(*it_path_main);
      nonzero_lives = checkpath_livesnonzero(*it_path_main);
      checkzeronetted_bypath_ghostedges(*it_path_main, nonzero_lives);
      calculate_pnltrk_bypath(*it_path_main, PNL_totalit);
      PNL_total += PNL_totalit;
      printf("\nPNL_total_main sum: %f\n", PNL_total);
    }
  printf("\n____________________________________________________\n");
  printf("\nChecking PNL global (Total Sum PNL by Path) using exit price by Equation found:\n");
  printf("\nPNL_total_main = %f", PNL_total);
  printf("\n____________________________________________________\n");
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
	      printf("\n\nNetted Event in the Row #%d\t Address Tracked: %s\n", i, addrs_opening.c_str());
	      printing_vector(jrow_database);
	      d_amounts = opened_contracts - amount_trd_sum;
	      printf("\n\nReview of d_amounts before cases:\t%ld", d_amounts);
	     
	      if ( d_amounts > 0 )
		{
		  printf("\n\nd_amounts = %ld > 0", d_amounts);
		  updating_lasttwocols_fromdatabase(addrs_opening, M_file, i, 0);
		  cout<<"\n\nCheck last two columns Updated: Row #"<<i<<"\n"<<M_file[i][8]<<"\t"<<M_file[i][9]<<"\n";
		  printf("\nOpened contrats: %ld > Sum amounts traded: %ld\n", opened_contracts, amount_trd_sum);

		  building_edge(path_first, pt_status_addrs_trk->addrs_src, pt_status_addrs_trk->addrs_trk, pt_status_addrs_trk->status_src, pt_status_addrs_trk->status_trk, pt_pos->matched_price, pt_status_addrs_trk->matched_price, 0, i, path_number, pt_status_addrs_trk->nlives_trk, 0);
		  path_main.push_back(path_first);
		  printf("\nEdge:\n");
		  printing_edges(path_first);

		  if ( find_open_incr_anypos(pt_status_addrs_trk->status_src, pt_open_incr_anypos) )
		    {
		      printf("\n_________________________________________\n");
		      int idx_long_shorti = find_open_incr_long(pt_status_addrs_trk->status_src, pt_open_incr_long) ? 0 : 1;
		      printf("\nLooking New Path for: %s", pt_status_addrs_trk->addrs_src.c_str());
		      struct status_amounts *pt_status_addrs_src = get_status_amounts_byaddrs(jrow_database, pt_status_addrs_trk->addrs_src);
		      int counting_nettedi = 0;
		      long int amount_trd_sumi = 0;
		      clearing_operator_fifo(jrow_database, M_file, i, pt_status_addrs_src, idx_long_shorti, counting_nettedi, amount_trd_sumi, path_main, path_number, pt_status_addrs_src->nlives_trk);
		      printf("\n_________________________________________\n");
		    }
		}
	      if ( d_amounts < 0)
		{
		  printf("\n\nd_amounts = %ld < 0", d_amounts);
		  updating_lasttwocols_fromdatabase(addrs_opening, M_file, i, labs(d_amounts));
		  cout<<"\n\nCheck last two columns Updated: Row #"<<i<<"\n"<<M_file[i][8]<<"\t"<<M_file[i][9]<<"\n";
		  printf("\nOpened contrats: %ld < Sum amounts traded: %ld\n", opened_contracts, amount_trd_sum);

		  building_edge(path_first, pt_status_addrs_trk->addrs_src, pt_status_addrs_trk->addrs_trk, pt_status_addrs_trk->status_src, pt_status_addrs_trk->status_trk, pt_pos->matched_price, pt_status_addrs_trk->matched_price, 0, i, path_number, pt_status_addrs_trk->nlives_trk-labs(d_amounts), 0);
		  path_main.push_back(path_first);
		  printf("\nEdge:\n");
		  printing_edges(path_first);

		  if ( find_open_incr_anypos(pt_status_addrs_trk->status_src, pt_open_incr_anypos) )
		    {
		      printf("\n_________________________________________\n");
		      int idx_long_shorti = find_open_incr_long(pt_status_addrs_trk->status_src, pt_open_incr_long) ? 0 : 1;
		      printf("\nLooking New Path for: %s", pt_status_addrs_trk->addrs_src.c_str());
		      struct status_amounts *pt_status_addrs_src = get_status_amounts_byaddrs(jrow_database, pt_status_addrs_trk->addrs_src);
		      int counting_nettedi = 0;
		      long int amount_trd_sumi = 0;
		      clearing_operator_fifo(jrow_database, M_file, i, pt_status_addrs_src, idx_long_shorti, counting_nettedi, amount_trd_sumi, path_main, path_number, pt_status_addrs_src->nlives_trk-labs(d_amounts)); 
		      printf("\n_________________________________________\n");
		    }
		  break;
		}
	      if ( d_amounts == 0)
		{
		  printf("\n\nd_amounts = %ld = 0", d_amounts);
		  updating_lasttwocols_fromdatabase(addrs_opening, M_file, i, 0);
		  cout<<"\n\nCheck last two columns Updated: Row #"<<i<<"\n"<<M_file[i][8]<<"\t"<<M_file[i][9]<<"\n";
		  printf("\nOpened contrats: %ld < Sum amounts traded: %ld\n", opened_contracts, amount_trd_sum);
		  
		  building_edge(path_first, pt_status_addrs_trk->addrs_src, pt_status_addrs_trk->addrs_trk, pt_status_addrs_trk->status_src, pt_status_addrs_trk->status_trk, pt_pos->matched_price, pt_status_addrs_trk->matched_price, 0, i, path_number, pt_status_addrs_trk->nlives_trk, 0);
		  path_main.push_back(path_first);
		  printf("\nEdge:\n");
		  printing_edges(path_first);

		  if ( find_open_incr_anypos(pt_status_addrs_trk->status_src, pt_open_incr_anypos) )
		    {
		      printf("\n_________________________________________\n");
		      int idx_long_shorti = find_open_incr_long(pt_status_addrs_trk->status_src, pt_open_incr_long) ? 0 : 1;
		      printf("\nLooking New Path for: %s", pt_status_addrs_trk->addrs_src.c_str());
		      struct status_amounts *pt_status_addrs_src = get_status_amounts_byaddrs(jrow_database, pt_status_addrs_trk->addrs_src);
		      int counting_nettedi = 0;
		      long int amount_trd_sumi = 0;
		      clearing_operator_fifo(jrow_database, M_file, i, pt_status_addrs_src, idx_long_shorti, counting_nettedi, amount_trd_sumi, path_main, path_number, pt_status_addrs_src->nlives_trk); 
		      printf("\n_________________________________________\n");
		    }
		  break;
		}	    
	    }
        }
    }
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
  cout <<"{ "<<"addrs_src : "<<path_first["addrs_src"]<<", status_src : "<<path_first["status_src"]<<", lives_src : "<<path_first["lives_src"]<<", addrs_trk : "<<path_first["addrs_trk"]<<", status_trk : "<<path_first["status_trk"]<<", lives_trk : "<<path_first["lives_trk"]<<", entry_price : "<<path_first["entry_price"]<<", exit_price: "<<path_first["exit_price"]<<", amount_trd : "<<path_first["amount_trd"]<<", edge_row : "<<path_first["edge_row"]<<", path_number : "<<path_first["path_number"]<<", ghost_edge : "<<path_first["ghost_edge"]<<" }\n";
}

void printing_edges_lives(std::map<std::string, std::string> &path_first)
{
  cout <<"{ "<<"addrs : "<<path_first["addrs"]<<", status : "<<path_first["status"]<<", lives : "<<path_first["lives"]<<", entry_price : "<<path_first["entry_price"]<<", edge_row : "<<path_first["edge_row"]<<", path_number : "<<path_first["path_number"]<<" }\n";
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

void computing_lives_bypath(std::vector<std::map<std::string, std::string>> &it_path_main)
{
  int q = 0;
  for (std::vector<std::map<std::string, std::string>>::iterator it = it_path_main.begin(); it != it_path_main.end(); ++it)
    {
      q += 1;
      struct status_amounts_edge *pt_status_byedge = get_status_byedge(*it);
      if ( find_open_incr_anypos(pt_status_byedge->status_src, pt_open_incr_anypos) )
      	{
	  looking_netted_events(pt_status_byedge->addrs_src, it_path_main, q, pt_status_byedge->amount_trd, 0);
      	}
      if ( find_open_incr_anypos(pt_status_byedge->status_trk, pt_open_incr_anypos) )
      	{
      	  looking_netted_events(pt_status_byedge->addrs_trk, it_path_main, q, pt_status_byedge->amount_trd, 1);
	}
    }}


void looking_netted_events(std::string &addrs_obj, std::vector<std::map<std::string, std::string>> &it_path_main, int q, long int amount_opened, int index_src_trk)
{
  long int sum_amount_trd = 0;
  int netted_counter = 0;
  unsigned index_netted = 0;

  for (unsigned i = q; i < it_path_main.size(); ++i)
    {
      struct status_amounts_edge *pt_status_byedge = get_status_byedge(it_path_main[i]);
      if ( pt_status_byedge->addrs_trk == addrs_obj )
      	{
	  index_netted = i;
	  netted_counter += 1;
	  sum_amount_trd += pt_status_byedge->amount_trd;
      	}
    }
  
  if ( netted_counter != 0 )
    it_path_main[index_netted]["lives_trk"] = std::to_string(amount_opened - sum_amount_trd);
  else
    {
      if ( index_src_trk == 0 )
	it_path_main[q-1]["lives_src"] = std::to_string(amount_opened);
      else
	it_path_main[q-1]["lives_trk"] = std::to_string(amount_opened);
    }
}

void printing_path_maini(std::vector<std::map<std::string, std::string>> &it_path_maini)
{
  for (std::vector<std::map<std::string, std::string>>::iterator it = it_path_maini.begin(); it != it_path_maini.end(); ++it)
    printing_edges(*it);
}

void checking_zeronetted_bypath(std::vector<std::map<std::string, std::string>> &path_maini)
{
  int contracts_opened = 0;
  int contracts_closed = 0;
  int contracts_lives  = 0;

  for (std::vector<std::map<std::string, std::string>>::iterator it = path_maini.begin(); it != path_maini.end(); ++it)
    {
      struct status_amounts_edge *pt_status_byedge = get_status_byedge(*it);
      if ( find_open_incr_anypos(pt_status_byedge->status_src, pt_open_incr_anypos) && find_open_incr_anypos(pt_status_byedge->status_trk, pt_open_incr_anypos) )
      	{
	  contracts_opened += pt_status_byedge->amount_trd;
      	}
      if ( find_netted_npartly_anypos(pt_status_byedge->status_src, pt_netted_npartly_anypos) && find_open_incr_anypos(pt_status_byedge->status_trk, pt_netted_npartly_anypos) )
      	{
      	  contracts_closed += pt_status_byedge->amount_trd;
      	}
      contracts_lives += pt_status_byedge->lives_src+pt_status_byedge->lives_trk;
    }
  if ( (2*contracts_opened - contracts_closed)-contracts_lives == 0 )
    cout << "\nChecking Zero Netted by Path:\n(contracts_opened - contracts_closed)-contracts_lives = (" << 2*contracts_opened << " - " << contracts_closed << ") - " << contracts_lives << " = " << 2*contracts_opened - contracts_closed << " - " << contracts_lives << " = " << (2*contracts_opened - contracts_closed)-contracts_lives <<"\n";
  else
    printf("Warning!! There is no zero netted event in the path");
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
	  else
	    lives_shorts.push_back(path_ele);
	}
      if ( pt_status_byedge->lives_trk != 0 )
	{
	  building_lives_edges(path_ele, pt_status_byedge->addrs_trk, pt_status_byedge->status_trk, pt_status_byedge->lives_trk, pt_status_byedge->entry_price, pt_status_byedge);
	  
	  if ( finding_string("Long", pt_status_byedge->status_trk) )
	    lives_longs.push_back(path_ele);
	  else
	    lives_shorts.push_back(path_ele);
	}
    }
}

void counting_lives_longshorts(std::vector<std::map<std::string, std::string>> &lives_longs, std::vector<std::map<std::string, std::string>> &lives_shorts)
{
  long int nlives_longs = 0;
  long int nlives_shorts = 0;

  printf("\nList of Long Lives:\n");
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives_longs.begin(); it != lives_longs.end(); ++it)
    {
      printing_edges_lives(*it);
      std::map<std::string, std::string> &ele_long = *it;
      nlives_longs += stol(ele_long["lives"]);
    }
  printf("\nList of Short Lives:\n");
  for (std::vector<std::map<std::string, std::string>>::iterator it = lives_shorts.begin(); it != lives_shorts.end(); ++it)
    {
      printing_edges_lives(*it);
      std::map<std::string, std::string> &ele_short = *it;
      nlives_shorts += stol(ele_short["lives"]);
    }
  cout << "\n|nlives_longs| : " << nlives_longs << ", |nlives_longs| : " << nlives_shorts << "\n";
}

void computing_settlement_exitprice(std::vector<std::map<std::string, std::string>> &it_path_main, long int &sum_oflives, double &PNL_total, double &gamma_p, double &gamma_q)
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
    printf("\nThis path does not have lives contracts!!\n");
  else
    {
      listof_addresses_bypath(it_path_main, addrsv);
      calculate_pnltrk_bypath(it_path_main, PNL_total, addrs_set, addrsv);
      getting_gammapq_bypath(it_path_main, PNL_total, gamma_p, gamma_q, addrs_set);
    }
}

void calculate_pnltrk_bypath(std::vector<std::map<std::string, std::string>> &path_main, double &PNL_total, std::unordered_set<std::string> &addrs_set, std::vector<std::string> addrsv)
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
	      PNL_trk = PNL_function(stod(edge_path["entry_price"]), stod(edge_path["exit_price"]), stol(edge_path["amount_trd"]), pt_jrow_database);
	      sumPNL_trk += PNL_trk;
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

  printf("\nChecking PNL in this Path:\n");
  for (it_path = path_main.begin(); it_path != path_main.end(); ++it_path)
    {
      sub_row(jrow_database, ndatabase, stol((*it_path)["edge_row"]));
      struct status_amounts *pt_jrow_database = get_status_amounts_byaddrs(jrow_database, (*it_path)["addrs_trk"]);
      PNL_trk = PNL_function(stod((*it_path)["entry_price"]), stod((*it_path)["exit_price"]), stol((*it_path)["amount_trd"]), pt_jrow_database);
      printf("\nPNL_trk = %f\n", PNL_trk);
      sumPNL_trk += PNL_trk;
    }
  PNL_total = sumPNL_trk;
  printf("\nPNL_total_thispath = %f\n", PNL_total);
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
  printf("\nLives in the Path - Lives settled : (%ld - %ld) = %ld\n", nonzero_lives, lives_settled, nonzero_lives - lives_settled);
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
