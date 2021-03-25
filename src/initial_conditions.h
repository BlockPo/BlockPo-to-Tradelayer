int N = 2;
int M = 4;
int NYears = 10;
int initYear = 19;

priceIndex = 110; // an index price (USDs) to calculate the interest in pegg currencies (10% more)
denMargin = 100;
n_cols = 10;

pt_open_incr_long  = new VectorTLS(N); VectorTLS &open_incr_longv = *pt_open_incr_long;
open_incr_longv[0] = "OpenLongPosition";
open_incr_longv[1] = "LongPosIncreased";

pt_open_incr_short  = new VectorTLS(N); VectorTLS &open_incr_shortv = *pt_open_incr_short;
open_incr_shortv[0] = "OpenShortPosition";
open_incr_shortv[1] = "ShortPosIncreased";

pt_netted_npartly_long  = new VectorTLS(N); VectorTLS &netted_npartly_long = *pt_netted_npartly_long;
netted_npartly_long[0] = "LongPosNetted";
netted_npartly_long[1] = "LongPosNettedPartly";

pt_netted_npartly_short  = new VectorTLS(N); VectorTLS &netted_npartly_short = *pt_netted_npartly_short;
netted_npartly_short[0] = "ShortPosNetted";
netted_npartly_short[1] = "ShortPosNettedPartly";

pt_open_incr_anypos = new VectorTLS(M); VectorTLS &open_incr_anypos = *pt_open_incr_anypos;
open_incr_anypos[0] = "OpenLongPosition";
open_incr_anypos[1] = "LongPosIncreased";
open_incr_anypos[2] = "OpenShortPosition";
open_incr_anypos[3] = "ShortPosIncreased";

pt_netted_npartly_anypos = new VectorTLS(M); VectorTLS &netted_npartly_anypos = *pt_netted_npartly_anypos;
netted_npartly_anypos[0] = "LongPosNetted";
netted_npartly_anypos[1] = "LongPosNettedPartly";
netted_npartly_anypos[2] = "ShortPosNetted";
netted_npartly_anypos[3] = "ShortPosNettedPartly";

pt_changepos_status = new VectorTLS(2); VectorTLS &changepos_status = *pt_changepos_status;
changepos_status[0] = "OpenLongPosByShortPosNetted";
changepos_status[1] = "OpenShortPosByLongPosNetted";

pt_expiration_dates = new VectorTLS(12*NYears); VectorTLS &expiration_dates = *pt_expiration_dates;
for ( int i = 0; i < NYears; i++ )
  {
    expiration_dates[i+11*i]    = "ALL F" + std::to_string(i+initYear);
    expiration_dates[i+1+11*i]  = "ALL G" + std::to_string(i+initYear);
    expiration_dates[i+2+11*i]  = "ALL H" + std::to_string(i+initYear);
    expiration_dates[i+3+11*i]  = "ALL J" + std::to_string(i+initYear);
    expiration_dates[i+4+11*i]  = "ALL K" + std::to_string(i+initYear);
    expiration_dates[i+5+11*i]  = "ALL M" + std::to_string(i+initYear);
    expiration_dates[i+6+11*i]  = "ALL N" + std::to_string(i+initYear);
    expiration_dates[i+7+11*i]  = "ALL Q" + std::to_string(i+initYear);
    expiration_dates[i+8+11*i]  = "ALL U" + std::to_string(i+initYear);
    expiration_dates[i+9+11*i]  = "ALL V" + std::to_string(i+initYear);
    expiration_dates[i+10+11*i] = "ALL X" + std::to_string(i+initYear);
    expiration_dates[i+11+11*i] = "ALL Z" + std::to_string(i+initYear);
  }

/** The final addrs amount will be chosen at mainnet launch time **/
nVestingAddrs = 5;
totalVesting = 1500000*COIN;
amountVesting = (1500000/nVestingAddrs)*COIN;
//volumeToVWAP = 200;
volumeToVWAP = 10;
//BlockS = 720; /** testnet **/
BlockS = 300; /** regtest **/
CompoundRate = 1.00002303;
DecayRate = 0.99998;
LongTailDecay = 0.99999992;
SatoshiH = 0.00000001;
