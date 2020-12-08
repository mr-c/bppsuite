//
// File: bppMixedLikelihoods.cpp
// Created by: Laurent Guéguen
// Created on: lundi 12 novembre 2012, à 07h 02
//

/*
   Copyright or © or Copr. Bio++ Development Team

   This software is a computer program whose purpose is to estimate
   phylogenies and evolutionary parameters from a dataset according to
   the maximum likelihood principle.

   This software is governed by the CeCILL  license under French law and
   abiding by the rules of distribution of free software.  You can  use,
   modify and/ or redistribute the software under the terms of the CeCILL
   license as circulated by CEA, CNRS and INRIA at the following URL
   "http://www.cecill.info".

   As a counterpart to the access to the source code and  rights to copy,
   modify and redistribute granted by the license, users are provided only
   with a limited warranty  and the software's author,  the holder of the
   economic rights,  and the successive licensors  have only  limited
   liability.

   In this respect, the user's attention is drawn to the risks associated
   with loading,  using,  modifying and/or developing or reproducing the
   software by the user in light of its specific status of free software,
   that may mean  that it is complicated to manipulate,  and  that  also
   therefore means  that it is reserved for developers  and  experienced
   professionals having in-depth computer knowledge. Users are therefore
   encouraged to load and test the software's suitability as regards their
   requirements in conditions enabling the security of their systems and/or
   data to be ensured and,  more generally, to use and operate it in the
   same conditions as regards security.

   The fact that you are presently reading this means that you have had
   knowledge of the CeCILL license and that you accept its terms.
 */

// From the STL:
#include <iostream>
#include <iomanip>

using namespace std;

// From bpp-core:
#include <Bpp/Version.h>
#include <Bpp/App/BppApplication.h>
#include <Bpp/App/ApplicationTools.h>
#include <Bpp/App/NumCalcApplicationTools.h>
#include <Bpp/Io/FileTools.h>
#include <Bpp/Text/TextTools.h>
#include <Bpp/Numeric/Prob/DiscreteDistribution.h>
#include <Bpp/Numeric/Prob/ConstantDistribution.h>
#include <Bpp/Numeric/DataTable.h>
#include <Bpp/Numeric/Matrix/MatrixTools.h>
#include <Bpp/Numeric/VectorTools.h>
#include <Bpp/Numeric/AutoParameter.h>
#include <Bpp/App/NumCalcApplicationTools.h>

// From bpp-seq:
#include <Bpp/Seq/SiteTools.h>
#include <Bpp/Seq/Alphabet/Alphabet.h>
#include <Bpp/Seq/Container/VectorSiteContainer.h>
#include <Bpp/Seq/Container/SequenceContainerTools.h>
#include <Bpp/Seq/Container/SiteContainerTools.h>
#include <Bpp/Seq/App/SequenceApplicationTools.h>

// From PhylLib:
#include <Bpp/Phyl/Tree/Tree.h>
#include <Bpp/Phyl/Tree/TreeTemplate.h>
#include <Bpp/Phyl/PatternTools.h>
#include <Bpp/Phyl/App/PhylogeneticsApplicationTools.h>
#include <Bpp/Phyl/OptimizationTools.h>
#include <Bpp/Phyl/Model/SubstitutionModelSetTools.h>
#include <Bpp/Phyl/Model/AbstractBiblioMixedTransitionModel.h>
#include <Bpp/Phyl/Model/MixedTransitionModel.h>
#include <Bpp/Phyl/Model/MixtureOfATransitionModel.h>
#include <Bpp/Phyl/Model/MixtureOfTransitionModels.h>
#include <Bpp/Phyl/Model/RateDistribution/ConstantRateDistribution.h>
#include <Bpp/Phyl/Io/Newick.h>

#include "bppTools.h"

using namespace bpp;

/******************************************************************************/

int main(int args, char** argv)
{
  cout << "******************************************************************" << endl;
  cout << "*     Bio++ Computation of site likelihoods inside mixed models  *" << endl;
  cout << "*                        Version " << BPP_VERSION << ".                          *" << endl;
  cout << "* Author: L. Guéguen                       Last Modif.: " << BPP_REL_DATE << " *" << endl;
  cout << "******************************************************************" << endl;
  cout << endl;

  if (args == 1)
  {
    bppTools::help("bppmixedlikelihoods");
    return 0;
  }

  try
  {
    BppApplication bppmixedlikelihoods(args, argv, "BppMixedLikelihoods");
    bppmixedlikelihoods.startTimer();

    Context context;
    
    ///// Alphabet

    unique_ptr<Alphabet> alphabet(bppTools::getAlphabet(bppmixedlikelihoods.getParams()));

    /// GeneticCode
    
    unique_ptr<GeneticCode> gCode(bppTools::getGeneticCode(bppmixedlikelihoods.getParams(), alphabet.get()));

    // get the data

    map<size_t, AlignedValuesContainer*> mSites = bppTools::getAlignmentsMap(bppmixedlikelihoods.getParams(), alphabet.get());

    if (mSites.size()!=1)
      throw Exception("Only one alignment possible.");

    auto sites = mSites.begin()->second;
        
    /////// Get the map of initial trees

    map<string, string> unparsedparams;

    auto mpTree = bppTools::getPhyloTreesMap(bppmixedlikelihoods.getParams(), mSites, unparsedparams);

    /////////////////
    // Computing stuff

    
    unique_ptr<SubstitutionProcessCollection> SPC(bppTools::getCollection(bppmixedlikelihoods.getParams(), alphabet.get(), gCode.get(), mSites, mpTree, unparsedparams));
    
    map<size_t, SequenceEvolution*> mSeqEvol = bppTools::getProcesses(bppmixedlikelihoods.getParams(), *SPC, unparsedparams);

    auto mPhyl(bppTools::getPhyloLikelihoods(bppmixedlikelihoods.getParams(), context, mSeqEvol, *SPC, mSites));

    if (!mPhyl->hasPhyloLikelihood(0))
      throw Exception("Missing phyloLikelihoods.");

    AlignedPhyloLikelihood* tl=dynamic_cast<AlignedPhyloLikelihood*>((*mPhyl)[0]);

    if (tl==0)
      throw Exception("Only possible on aligned phyloLikelihood.");
        
    //Check initial likelihood:
      
    bppTools::fixLikelihood(bppmixedlikelihoods.getParams(), alphabet.get(), gCode.get(), tl);

    // /////////////////////////////////////////////
    // Getting likelihoods per submodel

    string outputFile;
    outputFile = ApplicationTools::getAFilePath("output.likelihoods.file", bppmixedlikelihoods.getParams(), true, false);
    ApplicationTools::displayResult("Output file for likelihoods", outputFile);
    ofstream out(outputFile.c_str(), ios::out);

    size_t nSites = sites->getNumberOfSites();

    // Look for submodel
    
    size_t modNum(0);
    string realparname(""), parname("");
    
    vector<size_t> vNumMix; // numbers of mixed models in SPC
    auto nMod = SPC->getModelNumbers();
    for (auto n:nMod)
    {
      if (dynamic_cast<const MixedTransitionModel*>(SPC->getModel(n).get())!=NULL)
        vNumMix.push_back(n);
    }
    
    // Set model number
    if (vNumMix.size()==0)
      throw Exception("No mixture models found.");

    if (vNumMix.size()==1)
      modNum=vNumMix[0];
    else
    {
      modNum = ApplicationTools::getParameter<size_t>("likelihoods.model_number", bppmixedlikelihoods.getParams(), 0, "", true, true);

      if (!modNum)
      {
        realparname = ApplicationTools::getStringParameter("likelihoods.parameter_name", bppmixedlikelihoods.getParams(), "", "", true, false);

        if (realparname=="")
          throw Exception("Missing parameter name.");
        parname = realparname;
        
        size_t posund = realparname.find_last_of("_");
        if (posund!=string::npos)
        {
          try {
            modNum = (size_t)TextTools::toInt(realparname.substr(posund+1));
            parname = realparname.substr(0,posund);
          }
          catch (exception& e)
          { }
        }

        if (modNum==0)
          // next possibility is to get the only model where this parameter exists iff MixtureOfATransitionModel
        {
          for (auto n:vNumMix)
          {
            auto mod = SPC->getModel(n);
            if (mod->hasParameter(mod->getParameterNameWithoutNamespace(parname)))
            {
              // Check it is a MixtureOfATransitionModel 
              bool modok=(dynamic_cast<const MixtureOfATransitionModel*>(mod.get())!=NULL);
              if (!modok)
              {
                auto ptmp = dynamic_cast<const AbstractBiblioMixedTransitionModel*>(mod.get());
                if (ptmp) 
                  modok= dynamic_cast<const MixtureOfATransitionModel*>(&ptmp->getMixedModel());
              }
              
              if (!modok)
                continue;
              
              if (modNum!=0)
                throw Exception("Ambiguous model numbers for parameter " + parname + ":" + TextTools::toString(modNum) + " & " + TextTools::toString(n));
              else
                modNum=n;
            }
          }
          if (modNum==0)
            throw Exception("Unknown parameter " + realparname);
          realparname = parname + "_" + TextTools::toString(modNum);
        }
      }
    }

    // Get the model used to compute the likelihood
    auto model = mPhyl->getCollectionNodes()->getModel(modNum);
    if (!model)
    {
      ApplicationTools::displayError("Unknown number of model " + TextTools::toString(modNum) + ".");
      exit(-1);
    }

    auto * mixmodel = dynamic_cast<const MixedTransitionModel *> (model->getTargetValue());

    if (!mixmodel)
    {
      ApplicationTools::displayError("Model " + TextTools::toString(modNum) + " is not a Mixed Model.");
      exit(-1);
    }


    // look for parameter name in MixtureOfATransitionModel
    
    const AbstractBiblioMixedTransitionModel* pAbmtm(0);
    
     auto pMatm = (pAbmtm = dynamic_cast<const AbstractBiblioMixedTransitionModel*>(mixmodel))?
      dynamic_cast<const MixtureOfATransitionModel*>(mixmodel):
      dynamic_cast<const MixtureOfATransitionModel*>(&pAbmtm->getMixedModel());
    
    if (pMatm)
    {
      if (realparname=="")
      {
        realparname = ApplicationTools::getStringParameter("likelihoods.parameter_name", bppmixedlikelihoods.getParams(), "", "", true, false);
        parname = realparname;
      }
      
      size_t posund = realparname.find_last_of("_");
      if (posund!=string::npos)
      {
        size_t modNum2(modNum);
        try {
          modNum2 = (size_t)TextTools::toInt(realparname.substr(posund+1));
          parname = realparname.substr(0,posund);
        }
        catch (exception& e)
        { }
        if (modNum!=modNum2)
          throw BadIntegerException("Mismatch between model & parameter numbers: " + TextTools::toString(modNum), (int)modNum2);
        realparname= parname + "_" + TextTools::toString(modNum);
      }
    }

    // get rid of biblio link
    if (pAbmtm) {
      if (parname!="")
      {
        parname = pAbmtm->getPmodelParName(model->getParameterNameWithoutNamespace(parname));
        // Remove distribution suffix
        auto pos = parname.rfind("_");
        if (pos!=string::npos)
          parname=parname.substr(0,pos);
      }
      mixmodel = &pAbmtm->getMixedModel();
    }

    
    //////////////////////////////////////////////////
    // Case of a MixtureOfTransitionModels

    auto pMSM = dynamic_cast<const MixtureOfTransitionModels*>(mixmodel);

    if (pMSM)
    {
      vector<string> colNames;
      colNames.push_back("Sites");
      colNames.push_back("Ll");

      size_t nummod = pMSM->getNumberOfModels();
      for (unsigned int i = 0; i < nummod; i++)
      {
        colNames.push_back("Ll_" + pMSM->getNModel(i)->getName());
      }
      for (unsigned int i = 0; i < nummod; i++)
      {
        colNames.push_back("Pr_" + pMSM->getNModel(i)->getName());
      }

      DataTable* rates = new DataTable(nSites, colNames.size());
      rates->setColumnNames(colNames);

      // output sites
      for (unsigned int i = 0; i < nSites; i++)
      {
        const CruxSymbolListSite& currentSite = sites->getSymbolListSite(i);
        int currentSitePosition = currentSite.getPosition();
        (*rates)(i, "Sites") = string("[" + TextTools::toString(currentSitePosition) + "]");
      }

      // Likelihoods
      Vdouble vl = tl->getLikelihoodPerSite();
      for (unsigned int j = 0; j < nSites; j++)
        (*rates)(j, "Ll") = TextTools::toString(std::log(vl[j]));

      
      VVdouble vvd;
      Vdouble vprob = pMSM->getProbabilities();

      for (unsigned int i = 0; i < nummod; i++)
      {
        string modname = pMSM->getNModel(i)->getName();

        auto func= [nummod, i](BranchModel* bmodel){
          auto pAbmtm2 = dynamic_cast<AbstractBiblioMixedTransitionModel*>(bmodel);
          if (pAbmtm2) 
          {
            for (unsigned int j = 0; j < nummod; ++j)
              pAbmtm2->setNProbability(j, (j == i) ? 1 : 0);
          }
          else
          {
            auto pMSM2 = dynamic_cast<MixtureOfTransitionModels*>(bmodel);
            if (!pMSM2)
              throw Exception("Not mixed model " + bmodel->getName());
            
            for (unsigned int j = 0; j < nummod; j++)
              pMSM2->setNProbability(j, (j == i) ? 1 : 0);
          }
        };

        model->modify(func,false);
        
        tl->getValue();
        Vdouble vd = tl->getLikelihoodPerSite();

        for (unsigned int j = 0; j < nSites; j++)
        {
          (*rates)(j, "Ll_" + modname) = TextTools::toString(log(vd[j]));
        }

        vvd.push_back(vd);

        ApplicationTools::displayMessage("\n");
        ApplicationTools::displayMessage("Model " + modname + ":");
        ApplicationTools::displayResult("Log likelihood", TextTools::toString(tl->getValue(), 15));
        ApplicationTools::displayResult("Probability", TextTools::toString(vprob[i], 15));
      }

      for (size_t j = 0; j < nSites; j++)
      {
        Vdouble vd;
        for (size_t i = 0; i < nummod; i++)
          vd.push_back(std::log(vprob[i] * vvd[i][j]));
          
        VectorTools::logNorm(vd);
        for (size_t i = 0; i < nummod; i++)
          (*rates)(j, "Pr_" + pMSM->getNModel(i)->getName()) = TextTools::toString(std::exp(vd[i]));
      }

      DataTable::write(*rates, out, "\t");
    }

    //////////////////////////////////////////////////
    // Case of a MixtureOfASubstitutionModel

    else
    {
      pMatm = dynamic_cast<const MixtureOfATransitionModel*>(mixmodel);

      if (pMatm != NULL)
      {
        size_t nummod = pMatm->getNumberOfModels();
        if (realparname == "")
        {
          ParameterList pl=pMatm->getParameters();
          for (size_t i2 = 0; i2 < pl.size(); i2++)
          {
            string pl2n = pl[i2].getName();

            if (dynamic_cast<const ConstantDistribution*>(pMatm->getDistribution(pl2n))==NULL)
            {
              parname=pl2n;

              if (parname.size()>0 && pMatm->getDistribution(parname)==NULL)
                parname=parname.substr(0,parname.rfind("_"));

              if (parname.size()>0){
                break;
              }
            }
          }
          if (parname!="")
            realparname = parname + "_" + TextTools::toString(modNum);
        }

        if (parname == "")
        {
          ApplicationTools::displayError("Argument likelihoods.parameter_name is required.");
          exit(-1);
        }
        else
          ApplicationTools::displayResult("likelihoods.parameter_name", realparname);

        vector< Vuint > vvnmod;
        size_t i2 = 0;
        while (i2 < nummod)
        {
          string par2 = parname + "_" + TextTools::toString(i2 + 1);
          Vuint vnmod = pMatm->getSubmodelNumbers(par2);
          if (vnmod.size() == 0)
            break;
          vvnmod.push_back(vnmod);
          i2++;
        }

        size_t nbcl = vvnmod.size();
        if (nbcl<=1)
          throw Exception("Parameter " + realparname + " is not mixed.");
        
        Vdouble vprob = pMatm->getProbabilities();

        // vectors of sets of probabilities for each value of parname
        vector<vector<double> > vvprob;

        // vector of total probabilities for each value of parname
        vector<double> vsprob;
        
        for (size_t i = 0; i < nbcl; i++)
        {
          vector<double> vprob2;
          for (size_t j = 0; j < vvnmod[i].size(); j++)
          {
            vprob2.push_back(vprob[static_cast<size_t>(vvnmod[i][j])]);
          }

          vvprob.push_back(vprob2);
          vsprob.push_back(VectorTools::sum(vvprob[i]));
        }

        vector<string> colNames;
        colNames.push_back("Sites");
        colNames.push_back("Ll");
        
        Vdouble dval;
        for (size_t i = 0; i < nbcl; i++)
        {
          const TransitionModel* pSM = pMatm->getNModel(static_cast<size_t>(vvnmod[i][0]));
          double valPar = pSM->getParameterValue(pSM->getParameterNameWithoutNamespace(parname));
          dval.push_back(valPar);
          colNames.push_back("Ll_" + realparname + "=" + TextTools::toString(valPar));
        }
        for (size_t i = 0; i < nbcl; i++)
          colNames.push_back("Pr_" + realparname + "=" + TextTools::toString(dval[i]));

        colNames.push_back("mean");

        DataTable* rates = new DataTable(nSites, colNames.size());
        rates->setColumnNames(colNames);

        // output sites
        for (size_t i = 0; i < nSites; i++)
        {
          const CruxSymbolListSite& currentSite = sites->getSymbolListSite(i);
          int currentSitePosition = currentSite.getPosition();
          (*rates)(i,"Sites")=TextTools::toString(currentSitePosition);
        }

        // Likelihoods
        Vdouble vl = tl->getLikelihoodPerSite();
        for (unsigned int j = 0; j < nSites; j++)
          (*rates)(j, "Ll") = TextTools::toString(std::log(vl[j]));


        VVdouble vvd;
        vector<double> vRates = pMatm->getVRates();

        for (size_t i = 0; i < nbcl; ++i)
        {
          string par2 = realparname + "_" + TextTools::toString(i + 1);
          
          auto func= [nummod, i, &vvprob, &vvnmod, &vsprob](BranchModel* cmodel){
            auto pAbmtm2 = dynamic_cast<AbstractBiblioMixedTransitionModel*>(cmodel);
            if (pAbmtm2) 
            {
              for (unsigned int j = 0; j < nummod; ++j)
                pAbmtm2->setNProbability(j, 0);

              for (size_t j = 0; j < vvprob[i].size(); ++j)
                pAbmtm2->setNProbability(static_cast<size_t>(vvnmod[i][j]), vvprob[i][j] / vsprob[i]);
            }
            else{
              auto pMatm2 = dynamic_cast<MixtureOfATransitionModel*>(cmodel);
              if (!pMatm2)
                throw Exception("Not mixed model " + cmodel->getName());
              for (unsigned int j = 0; j < nummod; ++j)
                pMatm2->setNProbability(j, 0);

              for (size_t j = 0; j < vvprob[i].size(); ++j)
                pMatm2->setNProbability(static_cast<size_t>(vvnmod[i][j]), vvprob[i][j] / vsprob[i]);
            }
          };

          model->modify(func,false);

          tl->getValue();
          Vdouble vd = tl->getLikelihoodPerSite();

          for (unsigned int j = 0; j < nSites; j++)
            (*rates)(j, i + 2) = TextTools::toString(std::log(vd[j]));

          vvd.push_back(vd);

          ApplicationTools::displayMessage("\n");
          ApplicationTools::displayMessage("Parameter " + par2 + "=" + TextTools::toString(dval[i]) + " with rate=" + TextTools::toString(vRates[i]));

          ApplicationTools::displayResult("Log likelihood", TextTools::toString(tl->getValue(), 15));
          ApplicationTools::displayResult("Probability", TextTools::toString(vsprob[i], 15));
        }

        for (size_t j = 0; j < nSites; j++)
        {
          Vdouble vd;
          for (size_t i = 0; i < nbcl; i++)
            vd.push_back(std::log(vsprob[i] * vvd[i][j]));
          
          VectorTools::logNorm(vd);
          for (size_t i = 0; i < nbcl; i++)
            (*rates)(j,nbcl + i + 2) = TextTools::toString(std::exp(vd[i]));
          (*rates)(j, 2 * nbcl + 2) = TextTools::toString(VectorTools::sumExp(vd, dval));
        }

        DataTable::write(*rates, out, "\t");
      }
    }

    ApplicationTools::displayMessage("\n");
    bppmixedlikelihoods.done();
  }

  catch (exception& e)
  {
    cout << e.what() << endl;
    return 1;
  }

  return 0;
}

