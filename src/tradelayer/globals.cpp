#include <tradelayer/mdex.h>
#include <tradelayer/tradelayer_matrices.h>

#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

typedef boost::multiprecision::uint128_t ui128;
using namespace std;

uint64_t marketPrice;
int64_t globalNumPrice;
int64_t globalDenPrice;
int64_t priceIndex;
double percentLiqPrice;
double denMargin;
volatile int id_contract;
volatile int idx_q;
volatile unsigned int path_length;
//volatile std::vector<std::map<std::string, std::string>> path_eleg;
std::vector<std::map<std::string, std::string>> path_ele;
std::vector<std::map<std::string, std::string>> path_elef;
std::vector<std::map<std::string, std::string>> lives_longs_vg;
std::vector<std::map<std::string, std::string>> lives_shorts_vg;
int n_cols;
int n_rows;
int idx_expiration;
int expirationAchieve;
VectorTLS *pt_open_incr_long;
VectorTLS *pt_open_incr_short;
VectorTLS *pt_netted_npartly_long;
VectorTLS *pt_netted_npartly_short;
VectorTLS *pt_open_incr_anypos;
VectorTLS *pt_netted_npartly_anypos;
MatrixTLS *pt_ndatabase;
VectorTLS *pt_changepos_status;
std::map<std::string,uint32_t> peggedIssuers;

double globalPNLALL_DUSD;
int64_t globalVolumeALL_DUSD;
std::map<uint32_t, std::map<std::string, double>> addrs_upnlc;

// using for margin dynamics
std::map<std::string, int64_t> sum_upnls;

std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;
std::map<uint32_t, std::map<uint32_t, int64_t>> numVWAPMap;
std::map<uint32_t, std::map<uint32_t, int64_t>> denVWAPMap;
std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMap;
std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMapSubVector;
std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> numVWAPVector;
std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> denVWAPVector;

mutex map_vector_mtx;
std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;
std::map<uint32_t, std::vector<int64_t>> mapContractVolume;
std::map<uint32_t, int64_t> VWAPMapContracts;
VectorTLS *pt_expiration_dates;
int nVestingAddrs;
int64_t amountVesting;
int64_t totalVesting;
int lastBlockg;
int twapBlockg;

// for liquidation prices
int newTwapBlock;

int vestingActivationBlock;
volatile int64_t LTCPriceOffer;
volatile int64_t factorALLtoLTC;
volatile int64_t globalVolumeALL_LTC;
volatile int64_t Lastx_Axis;
volatile int64_t LastLinear;
volatile int64_t LastQuad;
volatile int64_t LastLog;
int volumeToVWAP;
int BlockS;
std::string setExoduss;

/*****************************************/
/** TWAP containers **/

// TWAP prices for n blocks used for liquidation prices
std::map<uint32_t, int64_t> cdex_twap_liq;

std::map<uint32_t, std::vector<uint64_t>> cdextwap_ele;
std::map<uint32_t, std::vector<uint64_t>> cdextwap_vec;
std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_ele;
std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_vec;

/*****************************************/
/**Node Reward**/

double CompoundRate;
double DecayRate;
double LongTailDecay;
mutex mReward;
double RewardSecndI;
double RewardFirstI;
int64_t SatoshiH;
