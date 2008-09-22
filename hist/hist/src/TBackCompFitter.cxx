#include "TROOT.h"
#include "TBackCompFitter.h"

#include "TBackCompFcnAdapter.h"

#include "TMethodCall.h"
#include "TInterpreter.h"

#include "Math/Util.h"

#include <iostream>
#include <cassert>

//needed by GetCondifenceLevel
#include "Math/IParamFunction.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TGraph2DErrors.h"
#include "TMultiGraph.h"
#include "Fit/BinData.h"
#include "HFitInterface.h"

//#define DEBUG 1


//______________________________________________________________________________
/**
   Backward comparible implementation of TVirtualFitter using the the class ROOT::Fit::Fitter
*/



ClassImp(TBackCompFitter);



TBackCompFitter::TBackCompFitter( ) 
{
   // Constructur needed by TVirtualFitter interface. Same behavior as default constructor.
   // initialize setting name and the global pointer
   SetName("BCFitter");
}

TBackCompFitter::TBackCompFitter(ROOT::Fit::Fitter & fitter) {
   // constructor used after having fit using directly ROOT::Fit::Fitter
   // will create a dummy fitter copying configuration and parameter settings
   fFitter = fitter; 
   SetName("LastFitter");
}



TBackCompFitter::~TBackCompFitter() { 
   
}

Double_t TBackCompFitter::Chisquare(Int_t npar, Double_t *params) const {
   // do chisquare calculations in case of likelihood fits 
   // do evaluation a the minimum only 
   const std::vector<double> & minpar = fFitter.Result().Parameters(); 
   assert (npar == (int) minpar.size() ); 
   double diff = 0; 
   double s = 0; 
   for (int i =0; i < npar; ++i) { 
      diff += std::abs( params[i] - minpar[i] );  
      s += minpar[i]; 
   }

   if (diff > s * 1.E-12 ) Warning("Chisquare","given parameter values are not at minimum - chi2 at minimum is returned"); 
   return fFitter.Result().Chi2(); 
}

void TBackCompFitter::Clear(Option_t*) {
   // clear resources for consecutive fits

   // need to do something here ??? to be seen
   
   
}





Int_t TBackCompFitter::ExecuteCommand(const char *command, Double_t *args, Int_t nargs){	
   // execute the command (Fortran Minuit compatible interface)
   
#ifdef DEBUG
   std::cout<<"Execute command= "<<command<<std::endl;
#endif

   int nfcn = GetMaxIterations();  
   double edmval = GetPrecision(); 

   // set also number of parameters in obj function
   DoSetDimension(); 

   // MIGRAD 
   if (strncmp(command,"MIG",3) == 0 || strncmp(command,"mig",3)  == 0) {
      if (nargs > 0) nfcn = int ( args[0] );
      if (nargs > 1) edmval = args[1];
      if (!fObjFunc) { 
         Error("ExecuteCommand","FCN must set before executing this command"); 
         return -1; 
      }

      fFitter.Config().SetMinimizer(GetDefaultFitter(), "Migrad");
      bool ret = fFitter.FitFCN(*fObjFunc); 
      return  (ret) ? 0 : -1;
      
      
      
   } 
   //Minimize
   if (strncmp(command,"MINI",4) == 0 || strncmp(command,"mini",4)  == 0) {

      if (nargs > 0) nfcn = int ( args[0] );
      if (nargs > 1) edmval = args[1];

      fFitter.Config().SetMinimizer(GetDefaultFitter(), "Minimize");
      if (!fObjFunc) { 
         Error("ExecuteCommand","FCN must set before executing this command"); 
         return -1; 
      }
      bool ret = fFitter.FitFCN(*fObjFunc); 
      return  (ret) ? 0 : -1;
   }
   //Simplex
   if (strncmp(command,"SIM",3) == 0 || strncmp(command,"sim",3)  == 0) {
      
      if (nargs > 0) nfcn = int ( args[0] );
      if (nargs > 1) edmval = args[1];
      if (!fObjFunc) { 
         Error("ExecuteCommand","FCN must set before executing this command"); 
         return -1; 
      }

      fFitter.Config().SetMinimizer(GetDefaultFitter(), "Simplex");
      bool ret = fFitter.FitFCN(*fObjFunc); 
      return  (ret) ? 0 : -1;
   }
   //SCan
   if (strncmp(command,"SCA",3) == 0 || strncmp(command,"sca",3)  == 0) {
      
      if (nargs > 0) nfcn = int ( args[0] );
      if (nargs > 1) edmval = args[1];
      if (!fObjFunc) { 
         Error("ExecuteCommand","FCN must set before executing this command"); 
         return -1; 
      }

      fFitter.Config().SetMinimizer(GetDefaultFitter(), "Scan");
      bool ret = fFitter.FitFCN(*fObjFunc); 
      return  (ret) ? 0 : -1;
   }
   // MINOS 
   else if (strncmp(command,"MINO",4) == 0 || strncmp(command,"mino",4)  == 0) {

      if (fFitter.Config().MinosErrors() ) return 0; 

      if (!fObjFunc) { 
         Error("ExecuteCommand","FCN must set before executing this command"); 
         return -1; 
      }
      // do only MINOS. need access to minimizer. For the moment re-run fitting with minos options 
      fFitter.Config().SetMinosErrors(true);
      // set new parameter values

      fFitter.Config().SetMinimizer(GetDefaultFitter(), "Migrad"); // redo -minimization with Minos
      bool ret = fFitter.FitFCN(*fObjFunc); 
      return  (ret) ? 0 : -1;

   } 
   //HESSE
   else if (strncmp(command,"HES",3) == 0 || strncmp(command,"hes",3)  == 0) {

      if (fFitter.Config().ParabErrors() ) return 0; 

      if (!fObjFunc) { 
         Error("ExecuteCommand","FCN must set before executing this command"); 
         return -1; 
      }

      // do only HESSE. need access to minimizer. For the moment re-run fitting with hesse options 
      fFitter.Config().SetParabErrors(true);
      fFitter.Config().SetMinimizer(GetDefaultFitter(), "Migrad"); // redo -minimization with Minos
      bool ret = fFitter.FitFCN(*fObjFunc); 
      return  (ret) ? 0 : -1;
   } 
   
   // FIX 
   else if (strncmp(command,"FIX",3) == 0 || strncmp(command,"fix",3)  == 0) {
      for(int i = 0; i < nargs; i++) {
         FixParameter(int(args[i])-1);
      }
      return 0;
   } 
   // SET LIMIT (upper and lower)
   else if (strncmp(command,"SET LIM",7) == 0 || strncmp(command,"set lim",7)  == 0) {
      if (nargs < 3) { 
         Error("ExecuteCommand","Invalid parameters given in SET LIMIT");
         return -1; 
      }
      int ipar = int(args[0]);
      if (!ValidParameterIndex(ipar) )  return -1;   
      double low = args[1];
      double up = args[2];
      fFitter.Config().ParSettings(ipar).SetLimits(low,up);
      return 0; 
   } 
   // SET PRINT
   else if (strncmp(command,"SET PRINT",9) == 0 || strncmp(command,"set print",9)  == 0) {
      if (nargs < 1) return -1;  
      fFitter.Config().MinimizerOptions().SetPrintLevel(int(args[0]) );
      return 0; 
   } 
   // SET ERR
   else if (strncmp(command,"SET Err",7) == 0 || strncmp(command,"set err",7)  == 0) {
      if (nargs < 1) return -1;  
      fFitter.Config().MinimizerOptions().SetPrintLevel(int( args[0]) );
      return 0; 
   } 
   // SET STRATEGY
   else if (strncmp(command,"SET STR",7) == 0 || strncmp(command,"set str",7)  == 0) {
      if (nargs < 1) return -1;  
      fFitter.Config().MinimizerOptions().SetStrategy(int(args[0]) );
      return 0; 
   } 
   //SET GRAD (not impl.) 
   else if (strncmp(command,"SET GRA",7) == 0 || strncmp(command,"set gra",7)  == 0) {
      //     not yet available
      //     fGradient = true;
      return -1;
   } 
   // CALL FCN
   else if (strncmp(command,"CALL FCN",8) == 0 || strncmp(command,"call fcn",8)  == 0) {
      //     call fcn function (global pointer to free function)

      if (nargs < 1 || fFCN == 0 ) return -1;
      int npar = fObjFunc->NDim();
      // use values in fit result if existing  otherwise in ParameterSettings
      std::vector<double> params(npar); 
      for (int i = 0; i < npar; ++i) 
         params[i] = GetParameter(i); 

      double fval = 0;
      (*fFCN)(npar, 0, fval, &params[0],int(args[0]) ) ;
      return 0; 
   } 
   else {
      // other commands passed 
      Error("ExecuteCommand","Invalid command given");
      return -1;
   }
   
   
}

bool TBackCompFitter::ValidParameterIndex(int ipar) const  { 
   int nps  = fFitter.Config().ParamsSettings().size(); 
   if (ipar  < 0 || ipar >= nps ) { 
      std::string msg = ROOT::Math::Util::ToString(ipar) + " is an invalid Parameter index";
      Error("ValidParameterIndex",msg.c_str());
      return false;
   } 
   return true; 
}

void TBackCompFitter::FixParameter(Int_t ipar) {
   // fix the paramter
   //   std::cout<<"FixParameter"<<std::endl;
   if (ValidParameterIndex(ipar) )    
      fFitter.Config().ParSettings(ipar).Fix();
}



void TBackCompFitter::GetConfidenceIntervals(Int_t n, Int_t ndim, const Double_t *x, Double_t *ci, Double_t cl)
{
//Computes point-by-point confidence intervals for the fitted function
//Parameters:
//n - number of points
//ndim - dimensions of points
//x - points, at which to compute the intervals, for ndim > 1 
//    should be in order: (x0,y0, x1, y1, ... xn, yn)
//ci - computed intervals are returned in this array
//cl - confidence level, default=0.95
//NOTE, that the intervals are approximate for nonlinear(in parameters) models
   
   if (!fFitter.Result().IsValid()) { 
      Error("GetConfidenceIntervals","Cannot compute confidence intervals with an invalide fit result");
      return; 
   }
   
   fFitter.Result().GetConfidenceIntervals(n,ndim,1,x,ci,cl);         
}

void TBackCompFitter::GetConfidenceIntervals(TObject *obj, Double_t cl)
{
//Computes confidence intervals at level cl. Default is 0.95
//The TObject parameter can be a TGraphErrors, a TGraph2DErrors or a TH1,2,3.
//For Graphs, confidence intervals are computed for each point,
//the value of the graph at that point is set to the function value at that
//point, and the graph y-errors (or z-errors) are set to the value of
//the confidence interval at that point.
//For Histograms, confidence intervals are computed for each bin center
//The bin content of this bin is then set to the function value at the bin
//center, and the bin error is set to the confidence interval value.
//NOTE: confidence intervals are approximate for nonlinear models!
//
//Allowed combinations:
//Fitted object               Passed object
//TGraph                      TGraphErrors, TH1
//TGraphErrors, AsymmErrors   TGraphErrors, TH1
//TH1                         TGraphErrors, TH1
//TGraph2D                    TGraph2DErrors, TH2
//TGraph2DErrors              TGraph2DErrors, TH2
//TH2                         TGraph2DErrors, TH2
//TH3                         TH3

   if (!fFitter.Result().IsValid() ) { 
      Error("GetConfidenceIntervals","Cannot compute confidence intervals with an invalide fit result");
      return; 
   }

   // get data dimension from fit object
   int datadim = 1; 
   TObject * fitobj = GetObjectFit(); 
   if (!fitobj) { 
      Error("GetConfidenceIntervals","Cannot compute confidence intervals without a fitting object");
      return; 
   }

   if (fitobj->InheritsFrom(TGraph2D::Class())) datadim = 2; 
   if (fitobj->InheritsFrom(TH1::Class())) { 
      TH1 * h1 = dynamic_cast<TH1*>(fitobj); 
      assert(h1 != 0); 
      datadim = h1->GetDimension(); 
   } 

   if (datadim == 1) { 
      if (!obj->InheritsFrom(TGraphErrors::Class()) && !obj->InheritsFrom(TH1::Class() ) )  {
         Error("GetConfidenceIntervals", "Invalid object passed for storing confidence level data, must be a TGraphErrors or a TH1");
         return; 
      }
   } 
   if (datadim == 2) { 
      if (!obj->InheritsFrom(TGraph2DErrors::Class()) && !obj->InheritsFrom(TH2::Class() ) )  {
         Error("GetConfidenceIntervals", "Invalid object passed for storing confidence level data, must be a TGraph2DErrors or a TH2");
         return; 
      }
   }
   if (datadim == 3) { 
      if (!obj->InheritsFrom(TH3::Class() ) )  {
         Error("GetConfidenceIntervals", "Invalid object passed for storing confidence level data, must be a TH3");
         return; 
      }
   }

   // fill bin data (for the moment use all ranges) 
   ROOT::Fit::BinData data; 
   // call appropriate function according to type of object
   if (fitobj->InheritsFrom(TGraph::Class()) ) 
      ROOT::Fit::FillData(data, dynamic_cast<TGraph *>(fitobj) ); 
   else if (fitobj->InheritsFrom(TGraph2D::Class()) ) 
      ROOT::Fit::FillData(data, dynamic_cast<TGraph2D *>(fitobj) ); 
   else if (fitobj->InheritsFrom(TMultiGraph::Class()) ) 
      ROOT::Fit::FillData(data, dynamic_cast<TMultiGraph *>(fitobj) ); 
   else if (fitobj->InheritsFrom(TH1::Class()) ) 
      ROOT::Fit::FillData(data, dynamic_cast<TH1 *>(fitobj) ); 
   

   unsigned int n = data.Size(); 
   std::vector<double> ci( n ); 

   fFitter.Result().GetConfidenceIntervals(data,&ci[0],cl);         

   

   const ROOT::Math::IParamMultiFunction * func =  fFitter.Result().FittedFunction(); 
   assert(func != 0); 

   // fill now the object with cl data
   for (unsigned int i = 0; i < n; ++i) {
      const double * x = data.Coords(i); 
      double y = (*func)( x ); // function is evaluated using its  parameters

      if (obj->InheritsFrom(TGraphErrors::Class()) ) { 
         TGraphErrors * gr = dynamic_cast<TGraphErrors *> (obj); 
         assert(gr != 0); 
         gr->SetPoint(i, *x, y); 
         gr->SetPointError(i, 0, ci[i]); 
      }
      if (obj->InheritsFrom(TGraph2DErrors::Class()) ) { 
         TGraph2DErrors * gr = dynamic_cast<TGraph2DErrors *> (obj); 
         assert(gr != 0); 
         gr->SetPoint(i, x[0], x[1], y); 
         gr->SetPointError(i, 0, 0, ci[i]); 
      }
      if (obj->InheritsFrom(TH1::Class()) ) { 
         TH1 * h1 = dynamic_cast<TH1 *> (obj); 
         assert(h1 != 0); 
         int ibin = 0; 
         if (datadim == 1) ibin = h1->FindBin(*x); 
         if (datadim == 2) ibin = h1->FindBin(x[0],x[1]); 
         if (datadim == 3) ibin = h1->FindBin(x[0],x[1],x[2]); 
         h1->SetBinContent(ibin, y); 
         h1->SetBinError(ibin, ci[i]); 
      }
   }

}

Double_t* TBackCompFitter::GetCovarianceMatrix() const {
   // get the error matrix in a pointer to a NxN array.  
   // excluding the fixed parameters 

   unsigned int nfreepar =   GetNumberFreeParameters();
   unsigned int ntotpar =   GetNumberTotalParameters();
   
   if (!fFitter.Result().IsValid() ) { 
      Warning("GetCovarianceMatrix","Invalid fit result");
      return 0; 
   }

   if (fCovar.size() !=  nfreepar*nfreepar ) 
      fCovar.resize(nfreepar*nfreepar);

   unsigned int l = 0; 
   for (unsigned int i = 0; i < ntotpar; ++i) { 
      if (fFitter.Config().ParSettings(i).IsFixed() ) continue;
      unsigned int m = 0; 
      for (unsigned int j = 0; j < ntotpar; ++j) {
         if (fFitter.Config().ParSettings(j).IsFixed() ) continue;
         unsigned int index = nfreepar*l + m;
         assert(index < fCovar.size() );
         fCovar[index] = fFitter.Result().CovMatrix(i,j);
         m++;
      }
      l++;
   }
   return &(fCovar.front());
}

Double_t TBackCompFitter::GetCovarianceMatrixElement(Int_t i, Int_t j) const {
   // get error matrix element

   unsigned int np2 = fCovar.size();
   unsigned int npar = GetNumberFreeParameters(); 
   if ( np2 == 0 || np2 != npar *npar )   GetCovarianceMatrix(); 
   return fCovar[i*npar + j];  
}


Int_t TBackCompFitter::GetErrors(Int_t ipar,Double_t &eplus, Double_t &eminus, Double_t &eparab, Double_t &globcc) const {
   // get fit errors 

   if (!ValidParameterIndex(ipar) )   return -1; 
   
   const ROOT::Fit::FitResult & result = fFitter.Result(); 
   if (!result.IsValid() ) { 
      Warning("GetErrors","Invalid fit result");
      return -1; 
   }

   eparab = result.Error(ipar); 
   eplus = result.UpperError(ipar); 
   eminus = result.LowerError(ipar); 
   globcc = result.GlobalCC(ipar); 
   return 0;
}

Int_t TBackCompFitter::GetNumberTotalParameters() const {
   // number of total parameters 
   return fFitter.Result().NTotalParameters();  
}
Int_t TBackCompFitter::GetNumberFreeParameters() const {
   // number of variable parameters
   return fFitter.Result().NFreeParameters();  
}


Double_t TBackCompFitter::GetParError(Int_t ipar) const {
   // parameter error
   if (fFitter.Result().IsEmpty() ) {
      if (ValidParameterIndex(ipar) )  return  fFitter.Config().ParSettings(ipar).StepSize();
      else return 0; 
   }
   return fFitter.Result().Error(ipar);  
}

Double_t TBackCompFitter::GetParameter(Int_t ipar) const {
   // parameter value
   if (fFitter.Result().IsEmpty() ) {
      if (ValidParameterIndex(ipar) )  return  fFitter.Config().ParSettings(ipar).Value();
      else return 0; 
   }
   return fFitter.Result().Value(ipar);  
}

Int_t TBackCompFitter::GetParameter(Int_t ipar,char *name,Double_t &value,Double_t &verr,Double_t &vlow, Double_t &vhigh) const {
   // get all parameter info (name, value, errors) 
   if (!ValidParameterIndex(ipar) )    {
      return -1; 
   }
   const std::string & pname = fFitter.Config().ParSettings(ipar).Name(); 
   const char * c = pname.c_str(); 
   std::copy(c,c + pname.size(),name);

   if (fFitter.Result().IsEmpty() ) { 
      value = fFitter.Config().ParSettings(ipar).Value(); 
      verr  = fFitter.Config().ParSettings(ipar).Value();  // error is step size in this case 
      vlow  = fFitter.Config().ParSettings(ipar).LowerLimit();  // vlow is lower limit in this case 
      vhigh   = fFitter.Config().ParSettings(ipar).UpperLimit();  // vlow is lower limit in this case 
      return 1; 
   }
   else { 
      value =  fFitter.Result().Value(ipar);  
      verr = fFitter.Result().Error(ipar);  
      vlow = fFitter.Result().LowerError(ipar);  
      vhigh = fFitter.Result().UpperError(ipar);  
   }
   return 0; 
}

const char *TBackCompFitter::GetParName(Int_t ipar) const {
   //   return name of parameter ipar
   if (!ValidParameterIndex(ipar) )    {
      return 0; 
   }
   return fFitter.Config().ParSettings(ipar).Name().c_str(); 
}

Int_t TBackCompFitter::GetStats(Double_t &amin, Double_t &edm, Double_t &errdef, Int_t &nvpar, Int_t &nparx) const {
   // get fit statistical information
   const ROOT::Fit::FitResult & result = fFitter.Result(); 
   amin = result.MinFcnValue(); 
   edm = result.Edm(); 
   errdef = fFitter.Config().MinimizerOptions().ErrorDef(); 
   nvpar = result.NFreeParameters();  
   nparx = result.NTotalParameters();  
   return 0;
}

Double_t TBackCompFitter::GetSumLog(Int_t) {
   //   sum of log . Un-needed
   Warning("GetSumLog","Dummy  method - returned 0"); 
   return 0.;
}


Bool_t TBackCompFitter::IsFixed(Int_t ipar) const {
   // query if parameter ipar is fixed
   if (!ValidParameterIndex(ipar) )    {
      return false; 
   }
   return fFitter.Config().ParSettings(ipar).IsFixed(); 
}


void TBackCompFitter::PrintResults(Int_t level, Double_t) const {
   // print the fit result
   if (level > 0) fFitter.Result().Print(std::cout); 
   if (level > 1)  fFitter.Result().PrintCovMatrix(std::cout);    
   // need to print minos errors and globalCC + other info
}

void TBackCompFitter::ReleaseParameter(Int_t ipar) {
   // release a fit parameter
   if (ValidParameterIndex(ipar) )    
      fFitter.Config().ParSettings(ipar).Release(); 
}



void TBackCompFitter::SetFitMethod(const char *) {
   // set fit method (i.e. chi2 or likelihood)
   // according to the method the appropriate FCN function will be created   
   Info("SetFitMethod","non supported method");
}

Int_t TBackCompFitter::SetParameter(Int_t ipar,const char *parname,Double_t value,Double_t verr,Double_t vlow, Double_t vhigh) {
   // set (add) a new fit parameter passing initial value,  step size (verr) and parametr limits
   // if vlow > vhigh the parameter is unbounded
   // if the stepsize (verr) == 0 the parameter is treated as fixed   

   std::vector<ROOT::Fit::ParameterSettings> & parlist = fFitter.Config().ParamsSettings(); 
   if ( ipar >= (int) parlist.size() ) parlist.resize(ipar); 
   ROOT::Fit::ParameterSettings ps(parname, value, verr); 
   if (verr == 0) ps.Fix(); 
   if (vlow < vhigh) ps.SetLimits(vlow, vhigh); 
   parlist[ipar] = ps; 
   return 0; 
}


void TBackCompFitter::SetFCN(void (*fcn)(Int_t &, Double_t *, Double_t &f, Double_t *, Int_t))
{
   // override setFCN to use the Adapter to Minuit2 FCN interface
   //*-*-*-*-*-*-*To set the address of the minimization function*-*-*-*-*-*-*-*
   //*-*          ===============================================
   //*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
   fFCN = fcn;
   if (fObjFunc) delete fObjFunc;
   fObjFunc = new TBackCompFcnAdapter(fFCN);
   DoSetDimension(); 
}

// need for interactive environment


// global functions needed by interpreter 


//______________________________________________________________________________
void InteractiveFCNm2(Int_t &npar, Double_t *gin, Double_t &f, Double_t *u, Int_t flag)
{
   //*-*-*-*-*-*-*Static function called when SetFCN is called in interactive mode
   //*-*          ===============================================
   
   // get method call from static instance
   TMethodCall *m  = (TVirtualFitter::GetFitter())->GetMethodCall();
   if (!m) return;
   
   Long_t args[5];
   args[0] = (Long_t)&npar;
   args[1] = (Long_t)gin;
   args[2] = (Long_t)&f;
   args[3] = (Long_t)u;
   args[4] = (Long_t)flag;
   m->SetParamPtrs(args);
   Double_t result;
   m->Execute(result);
}


//______________________________________________________________________________
void TBackCompFitter::SetFCN(void *fcn)
{
   //*-*-*-*-*-*-*To set the address of the minimization function*-*-*-*-*-*-*-*
   //*-*          ===============================================
   //     this function is called by CINT instead of the function above
   //*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
   
   if (!fcn) return;
   
   const char *funcname = gCint->Getp2f2funcname(fcn);
   if (funcname) {
      fMethodCall = new TMethodCall();
      fMethodCall->InitWithPrototype(funcname,"Int_t&,Double_t*,Double_t&,Double_t*,Int_t");
   }
   fFCN = InteractiveFCNm2;
   // set the static instance (required by InteractiveFCNm)
   TVirtualFitter::SetFitter(this); 
   
   if (fObjFunc) delete fObjFunc;
   fObjFunc = new TBackCompFcnAdapter(fFCN);
   DoSetDimension(); 
}

void TBackCompFitter::SetObjFunction(ROOT::Math::IMultiGenFunction   * fcn) { 
   // class clone a copy of the function
   if (fObjFunc) delete fObjFunc;
   fObjFunc = fcn->Clone(); 
}


void TBackCompFitter::DoSetDimension() { 
   // set dimension in objective function
   if (!fObjFunc) return; 
   TBackCompFcnAdapter * fobj = dynamic_cast<TBackCompFcnAdapter*>(fObjFunc); 
   assert(fobj != 0); 
   int ndim = fFitter.Config().ParamsSettings().size(); 
   if (ndim != 0) fobj->SetDimension(ndim); 
}
