/*----------------------------------------------------*/
/*   Spatio-temporal models based on factor copula    */
/*   Copyright: Lei Hua, larry.lei.hua@gmail.com      */
/*   License: GPL-3                                   */
/*----------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <RcppArmadillo.h>
#include <RcppGSL.h>
#include <vector>
#include <complex>
#include <string>
#include <numeric>
#include <algorithm>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_roots.h>
#include <gsl/gsl_sf.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_randist.h>

// [[Rcpp::depends(RcppGSL)]]

// [[Rcpp::depends(RcppArmadillo)]]

using std::pow; using std::log;
using std::exp; using std::vector;
using std::complex; using std::size_t;
using Rcpp::NumericVector;
using Rcpp::NumericMatrix;

//////////////////////////////////////
// Gaussian Quadratures             //
// based on CopulaModel, R package  //
//////////////////////////////////////
void gauleg(int, vector<double>&, vector<double>&);
  
// xq: nodes;  wq: weights
#define EPS 3.0e-11
void gauleg(int nq, vector<double>& xq, vector<double>& wq)
{
    size_t m,j,i,n;
    double z1,z,xm,xl,pp,p3,p2,p1;
    n = nq; 
    m = (n+1)/2;
  
    // boundary points 
    double x1 = 0;
    double x2 = 1;
    
    xm = 0.5*(x2 + x1);
    xl = 0.5*(x2 - x1);
    for(i=1;i<=m;++i) // yes, i starts from 1
    {
        z = cos(3.14159265358979323846*(i-0.25)/(n+0.5));
        do
        {
            p1=1.0;
            p2=0.0;
            for(j=1;j<=n;j++)
            {
                p3=p2;
                p2=p1;
                p1=((2.0*j-1.0)*z*p2-(j-1.0)*p3)/j;
            }
            pp=n*(z*p1-p2)/(z*z-1.0);
            z1=z;
            z=z1-p1/pp;
        }while (fabs(z-z1) > EPS);
        
        xq[i-1] = xm-xl*z;
        xq[n-i]=xm+xl*z;
        wq[i-1]=2.0*xl/((1.0-z*z)*pp*pp);
        wq[n-i]=wq[i-1];
  }
}
#undef EPS

/////////////////////////////////////
// bivariate normal copula density //
/////////////////////////////////////
double denB1(double u, double v, double r)
{
    double tem0, tem1, tem2, tem3, tem4, x, y;
    
    x = gsl_cdf_ugaussian_Pinv(u);
    y = gsl_cdf_ugaussian_Pinv(v);
    
    tem0 = (1- pow(r,2));
    tem1 = pow( tem0, -0.5 );
    tem2 = pow(x,2) +  pow(y,2);
    tem3 = exp( -0.5 / tem0 * ( tem2 - 2*r*x*y  ) );
    tem4 = exp( tem2 / 2 );
    
    return tem1 * tem3 * tem4;
}


// C_{1|2} of B1:
double C2B1(double u, double v, double rho)
{
  double x1 = gsl_cdf_ugaussian_Pinv(u);
  double x2 = gsl_cdf_ugaussian_Pinv(v);
  double mu = rho * x2;
  double sig = pow( (1 - pow(rho, 2)), 0.5);
  return gsl_cdf_gaussian_P(x1 - mu, sig);
}

// density of shifted Poisson
// x = 1, 2, 3, ...
double dSPoi(double x, double lam){
    double out;
    out = exp(-lam) * pow(lam, x-1) / gsl_sf_fact(x-1);
    return out;
}

// CDF of shifted Poisson
// x = 1, 2, 3, ...
double FSPoi(double x, double lam){
    double out = 0.0;
    int i;
    for(i=1; i<=x; ++i){
        out += exp(-lam) * pow(lam, i-1) / gsl_sf_fact(i-1);
    }
    return out;
}

// [[Rcpp::export]]
double nllk_p_C(NumericVector par, NumericMatrix dat, NumericMatrix centers, int Kcen, double g){
    NumericVector tt = dat(Rcpp::_, 0);
    NumericVector y = dat(Rcpp::_, 1);
    NumericVector LON = dat(Rcpp::_, 2);
    NumericVector LAT = dat(Rcpp::_, 3);
    
    const int M = 12;  // number of time periods (monthly data) per year
    const double w = 2*PI/M;
    double tmp = 0.0;
    
    vector<double> weig1;
    
    int i,j;
  
    for(j=0;j<Kcen+1;++j){
        weig1.push_back(par[j]); // the last one is for intercept for hx
    }
    
    double a0 = par[1+Kcen];
    double a1 = par[2+Kcen];
    double a2 = par[3+Kcen];
    double a3 = par[4+Kcen];
    
    int dd = dat.nrow();
    double sumj = 0.0;
    double pvec = 0.0;
    double nllk = 0.0;
    
    for(i=0;i<dd;++i){
        sumj = weig1[Kcen];
        for(j=0;j<Kcen;++j){
            sumj +=  weig1[j]*exp(-g*(pow((LON[i] - centers(j,0)),2) + pow((LAT[i] - centers(j,1)),2)));
        }
        tmp = exp(sumj + a0 + a1*tt[i] + a2*sin(w*tt[i]) + a3*cos(w*tt[i]));
        
        if(y[i] == 1){
            pvec = tmp / (1 + tmp);
        }else{
            pvec = 1 / (1 + tmp);
        }
         // Rcpp::Rcout << pvec << std::endl;
         // Rcpp::Rcout << " i = " << i << " centers(j,0) " << j << " " << centers(j,0) << std::endl;
        if(!R_finite(pvec)){
            pvec = 0.0001;
        }else if(pvec > 0.9999999){
            pvec = 0.9999999;
        }else if(pvec < 0.0000001){
            pvec = 0.0000001;
        }
        nllk -= log(pvec);
    }
    return nllk;
}


// [[Rcpp::export]]
double nllk_lambda_C(NumericVector par, NumericMatrix dat, NumericMatrix centers, int Kcen, double g){
    NumericVector tt = dat(Rcpp::_, 0);
    NumericVector y = dat(Rcpp::_, 1);
    NumericVector LON = dat(Rcpp::_, 2);
    NumericVector LAT = dat(Rcpp::_, 3);
    
    const int M = 12;  // number of time periods (monthly data) per year
    const double w = 2*PI/M;
    double lam = 0.0;
    
    vector<double> weig1;
    
    int i,j;
  
    for(j=0;j<Kcen+1;++j){
        weig1.push_back(par[j]); // the last one is for intercept for hx
    }
    
    double b0 = par[1+Kcen];
    double b1 = par[2+Kcen];
    double b2 = par[3+Kcen];
    double b3 = par[4+Kcen];
    
    int dd = dat.nrow();
    double sumj = 0.0;
    double nllk = 0.0;
    double loglam = 0.0;
    
    for(i=0;i<dd;++i){
        sumj = weig1[Kcen];
        for(j=0;j<Kcen;++j){
            sumj +=  weig1[j]*exp(-g*(pow((LON[i] - centers(j,0)),2) + pow((LAT[i] - centers(j,1)),2)));
        }
        loglam = sumj + b0 + b1*tt[i] + b2*sin(w*tt[i]) + b3*cos(w*tt[i]);
        lam = exp(loglam);
        nllk -= (-lam + (y[i]-1) * loglam - log(gsl_sf_fact(y[i]-1)));
    }
    return nllk;
}

// [[Rcpp::export]]
double nllk_all_C(NumericVector par, NumericMatrix dat, NumericMatrix centers, int Kcen, double g, int nq){

    NumericVector tt = dat(Rcpp::_, 0);
    NumericVector y = dat(Rcpp::_, 1); // number of events, including 0
    NumericVector LON = dat(Rcpp::_, 2);
    NumericVector LAT = dat(Rcpp::_, 3);
    
    const int M = 12;  // number of time periods (monthly data) per year
    const double w = 2*PI/M;
    int dd = dat.nrow();
    
    double sumj_p = 0.0;
    double pvec_p = 0.0;
    double tmp_p = 0.0;
    
    double lam = 0.0;
    double sumj_lam = 0.0;
    double sumj_dep = 0.0;
    double loglam = 0.0;
    double tmp_rho = 0.0;
    double rho = 0.0;
    
    vector<double> weig1_lam;
    vector<double> weig1_p;
    vector<double> weig1_dep;
    
    vector<double> xl(nq), wl(nq);
    gauleg(nq, xl, wl);
    
    int i,j,k;
  
    for(j=0;j<Kcen+1;++j){
        weig1_p.push_back(par[j]); // the last one is for intercept for hx
    }
    
    double a0 = par[1+Kcen];
    double a1 = par[2+Kcen];
    double a2 = par[3+Kcen];
    double a3 = par[4+Kcen];
    
    for(j=5+Kcen;j<2*Kcen+6;++j){
        weig1_lam.push_back(par[j]); // the last one is for intercept for hx
    }
    
    double b0 = par[2*Kcen+6];
    double b1 = par[2*Kcen+7];
    double b2 = par[2*Kcen+8];
    double b3 = par[2*Kcen+9];
   
    for(j=2*Kcen+10;j<3*Kcen+11;++j){
        weig1_dep.push_back(par[j]); // the last one is for intercept for hx
    }
    
    NumericMatrix dis(dd, Kcen);
    for(i=0;i<dd;++i){
        for(j=0;j<Kcen;++j){
            dis(i,j) = pow((LON[i] - centers(j,0)),2) + pow((LAT[i] - centers(j,1)),2);        
        }
    }

    double logintg_i = 0.0;
    double logintg_k = 0.0;
    double intg = 0.0;
    double debug1, debug2, debug3, debug4;
    
    for(k=0;k<nq;++k){
        logintg_k = 0.0;
        for(i=0;i<dd;++i){
            logintg_i = 0.0;
            // p-process
            sumj_p = weig1_p[Kcen];
            for(j=0;j<Kcen;++j){
                sumj_p +=  weig1_p[j]*exp(-g*(dis(i,j)));
            }
            tmp_p = exp(sumj_p + a0 + a1*tt[i] + a2*sin(w*tt[i]) + a3*cos(w*tt[i]));

            if(y[i] == 0){
                logintg_i += -log( (1 + tmp_p) );
            }else{
                
                // p(s,t)
                pvec_p = tmp_p / (1 + tmp_p);
                logintg_i += log(pvec_p);

                // lam-process
                sumj_lam = weig1_lam[Kcen];
                for(j=0;j<Kcen;++j){
                    sumj_lam +=  weig1_lam[j]*exp(-g*(dis(i,j)));
                }
                loglam = sumj_lam + b0 + b1*tt[i] + b2*sin(w*tt[i]) + b3*cos(w*tt[i]);
                lam = exp(loglam);
                
                // theta(s,t) - dependence parameters
                sumj_dep = weig1_dep[Kcen];
                for(j=0;j<Kcen;++j){
                    sumj_dep +=  weig1_dep[j]*exp(-g*(dis(i,j)));
                }
                tmp_rho = exp(2*sumj_dep);
                rho =  (tmp_rho - 1) / (tmp_rho + 1);
                
                // C_{i|v}(F(m+1)) - C_{i|v}(F(m))
                //debug1 = C2B1(FSPoi(y[i] + 1, lam), xl[k], rho);
                //debug2 = C2B1(FSPoi(y[i], lam), xl[k], rho);
                //debug3 = FSPoi(y[i] + 1, lam);
                //debug4 = FSPoi(y[i], lam);
                                
                // Rcpp::Rcout << " k | xl[k] | debug1 | debug2 | debug3 | debug4 " << k << " " << xl[k] << " " << debug1 << " " << debug2 << " " << debug3 << " " << debug4 << std::endl;
                logintg_i += log(C2B1(FSPoi(y[i] + 1, lam), xl[k], rho) - C2B1(FSPoi(y[i], lam), xl[k], rho));
            }
            if( R_finite(logintg_i)){
                logintg_k += logintg_i;
            }            
            // Rcpp::Rcout << " i " << i << " | logintg_i | logintg_k " << logintg_i << " | "  << logintg_k << std::endl;
            
        } // i for-loop
        intg += exp(logintg_k) * wl[k];
        // Rcpp::Rcout << " k " << k << " | intg " << intg << " " << std::endl;
        
    }// k for-loop
    return intg;
}