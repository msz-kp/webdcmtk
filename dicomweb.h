#pragma once

#include "drogon/HttpAppFramework.h"

#include "dimse.h"
#include "mime.h"
#include "misc.h"

const std::unordered_map<std::string, std::string> TagNamesTranslations{
    {"StudyDate", "00080020"},
    {"StudyTime", "00080030"},
    {"AccessionNumber", "00080050"},
    {"ModalitiesInStudy", "00080061"},
    {"ReferringPhysicianName", "00080090"},
    {"PatientName", "00100010"},
    {"PatientID", "00100020"},
    {"StudyInstanceUID", "0020000D"},
    {"StudyID", "00200010"},
    {"Modality", "00080060"},
    {"SeriesInstanceUID", "0020000E"},
    {"SeriesNumber", "00200011"},
    {"PerformedProcedureStepStartDate", "00400244"},
    {"Performed ProcedureStepStartTime", "00400245"},
    {"RequestAttributesSequence", "00400275"},
    {"ScheduledProcedureStepID", "00400009"},
    {"RequestedProcedureID", "00401001"},
    {"SOPClassUID", "00080016"},
    {"SOPInstanceUID", "00080018"},
    {"InstanceNumber", "00200013"},
    {"StudyDescription", "00081030"},
};

const std::unordered_set<std::string> STUDYReturnedAttributes = {
    "00080020",
    "00080030",
    "00080050",
    //"00080056", //Instance Availability
    "00080061",
    "00080090",
    //"00080201", //Timezone Offset From UTC
    //"00081190", //Retrieve URL Attribute
    "00100010",
    "00100020",
    "00100030",
    "00100040",
    "0020000D",
    "00200010",
    "00201206",
    "00201208"};

const std::unordered_set<std::string> SERIESReturnedAttributes{
    "00080060",
    //"00080201", //Timezone Offset From UTC
    "0008103E",
    //"00081190", //Retrieve URL
    "0020000E",
    "00200011",
    "00201209",
    "00400244",
    "00400245",
    "00400275",
    "00400009",
    "00401001"};

const std::unordered_set<std::string> INSTANCEReturnedAttributes = {
    "00080016",
    "00080018",
    //"00080056", //Instance Availability
    //"00080201", //Timezone Offset From UTC
    //"00081190", //Retrieve URL Attribute
    "00200013",
    "00280010",
    "00280011",
    "00280100",
    "00280008"};

const std::unordered_set<std::string> METADATAReturnedAttributes = {
    "00080008",
    "00080016",
    "00080018",
    "00080020",
    "00080021",
    "00080022",
    "00080023",
    "00080030",
    "00080031",
    "00080032",
    "00080033",
    "00080050",
    "00080060",
    "00080060",
    "00080061",
    "00080070",
    "00080080",
    "00080090",
    "00081010",
    "00081030",
    "0008103E",
    "00081090",
    "00100010",
    "00100020",
    "00100030",
    "00100040",
    "00101010",
    "00101030",
    "00180010",
    "00180015",
    "00180086",
    "00181030",
    "00181250",
    "00185100",
    "0020000D",
    "0020000E",
    "00200010",
    "00200011",
    "00200012",
    "00200013",
    "00200032",
    "00200037",
    "00200052",
    "00201041",
    "00280002",
    "00280004",
    "00280008",
    "00280010",
    "00280011",
    "00280030",
    "00280100",
    "00280101",
    "00280102",
    "00280103",
    "00540400",
};

bool stringCaseCmp(const std::string &str1, const std::string &str2);
bool isValidDicomTag(std::string s);
drogon::HttpResponsePtr jsonErrResposne(drogon::HttpStatusCode statusCode, std::string message);

class DicomWebCFindToJson {
public:
  std::string level;
  std::unordered_set<std::string> includefield;
  std::unordered_map<std::string, std::string> queryTags;

  DicomWebCFindToJson(const drogon::SafeStringMap<std::string> &requestParameters);
  void Perform(std::function<void(const drogon::HttpResponsePtr &)> &callback);
private:
  drogon::SafeStringMap<std::string> requestParameters;
  std::stringstream response_stream;
  int offset = 0;
  int limit = 0;

  void parseParams();
  bool performCFind();
};

class DicomWebCGetToResposne {
public:
  std::unordered_map<std::string, std::string> queryTags;

  DicomWebCGetToResposne(const drogon::SafeStringMap<std::string> &requestParameters);
  void PerformOne(std::function<void(const drogon::HttpResponsePtr &)> &callback, headerAccept &haccept, int frameNo = 1, unsigned long resizeWidth = 0, unsigned long resizeHeight = 0);
  void Perform(std::function<void(const drogon::HttpResponsePtr &)> &callback);
private:
  EasyDcmGet scu;
  std::stringstream response_stream;
  int offset = 0;
  int limit = 0;

  bool performCGet();
};

class STOWMimeParser : public multipart::Consumer {
public:
  std::shared_ptr<EasyDcmStore> scu;

  STOWMimeParser(std::shared_ptr<EasyDcmStore> scu, const std::string &boundary);
  ~STOWMimeParser();
  void receiveHeader(const std::string &name, const std::string &value);
  void receiveData(const char *at, size_t length);
  void receiveDataEnd();
private:
  std::stringstream stream;
  bool shouldAdd = false;
};

class DicomStowResposne {
public:
  void Perform(std::function<void(const drogon::HttpResponsePtr &)> &callback, std::string boundary, const char *body, size_t bodyLen);
};
