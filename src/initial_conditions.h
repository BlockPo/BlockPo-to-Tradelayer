int N = 2;
int M = 4;

factorE = 100000000;
priceIndex = 110; // an index price (USDs) to calculate the interest in pegg currencies (10% more)
allPrice = 10; // the ALLs is gonna cost $10 USDs
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

