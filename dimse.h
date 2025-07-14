#pragma once

#include "dcmtk/dcmdata/dcjson.h"
#include "dcmtk/dcmdata/dcostrmb.h"
#include "dcmtk/dcmnet/scu.h"

struct CustomJsonFormat : DcmJsonFormatPretty {
  virtual OFBool asBulkDataURI(const DcmTagKey &tag, OFString &uri) {
    if (strcmp(DcmTag(tag).getVR().getVRName(), "px") == 0) {
      uri = "<<BINARY_DATA>>";
      return OFTrue;
    }

    return OFFalse;
  }
};

std::string toJSON(DcmItem *di);
void stripPrivateTags(DcmItem *item);

class DcmFileFormatBuffer {
public:
  Uint32 size;
  void *buffer;
  DcmFileFormatBuffer(std::shared_ptr<DcmFileFormat> file);
  ~DcmFileFormatBuffer();
};

class EasyDcm : public DcmSCU {
private:
public:
  EasyDcm();
  OFCondition connect();
  void disconnect(DcmCloseAssociationType ct);
  void send();
};

class EasyDcmFind : public EasyDcm {
private:
  int responseNo = 1;
  int responseLimit = 0;
  bool shouldAbortAssoc = false;
  std::list<QRResponse*> cfindResponse;
public:
  std::list<DcmDataset*> response;

  ~EasyDcmFind();
  void disconnect();
  OFCondition find(DcmDataset &query, int limit = 0);
  OFCondition handleFINDResponse(const T_ASC_PresentationContextID, QRResponse *response, OFBool &waitForNextResponse);
};

class EasyDcmGet : public EasyDcm {
private:
public:
  std::list<std::shared_ptr<DcmFileFormat>> response;

  void disconnect();
  OFCondition get(DcmDataset &query);
  OFCondition handleSTORERequest(const T_ASC_PresentationContextID, DcmDataset *incomingObject,
				 OFBool &, Uint16 &cStoreReturnStatus);
};

class EasyDcmStoreData
{
public:
  std::shared_ptr<DcmFileFormat> dcm;
  OFString sopInstanceUID;
  OFString mediaStorageSOPClassUID;
  OFString transferSyntaxUID;
  bool sent;
  OFString error;

  EasyDcmStoreData(std::shared_ptr<DcmFileFormat> dataset, OFString sopUID, OFString ms="", OFString tr="", bool s=false, OFString e="");
};

class EasyDcmStore : public DcmSCU {
private:
  std::map<OFString, OFList<OFString>> presentationContexts;
  OFList<EasyDcmStoreData> files;
public:
  EasyDcmStore();
  OFCondition addFile(std::shared_ptr<DcmFileFormat> dataset);
  OFCondition connect();
  void disconnect();
  OFCondition echo();
  void send();
  const OFList<EasyDcmStoreData>& getFiles() { return files; }
  void cleanFiles() { files.clear(); }
};
