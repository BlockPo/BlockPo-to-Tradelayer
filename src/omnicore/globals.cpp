#include <stdio.h>
#include <string>
#include <vector>
#include <cstdint>
#include <stdint.h>
#include "tradelayer_matrices.h"
#include "mdex.h"

using namespace std;

volatile uint64_t marketPrice;
int64_t priceIndex;
int64_t allPrice;
double percentLiqPrice;
int64_t factorE;
double denMargin;
uint64_t marketP[NPTYPES];
volatile int id_contract;
volatile int idx_q;
volatile int path_length;
volatile int expirationAchieve;
std::vector<std::map<std::string, std::string>> path_ele;
//Map of addresses with pegged Currency (address, propertyId)
std::map<std::string,uint32_t> peggedIssuers;
