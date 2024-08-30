// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   ITSDecodingErrorCheck.cxx
/// \author Zhen Zhang
///

#include "ITS/ITSDecodingErrorCheck.h"
#include "QualityControl/MonitorObject.h"
#include "QualityControl/Quality.h"
#include "QualityControl/QcInfoLogger.h"
#include "ITSMFTReconstruction/DecodingStat.h"
#include <DataFormatsQualityControl/FlagTypeFactory.h>

#include <fairlogger/Logger.h>
#include "Common/Utils.h"

namespace o2::quality_control_modules::its
{

Quality ITSDecodingErrorCheck::check(std::map<std::string, std::shared_ptr<MonitorObject>>* moMap)
{
  // set timer
  if (nCycle == 0) {
    start = std::chrono::high_resolution_clock::now();
    nCycle++;
  } else {
    end = std::chrono::high_resolution_clock::now();
    TIME = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
  }
  std::vector<int> vDecErrorLimits = convertToArray<int>(o2::quality_control_modules::common::getFromConfig<string>(mCustomParameters, "DecLinkErrorLimits", ""));
  if (vDecErrorLimits.size() != o2::itsmft::GBTLinkDecodingStat::NErrorsDefined) {
    ILOG(Error, Support) << "Incorrect vector with DecodingError limits, check .json" << ENDM;
    doFlatCheck = true;
  }
  

  Quality result = Quality::Null;
  for (auto& [moName, mo] : *moMap) {
    (void)moName;
    if (mo->getName() == "General/ChipErrorPlots") {
      result = Quality::Good;
      auto* h = dynamic_cast<TH1D*>(mo->getObject());
      if (h == nullptr) {
        ILOG(Error, Support) << "could not cast ChipError plots to TH1D*" << ENDM;
        continue;
      }
      if (h->GetMaximum() > 200)
        result.set(Quality::Bad);
    }

    if (((string)mo->getName()).find("General/LinkErrorPlots") != std::string::npos) {
      result = Quality::Good;

      auto* h = dynamic_cast<TH1D*>(mo->getObject());
      if (h == nullptr) {
        ILOG(Error, Support) << "could not cast LinkErrorPlots to TH1D*" << ENDM;
        continue;
      }
  
      if (nCycleLink == 0){
	LinkErrorBuffer = h->Clone();
      }

     
      TH1D* LinkErrorDiff = h->Add(LinkErrorBuffer, -1);
      
   
      if (doFlatCheck) {
	for (int iBin = 1; iBin <= h->GetNbinsX()-1; iBin++)
	  if (h->GetBinContent(i) > 200){
	    result.set(Quality::Bad);
	    break;
	  }
      } else {
        for (int iBin = 1; iBin <= h->GetNbinsX(); iBin++) {

          if (vDecErrorLimits[iBin - 1] < 0)
            continue; // skipping bin

            if (vDecErrorLimits[iBin - 1] <= LinkErrorDiffh->GetBinContent(iBin)) {
              vListErrorIdBad.push_back(iBin - 1);
              result.set(Quality::Bad);
              result.addFlag(o2::quality_control::FlagTypeFactory::Unknown(), Form("BAD: ID = %d, %s", iBin - 1, std::string(statistics.ErrNames[iBin - 1]).c_str()));
            } 
        }
      }

      nCycleLink ++;
      LinkErrorBuffer = h->Clone();
    }
  }
  return result;
}

void ITSDecodingErrorCheck::beautify(std::shared_ptr<MonitorObject> mo, Quality checkResult)
{
  std::vector<string> vPlotWithTextMessage = convertToArray<string>(o2::quality_control_modules::common::getFromConfig<string>(mCustomParameters, "plotWithTextMessage", ""));
  std::vector<string> vTextMessage = convertToArray<string>(o2::quality_control_modules::common::getFromConfig<string>(mCustomParameters, "textMessage", ""));
  std::map<string, string> ShifterInfoText;

  if ((int)vTextMessage.size() == (int)vPlotWithTextMessage.size()) {
    for (int i = 0; i < (int)vTextMessage.size(); i++) {
      ShifterInfoText[vPlotWithTextMessage[i]] = vTextMessage[i];
    }
  } else
    ILOG(Warning, Support) << "Bad list of plot with TextMessages for shifter, check .json" << ENDM;

  std::shared_ptr<TLatex> tShifterInfo = std::make_shared<TLatex>(0.005, 0.006, Form("#bf{%s}", TString(ShifterInfoText[mo->getName()]).Data()));
  tShifterInfo->SetTextSize(0.04);
  tShifterInfo->SetTextFont(43);
  tShifterInfo->SetNDC();

  TString status;
  int textColor;
  if ((((string)mo->getName()).find("General/LinkErrorPlots") != std::string::npos) || (mo->getName() == "General/ChipErrorPlots")) {
    auto* h = dynamic_cast<TH1D*>(mo->getObject());
    if (h == nullptr) {
      ILOG(Error, Support) << "could not cast LinkErrorPlots to TH1D*" << ENDM;
      return;
    }
    if (checkResult == Quality::Good) {
      status = "Quality::GOOD";
      textColor = kGreen;
    } else {

      if (checkResult == Quality::Bad) {
        status = "Quality::BAD (call expert)";
        for (int id = 0; id < vListErrorIdBad.size(); id++) {
          int currentError = vListErrorIdBad[id];
          tInfo = std::make_shared<TLatex>(0.12, 0.835 - 0.04 * (id + 1), Form("BAD: ID = %d, %s", currentError, std::string(statistics.ErrNames[currentError]).c_str()));
          tInfo->SetTextColor(kRed + 2);
          tInfo->SetTextSize(0.04);
          tInfo->SetTextFont(43);
          tInfo->SetNDC();
          h->GetListOfFunctions()->Add(tInfo->Clone());
        }
        textColor = kRed + 2;
      }
    }
    tInfo = std::make_shared<TLatex>(0.05, 0.95, Form("#bf{%s}", status.Data()));
    tInfo->SetTextColor(textColor);
    tInfo->SetTextSize(0.06);
    tInfo->SetTextFont(43);
    tInfo->SetNDC();
    h->GetListOfFunctions()->Add(tInfo->Clone());
    if (ShifterInfoText[mo->getName()] != "")
      h->GetListOfFunctions()->Add(tShifterInfo->Clone());
  }
  vListErrorIdBad.clear();
  vListErrorIdMedium.clear();
}

} // namespace o2::quality_control_modules::its
