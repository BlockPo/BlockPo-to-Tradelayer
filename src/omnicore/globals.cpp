#include <stdio.h>
#include <string>

using namespace std;

volatile uint64_t marketPrice; // market price in USDs
uint32_t blocksUntilExpiration;
uint32_t notionalSize;
uint32_t collateralCurrency;
uint32_t marginRequirementContract;
int64_t priceIndex = 110; // an index price (USDs) to calculate the interest in pegg currencies (10% more)
int64_t allPrice = 10;  // the ALLs is gonna cost $10 USDs
uint64_t ask [10]; // minimum price of sellers of each contract
uint64_t bid [10]; // maximum price of buyers of each contract
double percentLiqPrice;
