#include "er-coap-transactions.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

void calculateRTO(coap_rtt_estimations_t *t)
{
	clock_time_t delta;
	clock_time_t rto;
	int deltaCoef,gainbar,gain=3;

	//Calculate delta
	if(t->srttCoef == 1)
	{
		if(t->rtt >= t->srtt)
		{
			delta = t->rtt - t->srtt;
			deltaCoef = 1;
		}
		else
		{
			delta = t->srtt - t->rtt;
			deltaCoef = 0;
		}
	}
	else
	{
		delta = t->rtt + t->srtt;
		deltaCoef = 1;
	}

	//printf("DELTA COEF:%d\n",deltaCoef);
	//printf("DELTA:%ld\n",delta);
	//calculate inverse of gain
	if(deltaCoef == 1)
	{
		if(t->rttvarCoef == 0)
		{
			gainbar = 3;
		}
		else
		{
			if(delta >= t->rttvar)
				gainbar = 3;
			else
				gainbar = 9;
		}
	}
	else
	{
		if(t->rttvarCoef == 1)
			gainbar = 9;
		else
		{
			if(t->rttvar >= delta)
				gainbar = 3;
			else
				gainbar = 9;
		}
	}

	//Calculate srtt
	if(t->srttCoef == 1)
	{
		if(deltaCoef == 1)
		{
			t->srtt = t->srtt + delta/3;
			t->srttCoef = 1;
		}
		else
		{
			if(t->srtt >= delta/3)
			{
				t->srtt = t->srtt - delta/3;
				t->srttCoef = 1;
			}
			else
			{
				t->srtt = delta/3 - t->srtt;
				t->srttCoef = 0;
			}
		}
	}
	else
	{
		if(deltaCoef == 1)
		{
			if(delta/3 >= t->srtt)
			{
				t->srtt = delta/3 - t->srtt;
				t->srttCoef = 1;
			}
			else
			{
				t->srtt = t->srtt - delta/3;
				t->srttCoef = 0;
			}
		}
		else
		{
			t->srtt = t->srtt + delta/3;
			t->srttCoef = 0;
		}
	}
	//printf("SRTT COEF:%d\n",t->srttCoef );
	//printf("SRTT:%ld\n",t->srtt);
	//Calculate rttvar
	if(deltaCoef == 1)
	{
		if(t->rttvarCoef == 1)
		{
			if(delta >= t->rttvar)
			{
				//printf("case 1\n");
				t->rttvar = t->rttvar + (delta - t->rttvar)/gainbar ;
				t->rttvarCoef = 1;
			}
			else
			{
				if(t->rttvar >= (t->rttvar - delta)/gainbar)
				{
					//printf("case 2\n");
					t->rttvar = t->rttvar - (t->rttvar - delta)/gainbar ;
					t->rttvarCoef = 1;
				}
				else
				{
					//printf("case 3\n");
					t->rttvar = (t->rttvar - delta)/gainbar - t->rttvar ;
					t->rttvarCoef = 0;
				}
			}
		}
		else
		{
			if(t->rttvar <= (delta + t->rttvar)/gainbar)
			{
				//printf("case 4\n");
				t->rttvar = (delta + t->rttvar)/gainbar - t->rttvar;
				t->rttvarCoef = 1;
			}
			else
			{
				//printf("case 5\n");
				t->rttvar = t->rttvar - (delta + t->rttvar)/gainbar;
				t->rttvarCoef = 0;
			}
		}
	}

	//printf("RTTVAR COEF:%d\n",t->rttvarCoef );
	//printf("RTTVAR:%ld\n",t->rttvar);
    clock_time_t constant = 2;
	if(t->srttCoef == 1)
	{
		if(t->rttvarCoef == 1)
		{
			if((t->rtt + constant) > t->srtt + t->rttvar*gain)
				rto = t->rtt + constant;
			else
			{
				rto = t->srtt + t->rttvar*gain;
			}
		}
		else
		{
			if(t->srtt >= t->rttvar*gain)
			{
				if((t->rtt + constant) > t->srtt - t->rttvar*gain)
					rto = t->rtt + constant;
				else
				{
					rto = t->srtt - t->rttvar*gain;
				}
			}
			else
			{
				rto = t->rtt + constant;
			}
		}
	}
	else
	{
		if(t->rttvarCoef == 1)
		{
			if(t->rttvar*gain >= t->srtt)
			{
				if((t->rttvar*gain - t->srtt) >= t->rtt + constant)
				{
					rto = t->rttvar*gain - t->srtt;
				}
				else
					rto = t->rtt + constant;
					
			}
		}
		else
			rto = t->rtt + constant;
	}

	printf("RTO : %f,RTT: %f\n",(float)rto/CLOCK_SECOND,(float)t->rtt/CLOCK_SECOND);
	t->rto = rto;
}

