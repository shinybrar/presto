/*

Sample code:

 if(!getpoly(mjd, &dm, fppoly, pulsarname)) 
 {printf("Polycos not found!\n"); exit(1);}

 mjd is the MJD of the start of the observation (or middle)
 fppoly is the FILE *

 ....

 phcalc(mjd0, mjd1, &phase, &psrfreq);
 p = 1./psrfreq;
 fbin = dt*((float)nbins)/p;
 tbin = phase*((float)nbins);

 mjd0 is integer day
 mjd1 is the fractional day (topocentric UTC)
 tbin is current phase (of central frequency; you need to add dm offset)
 fbin is bin increment per sample.  
 
 Generally it's a good idea to
 recalculate phase and p every now and then, depending on how
 fast/relativistic the pulsar is.  I often do it every ~100 msec.

 from Ingrid Stairs

*/

#include <math.h>
#include <stdio.h>

static double f0[30],z4[30],rphase[30],mjdmid[30],mjd1mid[30],coeff[30][15];
static int isets,nblk,ncoeff,icurr;
 
int getpoly(double mjd, double *dm, FILE *fp, char *pname)
{
 
/*
  Given name,mjd returns coeffs and dm
  Reads polynomial coeffs for current source and time from polyco.dat.
  DM and binary phase info added 23 Apr 90 (new version TZ)
  Puts out two sets of mjdmids for better precision. 16 Nov 1996.
  Recovers Earth-Doppler-shift factor and polyco frequency 13 Feb 1997.
*/
 
  char name0[15],testname[15],date0[15],binpha[16];
  char dummy[3][30];
  char buffer[160];
  double aphi0b[30],adphib[30];
  double dm0,z40;
  double phase,psrfreq;
  float r,tfreq;
  long int mjddummy;
  int binary;
 
  int i,j,k,kk,len;
  int jobs,nblk0,ncoeff0;
  double mjdcheck;

  j=0;
  while (fgets(buffer,90,fp) != NULL) {
    sscanf(buffer,"%s",testname);
    if(strncmp(pname,testname,4)==0 ) {
      sscanf(buffer,"%s%s%f%ld%lf%lf%lf",
	     name0,date0,&r,&mjddummy,&mjd1mid[j],&dm0,&z40);
      fgets(buffer,80,fp);
      sscanf(buffer,"%lf%lf%d%d%d",&rphase[j],&f0[j],&jobs,&nblk0,&ncoeff0);
      fgets(buffer,80,fp);
      sscanf(buffer,"%f%16c",&r,binpha);
      for(k=0;k<ncoeff0/3;k++) {
	fgets(buffer,80,fp);
	sscanf(buffer,"%s%s%s",dummy[0],dummy[1],dummy[2]);
	for(kk=0;kk<3;kk++) {
	  len = strlen(dummy[kk]);
	  if (dummy[kk][len-4] == 'D') dummy[kk][len-4] = 'e';
	  sscanf(dummy[kk],"%lf",&coeff[j][3*k+kk]);
	}
      } 
      mjdmid[j] = mjddummy; 
      
      /* just in case its a futmid we want an mjdmid */
      if (mjdmid[j] < 20000) mjdmid[j] += 39126.;
      z4[j] = z40;
      tfreq = r;
      *dm = dm0;
      binary = (binpha[0] != ' ');
      mjdcheck = mjdmid[j] + mjd1mid[j];
      /*      printf("mjd, mjdcheck,j: %lf %lf %d\n",mjd,mjdcheck,j);   */
      if (fabs(mjd-mjdcheck)<=0.5) {
	nblk = nblk0;
	ncoeff = ncoeff0;
	if (binary)
	  sscanf(binpha,"%lf%lf",&aphi0b[j],&adphib[j]);
	if (ncoeff > 15)
	  { printf("ncoeff too big in polyco.dat.\n"); exit(1); }
	if (ncoeff < 15)
	  for(k=ncoeff;k<15;k++)
	    coeff[j][k] = 0.;
	rphase[j] -= floor(rphase[j]);
	if ((rphase[j] < 0.) || (rphase[j] > 1.))
	  { printf("rphase[%d] = %f\n",j,rphase[j]); exit(1); }
	j++;
      } 
    }
  }
  
  isets = j;
  return(isets);
 
}


/*  Compute pulsar phase and frequency at time mjd0+mjd1. */
 
phcalc(mjd0,mjd1,phase,psrfreq)
double mjd0,mjd1,*phase,*psrfreq;
{
  double dtmin;
  int i,j;
  int show;
 
  show = 0;
   
  icurr = -1;
  *psrfreq = f0[0];                     /* Default psrfreq */
  for (j=0;j<isets;j++) {
    dtmin = (mjd0-mjdmid[j]);
    dtmin = (dtmin +(mjd1-mjd1mid[j]))*1440.; /* Time from center of this set*/
    if (fabs(dtmin) <= nblk/2) {
      /*      printf("j, mjds: %d %f %f %f %f\n",j,mjd0,mjd1,mjdmid[j],mjd1mid[j]); */
      *psrfreq = 0.;                    /* Compute psrfreq and phase from */
      *phase = coeff[j][ncoeff-1];      /* the polynomial coeffs. */
      if (show) printf("phase = %21.15e   :%21.15e\n",*phase,coeff[j][ncoeff-1]);
      for(i=ncoeff-1;i>0;--i) {
        *psrfreq = dtmin*(*psrfreq) + i*coeff[j][i];
        *phase = dtmin*(*phase) + coeff[j][i-1];
      if (show) printf("phase = %21.15e   :%21.15e\n",*phase,coeff[j][i-1]);
      }
 
      *psrfreq = f0[j] + *psrfreq/60.;  /* Add in the DC terms and scale */
      *phase += rphase[j]+dtmin*60.*f0[j];
      if (show) printf("phase = %21.15e   f0: %21.15e\n",*phase,f0[j]);
      *phase -= floor(*phase);
      if ((*phase < 0.) || (*phase > 1.))
        { printf("phase = %21.15f\n",*phase); exit(1); }
      icurr = j;
      break;
    }
  }
  if (icurr == -1) {
    printf("MJD %9.3f out of range (%9.3f to %9.3f)\n",
         (mjd0+mjd1),mjdmid[0]-nblk/2880.,mjdmid[isets-1]+nblk/2880.);
    *phase = -999.;
    printf("isets = %d\n",isets);
    exit(1);
  }
}
