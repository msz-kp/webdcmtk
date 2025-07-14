#include <memory>
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmnet/dfindscu.h"
#include "dcmtk/dcmnet/diutil.h"
#include "drogon/HttpAppFramework.h"
#include "trantor/utils/Logger.h"

#include "dimse.h"


std::string toJSON(DcmItem *di) {
  std::stringstream sstream;
  CustomJsonFormat fmt;
  di->writeJsonExt(sstream, fmt, OFTrue, OFTrue);

  return sstream.str();
}

void stripPrivateTags(DcmItem *item) {
  std::vector<DcmTag> to_remove;
  DcmStack stack;
  DcmObject *object = NULL;

  while (item->nextObject(stack, true).good()) {
    object = stack.top();
    auto tag = object->getTag();
    if (tag.isPrivate())
      to_remove.push_back(tag);
  }

  for (auto &t : to_remove)
    item->findAndDeleteElement(t);
}



// DcmFileFormatBuffer
DcmFileFormatBuffer::DcmFileFormatBuffer(std::shared_ptr<DcmFileFormat> file) {
  auto ds = file->getDataset();
  if (!ds)
    throw std::invalid_argument("empty dataset");

  file->transferInit();

  size = file->calcElementLength(ds->getOriginalXfer(), EET_UndefinedLength);
  buffer = (char *)malloc(sizeof(char) * size);
  if (!buffer) {
    file->transferEnd();
    throw std::bad_alloc();
  }

  DcmOutputBufferStream outputStream(buffer, size);
  auto res = file->write(outputStream, ds->getOriginalXfer(), EET_UndefinedLength, NULL);
  if (res.bad()) {
    file->transferEnd();
    throw std::runtime_error("DcmOutputBufferStream write failed");
  }

  file->transferEnd();
};

DcmFileFormatBuffer::~DcmFileFormatBuffer() {
  free(buffer);
}



// EasyDcm
EasyDcm::EasyDcm() : DcmSCU() {
  auto &cfg = drogon::app().getCustomConfig();
  setAETitle(cfg["dimse"]["aet"].asString());
  setPeerHostName(cfg["dimse"]["peer_hostname"].asString());
  setPeerPort(cfg["dimse"]["peer_port"].asInt());
  setPeerAETitle(cfg["dimse"]["peer_aet"].asString());
  setDatasetConversionMode(true);

  OFList<OFString> ts;
  ts.push_back(UID_LittleEndianExplicitTransferSyntax);
  ts.push_back(UID_BigEndianExplicitTransferSyntax);
  ts.push_back(UID_LittleEndianImplicitTransferSyntax);

  addPresentationContext(UID_VerificationSOPClass, ts);
  addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, ts);
  addPresentationContext(UID_GETStudyRootQueryRetrieveInformationModel, ts);
  for (Uint16 j = 0; j < numberOfDcmLongSCUStorageSOPClassUIDs; j++) {
    addPresentationContext(dcmLongSCUStorageSOPClassUIDs[j], ts, ASC_SC_ROLE_SCP);
  }
}

OFCondition EasyDcm::connect() {
  OFCondition result = initNetwork();
  if (result.bad())
    return result;

  result = negotiateAssociation();
  if (result.bad())
    return result;

  return EC_Normal;
}

void EasyDcm::disconnect(DcmCloseAssociationType ct) {
  closeAssociation(ct);
}



// EasyDcmFind
EasyDcmFind::~EasyDcmFind() {
  for (auto &r : cfindResponse)
    delete r;
}

void EasyDcmFind::disconnect() {
  closeAssociation((shouldAbortAssoc) ? DCMSCU_ABORT_ASSOCIATION : DCMSCU_RELEASE_ASSOCIATION);
};

OFCondition EasyDcmFind::find(DcmDataset &query, int limit) {
  responseLimit = limit;
  T_ASC_PresentationContextID pcID = findAnyPresentationContextID(UID_FINDStudyRootQueryRetrieveInformationModel, UID_LittleEndianExplicitTransferSyntax);
  if (pcID == 0)
    return makeOFCondition(9999, 0, OF_error, "findAnyPresentationContextID error");

  auto res = sendFINDRequest(pcID, &query, &cfindResponse);
  if (res.bad())
    return res;

  for (auto &r : cfindResponse) {
    auto ds = r->m_dataset;
    if (ds != NULL) {
      ds->findAndDeleteElement(DCM_QueryRetrieveLevel);
      response.push_back(ds);
    }
  }

  return EC_Normal;
}

OFCondition EasyDcmFind::handleFINDResponse(const T_ASC_PresentationContextID, QRResponse *response, OFBool &waitForNextResponse) {
  waitForNextResponse = false;

  if (response == NULL)
    return DIMSE_NULLKEY;

  if (responseLimit != 0) {
    responseNo++;
    if (responseNo > responseLimit) {
      shouldAbortAssoc = true;
      return EC_Normal;
    }
  }

  OFString s;
  s = DU_cfindStatusString(response->m_status);
  return handleSessionResponseDefault(response->m_status, s, waitForNextResponse);
}



// EasyDcmGet
void EasyDcmGet::disconnect() {
  closeAssociation(DCMSCU_RELEASE_ASSOCIATION);
}

OFCondition EasyDcmGet::get(DcmDataset &query) {
    T_ASC_PresentationContextID pcID = findAnyPresentationContextID(UID_GETStudyRootQueryRetrieveInformationModel, UID_LittleEndianExplicitTransferSyntax);
  if (pcID == 0)
    return makeOFCondition(9999, 0, OF_error, "findAnyPresentationContextID error");

  auto res = sendCGETRequest(pcID, &query, NULL);
  if (res.bad())
    return res;

  return EC_Normal;
}

OFCondition EasyDcmGet::handleSTORERequest(const T_ASC_PresentationContextID, DcmDataset *incomingObject,
					   OFBool &, Uint16 &cStoreReturnStatus)
{
    if (incomingObject == NULL)
        return DIMSE_NULLKEY;

    OFString sopClassUID;
    OFString sopInstanceUID;
    OFCondition result = incomingObject->findAndGetOFString(DCM_SOPClassUID, sopClassUID);
    if (result.good())
      result = incomingObject->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUID);
    if (result.bad())
    {
        cStoreReturnStatus = STATUS_STORE_Error_DataSetDoesNotMatchSOPClass;
        delete incomingObject;
        return EC_TagNotFound;
    }

    //DcmFileFormat dcmff(incomingObject, OFFalse /* do not copy but take ownership */);
    std::shared_ptr<DcmFileFormat> dcmff;
    dcmff = std::make_shared<DcmFileFormat>(incomingObject, OFFalse /* do not copy but take ownership */);
    result = dcmff->validateMetaInfo(incomingObject->getOriginalXfer());
    if (result.bad())
      LOG_ERROR << "validateMetaInfo(): " << result.text();
    else
      response.push_back(dcmff);

    return EC_Normal;
}



//store
EasyDcmStoreData::EasyDcmStoreData(std::shared_ptr<DcmFileFormat> ff, OFString sopUID, OFString ms, OFString tr, bool s, OFString e)
  : dcm{ff}, sopInstanceUID{sopUID}, mediaStorageSOPClassUID{ms}, transferSyntaxUID{tr}, sent{s}, error{e} {}

EasyDcmStore::EasyDcmStore() : DcmSCU() {
  auto &cfg = drogon::app().getCustomConfig();
  setAETitle(cfg["dimse"]["aet"].asString());
  setPeerHostName(cfg["dimse"]["peer_hostname"].asString());
  setPeerPort(cfg["dimse"]["peer_port"].asInt());
  setPeerAETitle(cfg["dimse"]["peer_aet"].asString());
  setDatasetConversionMode(true);


  OFList<OFString> ts;
  ts.push_back(UID_LittleEndianExplicitTransferSyntax);
  ts.push_back(UID_BigEndianExplicitTransferSyntax);
  ts.push_back(UID_LittleEndianImplicitTransferSyntax);
  addPresentationContext(UID_VerificationSOPClass, ts);
}

OFCondition EasyDcmStore::addFile(std::shared_ptr<DcmFileFormat> ff) {
  OFString sopinstanceuid;
  OFString mediastoragesopclassuid;
  OFString transfersyntaxuid;
  OFCondition result;

  result = ff->getDataset()->findAndGetOFString(DCM_SOPInstanceUID, sopinstanceuid);
  if (result.bad())
    return result;

  result = ff->getMetaInfo()->findAndGetOFString(DCM_MediaStorageSOPClassUID, mediastoragesopclassuid);
  if (result.bad())
    return result;

  result = ff->getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, transfersyntaxuid);
  if (result.bad())
    return result;

  files.push_back(EasyDcmStoreData(ff, sopinstanceuid, mediastoragesopclassuid, transfersyntaxuid, false, ""));
  auto found = std::find(presentationContexts[mediastoragesopclassuid].begin(),
                         presentationContexts[mediastoragesopclassuid].end(),
                         transfersyntaxuid);
  if (found == presentationContexts[mediastoragesopclassuid].end())
    presentationContexts[mediastoragesopclassuid].push_back(transfersyntaxuid);

  return makeOFCondition(9999, 0, OF_ok, "");
}

OFCondition EasyDcmStore::connect() {
  for (auto const &[key, val] : presentationContexts) {
    for (auto const &v : val) {
      std::list<OFString> xs {v};
      addPresentationContext(key, xs);
    }
    if  (std::find(val.begin(), val.end(), UID_LittleEndianExplicitTransferSyntax) == val.end())
      {
	std::list<OFString> xs_fallback {UID_LittleEndianExplicitTransferSyntax};
	addPresentationContext(key, xs_fallback);
      }
  }

  /* Initialize network */
  OFCondition result = initNetwork();
  if (result.bad()) {
    //OFLOG_WARN(customDCMTKLogger, "Unable to set up the network: " << result.text());
    return result;
  }

  /* Negotiate Association */
  result = negotiateAssociation();
  if (result.bad()) {
    //OFLOG_WARN(customDCMTKLogger, "Unable to negotiate association: " << result.text());
    return result;
  }

  return makeOFCondition(9999, 0, OF_ok, "");
}

void EasyDcmStore::disconnect() {
  presentationContexts.clear();
  releaseAssociation();
}

OFCondition EasyDcmStore::echo() {
  return sendECHORequest(0);
}

void EasyDcmStore::send() {
  OFCondition result;
  Uint16 rspStatusCode;

  if (!isConnected()) {
    //OFLOG_WARN(customDCMTKLogger, "Not connected!");
    return;
  }

  for (auto &file : files) {
    T_ASC_PresentationContextID pcID = findAnyPresentationContextID(file.mediaStorageSOPClassUID, file.transferSyntaxUID);
    if (pcID == 0) {
      //OFLOG_WARN(customDCMTKLogger, "There is no accepted presentation context for C-STORE");
      file.error = "There is no accepted presentation context for C-STORE";
      continue;
    }

    result = sendSTORERequest(pcID, NULL, file.dcm->getDataset(), rspStatusCode);
    if (result.bad()) {
      //OFLOG_WARN(customDCMTKLogger, "Unable to store: " << result.text());
      file.error = result.text();
      continue;
    }
    if (rspStatusCode != 0) {
      //OFLOG_WARN(customDCMTKLogger, "Unable to store, StatusCode: " << rspStatusCode);
      file.error = "Unable to store, StatusCode: " + std::to_string(rspStatusCode);
      continue;
    }

    file.sent = true;
  }
}
