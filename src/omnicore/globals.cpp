#include <stdio.h>
#include <string>
#include <vector>
#include <cstdint>
#include <stdint.h>
#include "tradelayer_matrices.h"
#include "mdex.h"

typedef boost::multiprecision::uint128_t ui128;
using namespace std;

uint64_t marketPrice;
int64_t globalNumPrice;
int64_t globalDenPrice;
int64_t priceIndex;
int64_t allPrice;
double percentLiqPrice;
int64_t factorE;
double denMargin;
uint64_t marketP[NPTYPES];
volatile int id_contract;
volatile int idx_q;
volatile unsigned int path_length;
std::vector<std::map<std::string, std::string>> path_ele;
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
VectorTLS *pt_expiration_dates;
VectorTLS *pt_vestingAddresses;
int nVestingAddrs;
int64_t amountVesting;
int lastBlockg;
int vestingActivationBlock;
volatile int64_t LTCPriceOffer;
volatile int64_t factorALLtoLTC;
volatile int64_t globalVolumeALL_LTC;
volatile int64_t Lastx_Axis;
volatile int64_t LastLinear;
volatile int64_t LastQuad;
