#include <complex>
#include <vector>
#include <iostream>
#include <cstring>
#include <fstream>
#include <algorithm>
#include "headers_Cheby/SP1D.h"
#include <random>
#include <functional>
#include <chrono>
#include <fftw3.h>
#include <iomanip>

using complex = std::complex<double>; 
using vector_real = std::vector<double>;
using vector_complex = std::vector<std::complex<double>>;
using vector_patch = std::vector<Patches>;

complex linear_interp(double x, double x0, double x1,
		      complex psi1, complex psi2){

  double xd = (x-x0)/(x1-x0);
  return psi1*(1-xd) + psi2 * xd;
}

vector_real cheby_exgrid(double N, double a, double b)           
{
  vector_real x(N+1);
  for (int i=0;i<N+1;i++)
    x[i] = (b-a)/2.*(-cos(i*M_PI/N))+(b+a)/2.0;

  return x;
}
double cheby_poly(double x, double n)
{
  double T0 = 1.0;
  double T1 = x;
  double T;

  if (n == 0) return  T0;
  else if (n == 1) return  T1;
  else if (n > 1){
    for (int i=0; i<n-1; i++){
      T = 2*x*T1-T0;
      T0 = T1;
      T1 = T;
    }
    return T;
  }
  return 0;
}

vector_complex cheby_coeff(vector_real x,vector_complex y, double N)
{

  vector_complex a;
  complex sum;
   vector_real x_chebgrid = cheby_exgrid(N,-1.0,1.0);
   for (int j=0;j<N+1;j++){
     sum = y[0]*cheby_poly(x_chebgrid[0],j) * 0.5; 

     for (int j_sum=1;j_sum<N;j_sum++)
       sum += y[j_sum]*cheby_poly(x_chebgrid[j_sum],j);    

     sum += y[N]*cheby_poly(x_chebgrid[N],j) * 0.5;
     sum *= 2.0/N;
     a.push_back(sum);
   }
   return a;
}

vector_complex cheby_interp(vector_real &x, vector_complex &coeff,double N,double a,double b)
{
  int Nx = x.size();
  vector_complex interp(Nx);
  double x_chebgrid;

  for (int ix=0; ix<Nx;ix++){
    x_chebgrid = ( 2.0 * x[ix] - a  - b ) / ( b-a);

    interp[ix]  = coeff[0]*cheby_poly(x_chebgrid,0) * 0.5;

    for (int j=1;j<N;j++)
      interp[ix] += coeff[j]*cheby_poly(x_chebgrid,j);

    interp[ix] += coeff[N]*cheby_poly(x_chebgrid,N) * 0.5;
  }

  return interp ;	  
}
complex cheby_interp(double &x, vector_complex &coeff,double N,double a,double b)
{
  //  int Nx = x.size();
  complex interp;
  double x_chebgrid;

    x_chebgrid = ( 2.0 * x - a  - b ) / ( b-a);

    interp  = coeff[0]*cheby_poly(x_chebgrid,0) * 0.5;

    for (int j=1;j<N;j++)
      interp += coeff[j]*cheby_poly(x_chebgrid,j);

    interp += coeff[N]*cheby_poly(x_chebgrid,N) * 0.5;

  return interp ;	  
}

void cheby_DD_matrix(vector_real &D, vector_real &x, unsigned long int N)
{

  D.resize((N+1)*(N+1));
  vector_real dX((N+1)*(N+1));
  for(int i=0; i<N+1;i++){
    for(int j=0; j<N+1;j++){
      dX[i*(N+1)+j] = x[j] - x[i];
      if (i==j) dX[i*(N+1)+j] += 1.0;
      if      ((i==0 || i==N) && (j>0  && j<N))   D[i*(N+1)+j] = pow(-1,j+i) * 2.0; 
      else if ((i==0 || i==N) && (j==0 || j==N)) D[i*(N+1)+j] = pow(-1,j+i) * 1.0; 
      else if ((j==0 || j==N) && (i>0  && i<N)) D[i*(N+1)+j] = pow(-1,j+i) * 0.5;
      else D[i*(N+1)+j] = pow(-1,j+i) * 1.0;
      D[i*(N+1)+j] /= dX[i*(N+1)+j]; 
      //      std::cout<< D[i*(N+1)+j] << " "; 
    }
    //    std::cout <<"\n";
  }

  // correction diagonal
  double sum[N+1];
  for(int i=0; i<N+1;i++){
    sum[i] = 0;
    for(int j=0; j<N+1;j++) sum[i] += D[i*(N+1)+j];        
  }
  for(int i=0; i<N+1;i++) D[i*(N+1)+i] -= sum[i];

  // make D^2
  for(int i=0; i<N+1;i++) {
    for(int j=0; j<N+1; j++){
      dX[i*(N+1)+j] = 0.0;
      for(int k=0; k<N+1; k++){
	dX[i*(N+1)+j] += D[i*(N+1)+k]* D[k*(N+1)+j] ;
      }
    }
  }
  for(int i=0; i<(N+1)*(N+1);i++) D[i] = dX[i];
}
void fft_r2c(vector_real &x, vector_complex &y, bool forward)
{
  fftw_complex *out = reinterpret_cast<fftw_complex*>(y.data());  // standard declaration of output fttw
  fftw_plan p;

  if (forward)
  {
    p = fftw_plan_dft_r2c_1d(x.size(), x.data(), out, FFTW_ESTIMATE);
    fftw_execute(p);
    fftw_destroy_plan(p);
    for (size_t i = 0; i < x.size(); ++i)
    {
      y[i] = y[i]/sqrt(x.size());
    }
  }

  else 
  {
    p = fftw_plan_dft_c2r_1d(x.size(), out, x.data(), FFTW_ESTIMATE);
    fftw_execute(p);
    fftw_destroy_plan(p);
    for (size_t i = 0; i < x.size(); ++i)
    {
      x[i] = x[i]/sqrt(x.size());
    }
  }
}  

void fft(vector_complex &x, bool inverse)
{
  vector_complex y(x.size(), complex(0.0, 0.0));
  fftw_plan p;

  fftw_complex *in = reinterpret_cast<fftw_complex*>(x.data());   // standard declaration of input fttw
  fftw_complex *out = reinterpret_cast<fftw_complex*>(y.data());  // standard declaration of output fttw

  p = fftw_plan_dft_1d(x.size(), in, out, (inverse ? FFTW_BACKWARD : FFTW_FORWARD), FFTW_ESTIMATE);

  fftw_execute(p);
  fftw_destroy_plan(p);

  for (size_t i = 0; i < x.size(); ++i)
  {
    x[i] = y[i] / sqrt(static_cast<double>(x.size()));   // fftw give unnormalized output, needs renormalization here.
  }
} 
complex GWP_vals(double x,double y, double Time)
{
  
  double hbar = 1.;
  double m =1.;
  double  sigma_px = 8, sigma_py=8;
  //  double  vx0 =  v0_init ; //2e1;
  double  vx0 =  -5e0,  vy0 = -5e0; //2e1;
  double  x0 = x0_init;
  double  y0 = x0_init;

  double sigma_x, phase_x, M_x;
  double sigma_y, phase_y, M_y;

  sigma_x = hbar/(2*sigma_px) * pow(1+4*pow(sigma_px,4)/(hbar*hbar)*Time*Time/(m*m),1./2);
  phase_x = 1./hbar * (m*vx0+sigma_px*sigma_px/(sigma_x*sigma_x)*Time/(2*m)*(x-x0-vx0*Time))*
    (x-x0-vx0*Time) + vx0*m/(2*hbar) * vx0*Time - atan(2*sigma_px*sigma_px*Time/(hbar*m))/ 2 ;
  M_x = 1./(pow(2*M_PI,1./4)*pow(sigma_x,1./2)) * exp(-(x-x0-vx0*Time)*(x-x0-vx0*Time)/(4*sigma_x*sigma_x)) ;

  sigma_y = hbar/(2*sigma_py) * pow(1+4*pow(sigma_py,4)/(hbar*hbar)*Time*Time/(m*m),1./2);
  phase_y = 1./hbar * (m*vy0+sigma_py*sigma_py/(sigma_y*sigma_y)*Time/(2*m)*(y-y0-vy0*Time))*
    (y-y0-vy0*Time) + vy0*m/(2*hbar) * vy0*Time - atan(2*sigma_py*sigma_py*Time/(hbar*m))/ 2 ;
  M_y = 1./(pow(2*M_PI,1./4)*pow(sigma_y,1./2)) * exp(-(y-y0-vy0*Time)*(y-y0-vy0*Time)/(4*sigma_y*sigma_y));

  return complex(M_x*M_y*cos(phase_x+phase_y),M_x*M_y*sin(phase_x+phase_y));
}

void GWP(Params &par, Operators &opr,double Time)
{
  
  unsigned long int N = opr.size;
  double hbar = 1.;
  double m =1.;
  double  sigma_px = 8;
  double  vx0 =  v0_init ; //2e1;
  double  x0 = x0_init;

  //  double Time = 0;
  double sigma_x, phase_x, M_x;


  for (size_t i = 0; i < N+1; ++i){
    sigma_x = hbar/(2*sigma_px) * pow(1+4*pow(sigma_px,4)/(hbar*hbar)*Time*Time/(m*m),1./2);
    phase_x = 1./hbar * (m*vx0+sigma_px*sigma_px/(sigma_x*sigma_x)*Time/(2*m)*(par.x[i]-x0-vx0*Time))*
      (par.x[i]-x0-vx0*Time) + vx0*m/(2*hbar) * vx0*Time - atan(2*sigma_px*sigma_px*Time/(hbar*m))/ 2 ;
    M_x = 1./(pow(2*M_PI,1./4)*pow(sigma_x,1./2)) * exp(-(par.x[i]-x0-vx0*Time)*(par.x[i]-x0-vx0*Time)/(4*sigma_x*sigma_x)) ;

    opr.phi[i] = M_x*M_x; //imag
	 //	 std::cout << opr.wfc[k+N*(j+N*i)] <<  "\n"; 
    if (i==0) {
      // sigma_x = hbar/(2*sigma_px) * pow(1+4*pow(sigma_px,4)/(hbar*hbar)*Time*Time/(m*m),1./2);
      // phase_x = 1./hbar * (m*vx0+sigma_px*sigma_px/(sigma_x*sigma_x)*Time/(2*m)*(-x0-vx0*Time))*
      // 	(-x0-vx0*Time) + vx0*m/(2*hbar) * vx0*Time - atan(2*sigma_px*sigma_px*Time/(hbar*m))/ 2 ;
      // M_x = 1./(pow(2*M_PI,1./4)*pow(sigma_x,1./2)) * exp(-(-x0-vx0*Time)*(-x0-vx0*Time)/(4*sigma_x*sigma_x)) ;

      sigma_x = hbar/(2*sigma_px) * pow(1+4*pow(sigma_px,4)/(hbar*hbar)*Time*Time/(m*m),1./2);
      phase_x = 1./hbar * (m*vx0+sigma_px*sigma_px/(sigma_x*sigma_x)*Time/(2*m)*(par.x[i]-x0-vx0*Time))*
      	(par.x[i]-x0-vx0*Time) + vx0*m/(2*hbar) * vx0*Time - atan(2*sigma_px*sigma_px*Time/(hbar*m))/ 2 ;
      M_x = 1./(pow(2*M_PI,1./4)*pow(sigma_x,1./2)) * exp(-(par.x[i]-x0-vx0*Time)*(par.x[i]-x0-vx0*Time)/(4*sigma_x*sigma_x)) ;
      //opr.wfc[i].real( M_x*cos(phase_x)); 
      //opr.wfc[i].imag( M_x*sin(phase_x)); 
      par.a_R = M_x*cos(phase_x); 
      par.a_I = M_x*sin(phase_x); 
    }
    if (i==N) {
      // sigma_x = hbar/(2*sigma_px) * pow(1+4*pow(sigma_px,4)/(hbar*hbar)*Time*Time/(m*m),1./2);
      // phase_x = 1./hbar * (m*vx0+sigma_px*sigma_px/(sigma_x*sigma_x)*Time/(2*m)*(par.xmax-x0-vx0*Time))*
      // 	(par.xmax-x0-vx0*Time) + vx0*m/(2*hbar) * vx0*Time - atan(2*sigma_px*sigma_px*Time/(hbar*m))/ 2 ;
      // M_x = 1./(pow(2*M_PI,1./4)*pow(sigma_x,1./2)) * exp(-(par.xmax-x0-vx0*Time)*(par.xmax-x0-vx0*Time)/(4*sigma_x*sigma_x)) ;

      sigma_x = hbar/(2*sigma_px) * pow(1+4*pow(sigma_px,4)/(hbar*hbar)*Time*Time/(m*m),1./2);
      phase_x = 1./hbar * (m*vx0+sigma_px*sigma_px/(sigma_x*sigma_x)*Time/(2*m)*(par.x[i]-x0-vx0*Time))*
      	(par.x[i]-x0-vx0*Time) + vx0*m/(2*hbar) * vx0*Time - atan(2*sigma_px*sigma_px*Time/(hbar*m))/ 2 ;
      M_x = 1./(pow(2*M_PI,1./4)*pow(sigma_x,1./2)) * exp(-(par.x[i]-x0-vx0*Time)*(par.x[i]-x0-vx0*Time)/(4*sigma_x*sigma_x)) ;
      // opr.wfc[i].real( M_x*cos(phase_x)); 
      //opr.wfc[i].imag( M_x*sin(phase_x)); 
      par.b_R = M_x*cos(phase_x); 
      par.b_I = M_x*sin(phase_x); 
    }

  }
}
void initial_GWP(vector_real &x, 
		 vector_real &y,
		 vector_complex &psi)
{
  
  unsigned long int N = x.size();
  double Time = 0;

  for (size_t i = 0; i < N; ++i){
  for (size_t j = 0; j < N; ++j){
    psi[i*N+j] = GWP_vals(x[i], y[j], Time); //imag
    //    std::cout << x[i]<< " " << std::abs(psi[i]) <<  "\n"; 
  }}
  //  write_data(par, opr, 0);

}
void Print(Params &par,
	   vector_real &x,
	   vector_real &y,
	   vector_complex  &psi,
	   int tn){

  double size = x.size()-1;

  for (int i=0; i< size+1; i++){
  for (int j=0; j< size+1; j++){
    std::cout << x[i] << " " 
  	      << std::abs(psi[i]) << " " 
  	      << std::abs(GWP_vals(x[i],y[j], tn*par.dt))<< " "
  	      << psi[i*(size+1)+j].real() << " " 
  	      << GWP_vals(x[i],y[j], tn*par.dt).real()<< " " 
  	      << psi[i*(size+1)+j].imag() << " " 
  	      << GWP_vals(x[i],y[j], tn*par.dt).imag()<< "\n"; 
  }}
}
void Print_file(Params &par,
		vector_patch &P,
		unsigned long int NsubD,
		int tn){

  int size,D;

  std::stringstream filename_fdm;
  filename_fdm << "output/" << tn << ".dat";
  std::ofstream ffdm = std::ofstream(filename_fdm.str());

  if (ffdm)
  {
    std::stringstream data_fdm;

    for (int Di=0;Di<NsubD;Di++){
      for (int Dj=0;Dj<NsubD;Dj++){
	D = Di*NsubD+Dj;
	for (int i = 1; i < P[D].N; ++i){
	for (int j = 1; j < P[D].N; ++j){

  	  data_fdm << i + Di*(P[D].N+1) << " "
		   << j + Dj*(P[D].N+1) << " " 
  		   << P[D].x[i] << " " 
  		   << P[D].y[j] << " " 
  		   << std::abs(P[D].psi[i*(P[D].N+1)+j]) << " " 
  		   << std::abs(GWP_vals(P[D].x[i],P[D].y[j], tn*par.dt))<< " "
  		   << P[D].psi[i*(P[D].N+1)+j].real() << " " 
  		   << GWP_vals(P[D].x[i],P[D].y[j], tn*par.dt).real()<< " " 
  		   << P[D].psi[i*(P[D].N+1)+j].imag() << " " 
  		   << GWP_vals(P[D].x[i], P[D].y[j],tn*par.dt).imag()<< " "
  		   << std::abs(P[D].debug[i*(P[D].N+1)+j]) << "\n" ;
  	}
      }
      }
    }
    ffdm.write(data_fdm.str().c_str(), data_fdm.str().length());
    ffdm.close();
  }


  //  }
}
void Error(Params &par,
	   vector_real &x,
	   vector_real &y,
	   vector_complex  &psi,
	   double &Error){
  
  double size = x.size();
  for (size_t i = 1; i < size-1; ++i){
  for (size_t j = 1; j < size-1; ++j){
    Error += pow(std::abs(psi[i*(size)+j])
	       - std::abs(GWP_vals(x[i],y[j], par.timesteps*par.dt)),2);
  }}
  //  error = pow(error,1./2)/opr.size;  
  //  std::cout << "error = " << error << "\n";    
}
void RK2(int N, vector_real &D,vector_complex &psi, Params &par){

  vector_complex k1(N+1),tmp(N+1);
    for(int i=1; i<N;i++) {
      k1[i] = complex(0.0,0.0);
            for(int j=1; j<N; j++) k1[i] += D[i*(N+1)+j] * psi[j] ;
    }
    for(int i=1; i<N;i++) k1[i] = psi[i] + k1[i] * 0.25 * par.dt * complex(0.0,1.0);

    for(int i=1; i<N;i++) {
      tmp[i] = complex(0.0,0.0);
      for(int j=1; j<N; j++) tmp[i] += D[i*(N+1)+j] * k1[j] ;
    }
    for(int i=1; i<N;i++) psi[i] += tmp[i] * 0.5 * par.dt * complex(0.0,1.0);
}

void Refinement(Patches &P){
  
  // Perform Refinement: double cheby grid size

  // for (int i=0; i< P.N+1; i++){
  //   std::cout << P.x[i] << " " 
  // 	      << P.psi[i].real() << " " 
  // 	      << P.psi[i].imag() <<"\n";
  // }
  P.coeff = cheby_coeff(P.x,P.psi,P.N);
  P.x  = cheby_exgrid(2*P.N,P.x[0],P.x[P.N]);
  P.psi = cheby_interp(P.x,P.coeff,P.N,P.x[0],P.x[2*P.N]); 
  cheby_DD_matrix(P.D, P.x, 2*P.N);
  P.level = 1;

  P.N *= 2.0; 
  P.tmp.resize(P.N);
  P.k1.resize(P.N);
  P.k2.resize(P.N);
  P.k3.resize(P.N);
  P.k4.resize(P.N);
  // std::cout <<"\n";

  // for (int i=0; i< P.N+1; i++){
  //   std::cout << P.x[i] << " " 
  // 	      << P.psi[i].real() << " " 
  // 	      << P.psi[i].imag() <<"\n";
  // }
  //   std::exit(0);
}

void Derefinement(Patches &P){
  
  // Perform De-Refinement: half cheby grid size

  // for (int i=0; i< P.N+1; i++){
  //   std::cout << P.x[i] << " " 
  // 	      << P.psi[i].real() << " " 
  // 	      << P.psi[i].imag() <<"\n";
  // }
  double half = 0.5 ; 
  P.coeff = cheby_coeff(P.x,P.psi,P.N);
  P.x  = cheby_exgrid(half*P.N,P.x[0],P.x[P.N]);
  P.psi = cheby_interp(P.x,P.coeff,P.N,P.x[0],P.x[half*P.N]); 
  cheby_DD_matrix(P.D, P.x, half*P.N);
  P.level = 0;

  P.N *= half; 
  P.tmp.resize(P.N);
  P.k1.resize(P.N);
  P.k2.resize(P.N);
  P.k3.resize(P.N);
  P.k4.resize(P.N);
  //  std::cout <<"\n";

  // for (int i=0; i< P.N+1; i++){
  //   std::cout << P.x[i] << " " 
  // 	      << P.psi[i].real() << " " 
  // 	      << P.psi[i].imag() <<"\n";
  // }

  //   std::exit(0);
}

void Assign_BC(vector_patch &Patch,
	       signed long int NsubD,
	       Params &par,
	       double tn){

  //    std::cout << tn <<"\n";
  //  unsigned long int N = Patch[0].N;

  int N , D, Dfrom;

  for (int Di=0;Di<NsubD;Di++) {
  for (int Dj=0;Dj<NsubD;Dj++) {
    D = Di*NsubD + Dj;
    N = Patch[D].N;

    if (Di==0){      
      for (int j = 0; j<N+1; j++){
	Patch[D].a[j] = GWP_vals(Patch[D].x[0],Patch[D].y[j],(tn-1.)*par.dt); 
	Patch[D].psi[j] = Patch[D].a[j];
	Patch[D].tmp[j] = Patch[D].a[j];
      }
    }
    else if (Patch[(Di-1)*NsubD+Dj].level==0){
      Dfrom = (Di-1)*NsubD+Dj;
      for (int j = 1; j<N; j++){
	Patch[D].a[j] = Patch[Dfrom].tmp[(Patch[Dfrom].N-1)*(Patch[Dfrom].N+1)+j]; 
	Patch[D].psi[j] = Patch[D].a[j];
	Patch[D].tmp[j] = Patch[D].a[j];
      }
    }
    // else if (Patch[D-1].level==1){
    //   Patch[D].a = Patch[D-1].tmp[Patch[D-1].N-2]; 
    //   Patch[D].psi[0] = Patch[D].a;
    // }

    // B face
    if (Di==(NsubD-1)){
      for (int j = 0; j<N+1; j++){
	Patch[D].b[j] = GWP_vals(Patch[D].x[N],Patch[D].y[j],(tn-1.)*par.dt); 
	Patch[D].psi[N*(N+1)+j] = Patch[D].b[j];
	Patch[D].tmp[N*(N+1)+j] = Patch[D].b[j];
	//	std::cout << "b"<<j<<"="<< Patch[D].b[j] <<"\n";
      }
    }
    else if (Patch[(Di+1)*NsubD+Dj].level==0){
      Dfrom = (Di+1)*NsubD+Dj;
      for (int j = 1; j<N; j++){
	Patch[D].b[j] = Patch[Dfrom].tmp[(Patch[Dfrom].N+1)+j]; 
	Patch[D].psi[N*(N+1)+j] = Patch[D].b[j];
	Patch[D].tmp[N*(N+1)+j] = Patch[D].b[j];
      }
    }
    // else if (Patch[D+1].level==1){
    //   Patch[D].b = Patch[D+1].tmp[2]; 
    //   Patch[D].psi[Patch[D].N] = Patch[D].b;
    // }
    //    std::cout << Patch[D].a  << " " << Patch[D].b <<"\n";

    // C face
    if (Dj==0){
      for (int i = 0; i<N+1; i++){
	Patch[D].c[i] = GWP_vals(Patch[D].x[i],Patch[D].y[0],(tn-1.)*par.dt); 
	Patch[D].psi[i*(N+1)] = Patch[D].c[i];
	Patch[D].tmp[i*(N+1)] = Patch[D].c[i];
	//	std::cout << "c"<<i<<"="<< Patch[D].c[i] <<"\n";
      }
    }
    else if (Patch[Di*NsubD+Dj-1].level==0){
      Dfrom = Di*NsubD+Dj-1;
      for (int i = 1; i<N; i++){
	Patch[D].c[i] = Patch[Dfrom].tmp[i*(Patch[Dfrom].N+1)+Patch[Dfrom].N-1]; 
	Patch[D].psi[i*(N+1)] = Patch[D].c[i];
	Patch[D].tmp[i*(N+1)] = Patch[D].c[i];
      }
    }

    // D face
    if (Dj==(NsubD-1)){
      for (int i = 0; i<N+1; i++){
	Patch[D].d[i] = GWP_vals(Patch[D].x[i],Patch[D].y[N],(tn-1.)*par.dt); 
	Patch[D].psi[i*(N+1)+N] = Patch[D].d[i];
	Patch[D].tmp[i*(N+1)+N] = Patch[D].d[i];
	//	std::cout << "d"<<i<<"="<< Patch[D].d[i] <<"\n";
      }
    }
    else if (Patch[Di*NsubD+Dj+1].level==0){
      Dfrom = Di*NsubD+Dj+1;
      for (int i = 1; i<N; i++){
	Patch[D].d[i] = Patch[Dfrom].tmp[i*(Patch[Dfrom].N+1)+1]; 
	Patch[D].psi[i*(N+1)+N] = Patch[D].d[i];
	Patch[D].tmp[i*(N+1)+N] = Patch[D].d[i];
      }
    }
  }}
  //  std::exit(0);
}

void RKstep(Patches &P, 
	    Params &par, 
	    vector_complex &k,
	    double cst)
{

  // Do chebyshev D2 multiplicaation
  for(unsigned long int i=1; i<P.N;i++)
    for(unsigned long int j=1; j<P.N;j++)
      k[i*(P.N+1)+j] = complex(0.0,0.0);

  for(unsigned long int i=1; i<P.N;i++){
    for(unsigned long int j=1; j<P.N;j++){
      for(unsigned long int t=0; t<P.N+1; t++){ 
		k[i*(P.N+1)+j] += P.D[j*(P.N+1)+t] * P.tmp[i*(P.N+1)+t] ;
		k[j*(P.N+1)+i] += P.D[j*(P.N+1)+t] * P.tmp[t*(P.N+1)+i] ;
    }}}

  for(int i=1; i<P.N;i++)
  for(int j=1; j<P.N;j++)
    P.tmp[i*(P.N+1)+j] = P.psi[i*(P.N+1)+j] + k[i*(P.N+1)+j] * cst * 0.5 * par.dt * complex(0.0,1.0);

}
void AMR_Scheme(Patches &P){
  double threshold = 0.1;

  double ave = 0.0;
  for (size_t j = 1; j < P.N-1; ++j)
    ave += std::abs(P.psi[j]);

  ave /= P.N;

  if ((ave>threshold) && (P.level==0)) {
    Refinement(P);
    std::cout <<"refined?\n";
  }
  else if ((ave<threshold) && (P.level==1)) {
    Derefinement(P);
    std::cout <<"Derefined?\n";
  }
  // if ((std::abs(P.psi[j])>threshold) && (P.level==0)) {
  //   Refinement(P);
  //   std::cout <<"refined?\n";
  //   break;
  // }
  // else if ((std::abs(P.psi[j])<threshold) && (P.level==1)) {
  //   Derefinement(P);
  //   std::cout <<"Derefined?\n";
  //   break;
  // }

}
void Time_evolv_RK1(vector_patch &Patch,
		    signed long int NsubD,
		    Params &par,
		    double tn){

  // for (int D=0;D<NsubD*NsubD;D++){
  //   for (size_t i = 0; i < Patch[D].N+1; ++i){
  //   for (size_t j = 0; j < Patch[D].N+1; ++j){
  //     std::cout << i << " " << j << " " << std::abs(Patch[D].psi[i*(Patch[D].N+1)+j]) << "\n";
  // }}}
  // std::exit(0);

  // initialize
  for (int D=0;D<NsubD*NsubD;D++){
    for (size_t j = 0; j < (Patch[D].N+1)*(Patch[D].N+1) ; ++j){
      Patch[D].tmp[j] = Patch[D].psi[j]; 
  }}

  // transform homogeneous problem
  Assign_BC(Patch,NsubD,par,tn);

  for (int D=0;D<NsubD*NsubD;D++){
    Patch[D].aold = Patch[D].a;
    Patch[D].bold = Patch[D].b;
    Patch[D].cold = Patch[D].c;
    Patch[D].dold = Patch[D].d;    
  }
  //  std::cout << Patch[0].N+1 <<"\n";

  //  for (int D=0;D<NsubD;D++) AMR_Scheme(Patch[D]);

  // std::cout << Patch[0].x[0] << " " << Patch[0].x[Patch[0].N] << " " << Patch[0].L << "\n";
  for (int D=0;D<NsubD*NsubD;D++){
    for (size_t i = 0; i < Patch[D].N+1; ++i){
    for (size_t j = 0; j < Patch[D].N+1; ++j){
      Patch[D].psi[i*(Patch[D].N+1)+j]  -= (((1.-(Patch[D].x[i] - Patch[D].x[0])/Patch[D].L)* Patch[D].aold[j] + (Patch[D].x[i]-Patch[D].x[0])/Patch[D].L* Patch[D].bold[j])
  					    + ((1.-(Patch[D].y[j] - Patch[D].y[0])/Patch[D].L)* Patch[D].cold[i] + (Patch[D].y[j]-Patch[D].y[0])/Patch[D].L* Patch[D].dold[i]))
  	                                 - ((1.-(Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * (1.-(Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].aold[0]) 
                                   	 - ((1.-(Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * ((Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].aold[Patch[D].N]) 
  			                 - (((Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * (1.-(Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].bold[0]) 
  			                 - (((Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * ((Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].bold[Patch[D].N]); 
    }}}

  for(unsigned long int i=1; i<Patch[0].N;i++)
    for(unsigned long int j=1; j<Patch[0].N;j++)
      Patch[0].k1[i*(Patch[0].N+1)+j] = complex(0.0,0.0);

  for(unsigned long int i=1; i<Patch[0].N;i++){
    for(unsigned long int j=1; j<Patch[0].N;j++){
      for(unsigned long int t=1; t<Patch[0].N; t++){ 
		Patch[0].k1[i*(Patch[0].N+1)+j] += Patch[0].D[j*(Patch[0].N+1)+t] * Patch[0].psi[i*(Patch[0].N+1)+t] ;
		Patch[0].k1[j*(Patch[0].N+1)+i] += Patch[0].D[j*(Patch[0].N+1)+t] * Patch[0].psi[t*(Patch[0].N+1)+i] ;
    }}}
  
  for(int i=1; i<Patch[0].N;i++) 
  for(int j=1; j<Patch[0].N;j++) 
    Patch[0].psi[i*(Patch[0].N+1)+j] += Patch[0].k1[i*(Patch[0].N+1)+j] * 0.5 * par.dt * complex(0.0,1.0);

  // // de-homogenization
  for (int D=0;D<NsubD*NsubD;D++){
    for (size_t i = 1; i < Patch[D].N; ++i){
    for (size_t j = 1; j < Patch[D].N; ++j){
      Patch[D].psi[i*(Patch[D].N+1)+j]  += ((1.-(Patch[D].x[i] - Patch[D].x[0])/Patch[D].L)* Patch[D].aold[j] + (Patch[D].x[i]-Patch[D].x[0])/Patch[D].L* Patch[D].bold[j])
  	                                + ((1.-(Patch[D].y[j] - Patch[D].y[0])/Patch[D].L)* Patch[D].cold[i] + (Patch[D].y[j]-Patch[D].y[0])/Patch[D].L* Patch[D].dold[i])
  	                                 - ((1.-(Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * (1.-(Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].aold[0]) 
                                   	 - ((1.-(Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * ((Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].aold[Patch[D].N]) 
  			                 - (((Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * (1.-(Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].bold[0]) 
  			                 - (((Patch[D].x[i] - Patch[D].x[0])/Patch[D].L) * ((Patch[D].y[j] - Patch[D].y[0])/Patch[D].L) * Patch[D].bold[Patch[D].N]); 

  //     //      std::cout << i << " " << j << " " << std::abs(Patch[D].psi[i*(Patch[D].N+1)+j]) << "\n";
  }}}
  //  Print_file(par,Patch,NsubD,tn);

  //  std::exit(0);
}

void Time_evolv(vector_patch &Patch,
		signed long int NsubD,
		Params &par,
		double tn){


  // initialize
  for (int D=0;D<NsubD*NsubD;D++){
    for (size_t j = 0; j < (Patch[D].N+1)*(Patch[D].N+1) ; ++j){
      Patch[D].tmp[j] = Patch[D].psi[j]; 
  }}

  // transform homogeneous problem
  Assign_BC(Patch,NsubD,par,tn);


  //  for (int D=0;D<NsubD;D++) AMR_Scheme(Patch[D]);

  // Runge kutta 4
  for (int D=0;D<NsubD*NsubD;D++)   RKstep(Patch[D],par,Patch[D].k1,0.5);

  Assign_BC(Patch,NsubD,par,tn+0.5);
  for (int D=0;D<NsubD*NsubD;D++)   RKstep(Patch[D],par,Patch[D].k2,0.5);

  Assign_BC(Patch,NsubD,par,tn+0.5);
  for (int D=0;D<NsubD*NsubD;D++)   RKstep(Patch[D],par,Patch[D].k3,1.0);

  Assign_BC(Patch,NsubD,par,tn+1.0);
  for (int D=0;D<NsubD*NsubD;D++)   RKstep(Patch[D],par,Patch[D].k4,1.0);

  for (int D=0;D<NsubD*NsubD;D++){
    for (size_t i = 1; i < Patch[D].N; ++i){
    for (size_t j = 1; j < Patch[D].N; ++j){
	Patch[D].psi[i*(Patch[D].N+1)+j] += 1./6 * (Patch[D].k1[i*(Patch[D].N+1)+j] 
    					  + 2.0*Patch[D].k2[i*(Patch[D].N+1)+j] 
					  + 2.0*Patch[D].k3[i*(Patch[D].N+1)+j] 
					  + Patch[D].k4[i*(Patch[D].N+1)+j]) * 0.5 * par.dt * complex(0.0,1.0);
	//	std::cout << Patch[D].psi[i] << "\n";
    }}}
  
}

int main(int argc, char **argv)
{
  unsigned long int N,Nsub;
  signed long int NsubD;

  if (argc != 4) {
    std::cout << "ERROR: Arguments "<< argc << "!= 5 \n";
    std::exit(0);
  }
  else{
    inp_timestep = std::atof(argv[1]);
    N = std::atoi(argv[2]) ;
    NsubD = std::atoi(argv[3]) ;
    //    NsubD = std::atoi(argv[4]) ;
    //    inp_dt = 0.5*0.05/inp_timestep; // 
    inp_dt = 0.1/inp_timestep; // 
  }
  
  //  std::cout << N+1<<"\n";
  Params par = Params(inp_xmax, inp_res, inp_dt, inp_timestep, inp_np, inp_im, inp_xoffset); // xmax, res, dt, timestep, np, im
  Operators opr = Operators(par, 0.0, inp_xoffset);
  double Coeff = 0.5 * par.dt / (par.dx*par.dx);

  // sub domain parameters
  //  unsigned long int Nsub = 20 ; // doesnt matter if no refinement
  complex a1,b1,a2,b2,a3,b3,a4,b4;
  double L = par.xmax/((double) NsubD+(1-NsubD)*(1.0-cos(M_PI/N))/2.);
  vector_patch Patch;
  
  for (int i=0;i<NsubD*NsubD;i++) {
    Patch.push_back(Patches(N));
    Patch[i].L = L;
  }

  // create subdomain x
  for (size_t i = 0; i < N+1; ++i){
    for (int D=0;D<NsubD*NsubD;D++) {
      Patch[D].x[i] = L/2.0*(-cos(i*M_PI/N)) + L/2.0 ;
      Patch[D].y[i] = L/2.0*(-cos(i*M_PI/N)) + L/2.0 ;
    }
  }

  double dx_sub = Patch[0].x[1]-Patch[0].x[0];

  for (size_t i = 0; i < N+1; ++i){
    for (int Di=0;Di<NsubD;Di++) {
    for (int Dj=0;Dj<NsubD;Dj++) {
      Patch[Di*NsubD+Dj].x[i] += Di * (L-dx_sub);
      Patch[Di*NsubD+Dj].y[i] += Dj * (L-dx_sub);
    }}
  }

  // make cheby D matrix
  for (int D=0;D<NsubD*NsubD;D++)  cheby_DD_matrix(Patch[D].D, Patch[D].x,Patch[D].N);

  // initialize GWP
  for (int D=0; D<NsubD*NsubD;D++)   initial_GWP(Patch[D].x,Patch[D].y,Patch[D].psi);
//  std::exit(0);
  Print_file(par,Patch,NsubD,0);

  std::chrono::steady_clock::time_point begin2 = std::chrono::steady_clock::now();
  
  //    std::cout<< Patch[0].N+1 <<"\n";
  //  Print_file(par,Patch,NsubD,0);
  for (int tn = 1; tn <= par.timesteps; ++tn){

    // RK4
    Time_evolv(Patch,
	       NsubD,
	       par,
	       tn);
    //    Print_file(par,Patch,NsubD,tn);
    //    if (tn%1000==0) Print_file(par,Patch,NsubD,tn);
    //if (tn%1000==0) std::cout << tn << "\n";
 } //   for (int tn = 0; tn <= par.timesteps; ++tn){

  double err = 0.0;
  double NN = N + (N-1)*(NsubD-1) ;

  //  for (int D=0;D<NsubD;D++)   Print(par,Patch[D].x,Patch[D].psi,par.timesteps);
  Print_file(par,Patch,NsubD,par.timesteps);
  for (int D=0;D<NsubD*NsubD;D++)   Error(par,Patch[D].x,Patch[D].y,Patch[D].psi,err);

  std::cout << "Error =" << pow(err,1./2)/pow(NN,2) << "\n";
  std::cout << NN <<"\n";
  std::cout << "CFL constant = "<< par.dt/(dx_sub * dx_sub) << "\n";

  Print_file(par,Patch,NsubD,par.timesteps);
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  //  std::cout << "simulation takes = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin2).count()*1e-6<< " seconds"<< std::endl;

return 0;
}
